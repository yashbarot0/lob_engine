#include "order_book.hpp"
#include <algorithm>
#include <cassert>

namespace lob {

// PriceLevel methods
void PriceLevel::add_order(Order* order) noexcept {
    assert(order != nullptr);
    assert(order->price == price);
    
    order->parent_level = this;
    order->next = nullptr;
    order->prev = tail_order;
    
    if (tail_order) {
        tail_order->next = order;
    } else {
        head_order = order;
    }
    
    tail_order = order;
    total_volume += order->remaining_quantity;
    ++order_count;
}

void PriceLevel::remove_order(Order* order) noexcept {
    assert(order != nullptr);
    assert(order->parent_level == this);
    
    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head_order = order->next;
    }
    
    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail_order = order->prev;
    }
    
    total_volume -= order->remaining_quantity;
    --order_count;
    
    order->parent_level = nullptr;
    order->next = nullptr;
    order->prev = nullptr;
}

// OrderBook implementation
OrderBook::OrderBook()
    : bid_tree_root_(nullptr), ask_tree_root_(nullptr),
      best_bid_(nullptr), best_ask_(nullptr),
      pool_index_(0), order_count_(0), match_count_(0) {
    
    // Pre-allocate price level pool
    price_level_pool_.reserve(10000);
    for (size_t i = 0; i < 10000; ++i) {
        price_level_pool_.emplace_back(std::make_unique<PriceLevel>(0));
    }
}

OrderBook::~OrderBook() {
    orders_.clear();
}

void OrderBook::add_order(Order* order) {
    assert(order != nullptr);
    
    // Find or create price level
    PriceLevel* level = find_or_create_level(order->price, order->side);
    level->add_order(order);
    
    // Update order lookup
    orders_[order->order_id] = order;
    
    // Update best bid/ask
    if (order->side == Side::BUY) {
        if (!best_bid_ || level->price > best_bid_->price) {
            best_bid_ = level;
        }
    } else {
        if (!best_ask_ || level->price < best_ask_->price) {
            best_ask_ = level;
        }
    }
    
    ++order_count_;
}

void OrderBook::cancel_order(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;
    
    Order* order = it->second;
    PriceLevel* level = order->parent_level;
    
    level->remove_order(order);
    
    // Remove empty price level
    if (level->order_count == 0) {
        if (order->side == Side::BUY) {
            remove_level(level, bid_tree_root_);
            if (level == best_bid_) {
                update_best_bid();
            }
        } else {
            remove_level(level, ask_tree_root_);
            if (level == best_ask_) {
                update_best_ask();
            }
        }
    }
    
    orders_.erase(it);
    --order_count_;
}

void OrderBook::modify_order(uint64_t order_id, uint32_t new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;
    
    Order* order = it->second;
    PriceLevel* level = order->parent_level;
    
    level->total_volume -= order->remaining_quantity;
    order->remaining_quantity = new_quantity;
    level->total_volume += new_quantity;
}

std::vector<ExecutionReport> OrderBook::match_order(Order* order) {
    std::vector<ExecutionReport> reports;
    
    if (order->type != OrderType::LIMIT && order->type != OrderType::MARKET) {
        return reports;
    }
    
    PriceLevel* contra_level = (order->side == Side::BUY) ? best_ask_ : best_bid_;
    
    while (order->remaining_quantity > 0 && contra_level) {
        // Check price crossing
        if (order->type == OrderType::LIMIT) {
            if (order->side == Side::BUY && order->price < contra_level->price) break;
            if (order->side == Side::SELL && order->price > contra_level->price) break;
        }
        
        Order* passive = contra_level->head_order;
        while (passive && order->remaining_quantity > 0) {
            uint32_t match_qty = std::min(order->remaining_quantity, 
                                         passive->remaining_quantity);
            
            // Generate execution report
            uint64_t match_id = ++match_count_;
            reports.push_back(execute_trade(order, passive, match_qty, match_id));
            
            // Update quantities
            order->remaining_quantity -= match_qty;
            passive->remaining_quantity -= match_qty;
            contra_level->total_volume -= match_qty;
            
            Order* next_passive = passive->next;
            
            // Remove fully filled passive order
            if (passive->remaining_quantity == 0) {
                contra_level->remove_order(passive);
                orders_.erase(passive->order_id);
                --order_count_;
            }
            
            passive = next_passive;
        }
        
        // Move to next price level if current is depleted
        if (contra_level->order_count == 0) {
            PriceLevel* next_level = (order->side == Side::BUY) 
                ? contra_level->right  // Next higher ask
                : contra_level->left;  // Next lower bid
            
            if (order->side == Side::BUY) {
                remove_level(contra_level, ask_tree_root_);
                best_ask_ = next_level;
            } else {
                remove_level(contra_level, bid_tree_root_);
                best_bid_ = next_level;
            }
            
            contra_level = next_level;
        } else {
            break;
        }
    }
    
    return reports;
}

ExecutionReport OrderBook::execute_trade(Order* aggressive, Order* passive,
                                        uint32_t quantity, uint64_t match_id) {
    return ExecutionReport(
        aggressive->order_id,
        match_id,
        aggressive->timestamp,
        passive->price, // Trade at passive price
        quantity,
        aggressive->side,
        aggressive->remaining_quantity == quantity
    );
}

PriceLevel* OrderBook::find_or_create_level(uint32_t price, Side side) {
    PriceLevel*& root = (side == Side::BUY) ? bid_tree_root_ : ask_tree_root_;
    PriceLevel* level = find_level(price, root);
    
    if (!level) {
        level = insert_level(price, root);
    }
    
    return level;
}

PriceLevel* OrderBook::find_level(uint32_t price, PriceLevel* root) {
    while (root) {
        if (price == root->price) return root;
        root = (price < root->price) ? root->left : root->right;
    }
    return nullptr;
}

PriceLevel* OrderBook::insert_level(uint32_t price, PriceLevel*& root) {
    assert(pool_index_ < price_level_pool_.size());
    
    PriceLevel* new_level = price_level_pool_[pool_index_++].get();
    new_level->price = price;
    new_level->total_volume = 0;
    new_level->order_count = 0;
    new_level->head_order = nullptr;
    new_level->tail_order = nullptr;
    new_level->parent = nullptr;
    new_level->left = nullptr;
    new_level->right = nullptr;
    
    if (!root) {
        root = new_level;
        return new_level;
    }
    
    PriceLevel* current = root;
    while (true) {
        if (price < current->price) {
            if (!current->left) {
                current->left = new_level;
                new_level->parent = current;
                return new_level;
            }
            current = current->left;
        } else {
            if (!current->right) {
                current->right = new_level;
                new_level->parent = current;
                return new_level;
            }
            current = current->right;
        }
    }
}

void OrderBook::remove_level(PriceLevel* level, PriceLevel*& root) {
    // Simple BST deletion (can be optimized with balanced tree)
    if (!level->left && !level->right) {
        if (level->parent) {
            if (level->parent->left == level) level->parent->left = nullptr;
            else level->parent->right = nullptr;
        } else {
            root = nullptr;
        }
    } else if (!level->left) {
        if (level->parent) {
            if (level->parent->left == level) level->parent->left = level->right;
            else level->parent->right = level->right;
        } else {
            root = level->right;
        }
        if (level->right) level->right->parent = level->parent;
    } else if (!level->right) {
        if (level->parent) {
            if (level->parent->left == level) level->parent->left = level->left;
            else level->parent->right = level->left;
        } else {
            root = level->left;
        }
        if (level->left) level->left->parent = level->parent;
    } else {
        // Find in-order successor
        PriceLevel* successor = level->right;
        while (successor->left) successor = successor->left;
        
        level->price = successor->price;
        level->total_volume = successor->total_volume;
        level->order_count = successor->order_count;
        level->head_order = successor->head_order;
        level->tail_order = successor->tail_order;
        
        remove_level(successor, root);
    }
}

void OrderBook::update_best_bid() {
    best_bid_ = bid_tree_root_;
    if (best_bid_) {
        while (best_bid_->right) best_bid_ = best_bid_->right;
    }
}

void OrderBook::update_best_ask() {
    best_ask_ = ask_tree_root_;
    if (best_ask_) {
        while (best_ask_->left) best_ask_ = best_ask_->left;
    }
}

uint32_t OrderBook::get_spread() const noexcept {
    if (!best_bid_ || !best_ask_) return 0;
    return best_ask_->price - best_bid_->price;
}

uint64_t OrderBook::get_total_bid_volume() const noexcept {
    uint64_t volume = 0;
    PriceLevel* level = bid_tree_root_;
    while (level) {
        volume += level->total_volume;
        level = level->right;
    }
    return volume;
}

uint64_t OrderBook::get_total_ask_volume() const noexcept {
    uint64_t volume = 0;
    PriceLevel* level = ask_tree_root_;
    while (level) {
        volume += level->total_volume;
        level = level->left;
    }
    return volume;
}

} // namespace lob
