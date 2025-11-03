#pragma once

#include "order.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstring>

namespace lob {

// Price level - doubly linked list of orders at same price
class alignas(CACHE_LINE_SIZE) PriceLevel {
public:
    uint32_t price;
    uint32_t total_volume;
    uint32_t order_count;
    
    // Doubly-linked list of orders (FIFO queue)
    Order* head_order;
    Order* tail_order;
    
    // Binary tree pointers for price levels
    PriceLevel* parent;
    PriceLevel* left;
    PriceLevel* right;
    
    explicit PriceLevel(uint32_t p) noexcept
        : price(p), total_volume(0), order_count(0),
          head_order(nullptr), tail_order(nullptr),
          parent(nullptr), left(nullptr), right(nullptr) {}
    
    void add_order(Order* order) noexcept;
    void remove_order(Order* order) noexcept;
};

// Main order book class
class OrderBook {
public:
    OrderBook();
    ~OrderBook();
    
    // Core operations
    void add_order(Order* order);
    void cancel_order(uint64_t order_id);
    void modify_order(uint64_t order_id, uint32_t new_quantity);
    
    // Matching
    std::vector<ExecutionReport> match_order(Order* order);
    
    // Book state
    PriceLevel* get_best_bid() const noexcept { return best_bid_; }
    PriceLevel* get_best_ask() const noexcept { return best_ask_; }
    
    uint32_t get_spread() const noexcept;
    uint64_t get_total_bid_volume() const noexcept;
    uint64_t get_total_ask_volume() const noexcept;
    
    // Stats
    uint64_t get_order_count() const noexcept { return order_count_; }
    uint64_t get_match_count() const noexcept { return match_count_; }
    
private:
    // Binary search trees for price levels
    PriceLevel* bid_tree_root_;
    PriceLevel* ask_tree_root_;
    
    // Best bid/ask pointers
    PriceLevel* best_bid_;
    PriceLevel* best_ask_;
    
    // Order lookup
    std::unordered_map<uint64_t, Order*> orders_;
    
    // Price level pool (pre-allocated)
    std::vector<std::unique_ptr<PriceLevel>> price_level_pool_;
    size_t pool_index_;
    
    // Statistics
    std::atomic<uint64_t> order_count_;
    std::atomic<uint64_t> match_count_;
    
    // Helper methods
    PriceLevel* find_or_create_level(uint32_t price, Side side);
    PriceLevel* find_level(uint32_t price, PriceLevel* root);
    PriceLevel* insert_level(uint32_t price, PriceLevel*& root);
    void remove_level(PriceLevel* level, PriceLevel*& root);
    void update_best_bid();
    void update_best_ask();
    
    // Matching helpers
    ExecutionReport execute_trade(Order* aggressive, Order* passive, 
                                  uint32_t quantity, uint64_t match_id);
};

} // namespace lob
