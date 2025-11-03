#include "matching_engine.hpp"
#include "utils.hpp"
#include <iostream>
#include <cstring>

#ifdef __linux__
#include <numa.h>
#endif

namespace lob {

MatchingEngine::MatchingEngine(const EngineConfig& config)
    : config_(config), pool_index_(0), 
      total_orders_(0), total_matches_(0), running_(false) {
    
    // Setup NUMA and CPU affinity
    if (config_.numa_node >= 0) {
        setup_numa_affinity();
    }
    
    if (config_.cpu_affinity >= 0) {
        setup_cpu_affinity();
    }
    
    // Pre-allocate order pool
    order_pool_.reserve(config_.order_pool_size);
    
#ifdef __linux__
    if (config_.numa_node >= 0) {
        // NUMA-aware allocation
        for (size_t i = 0; i < config_.order_pool_size; ++i) {
            void* mem = numa_alloc_onnode(sizeof(Order), config_.numa_node);
            order_pool_.push_back(new (mem) Order());
        }
    } else {
#endif
        for (size_t i = 0; i < config_.order_pool_size; ++i) {
            order_pool_.push_back(new Order());
        }
#ifdef __linux__
    }
#endif
    
    std::cout << "Matching engine initialized with " << config_.order_pool_size 
              << " order pool size" << std::endl;
}

MatchingEngine::~MatchingEngine() {
    stop();
    
    // Cleanup order pool
#ifdef __linux__
    if (config_.numa_node >= 0) {
        for (Order* order : order_pool_) {
            order->~Order();
            numa_free(order, sizeof(Order));
        }
    } else {
#endif
        for (Order* order : order_pool_) {
            delete order;
        }
#ifdef __linux__
    }
#endif
}


void MatchingEngine::submit_order(const char* symbol, uint64_t order_id,
                                  uint64_t timestamp, uint32_t price,
                                  uint32_t quantity, Side side, OrderType type) {
    // Get or create order book
    OrderBook* book = get_book(symbol);
    if (!book) {
        books_[symbol] = std::make_unique<OrderBook>();
        book = books_[symbol].get();
    }
    
    // Allocate order from pool
    Order* order = allocate_order();
    if (!order) {
        // Don't spam stderr on every failure
        static std::atomic<size_t> error_count{0};
        if (error_count.fetch_add(1) % 100000 == 0) {
            std::cerr << "ERROR: Order pool exhausted at order " << order_id 
                      << " (suppressing further messages)" << std::endl;
        }
        return;
    }
    
    order->order_id = order_id;
    order->timestamp = timestamp;
    order->price = price;
    order->quantity = quantity;
    order->remaining_quantity = quantity;
    order->side = side;
    order->type = type;
    
    // Match aggressive orders
    if (type == OrderType::MARKET || 
        (type == OrderType::LIMIT && 
         ((side == Side::BUY && book->get_best_ask() && price >= book->get_best_ask()->price) ||
          (side == Side::SELL && book->get_best_bid() && price <= book->get_best_bid()->price)))) {
        
        auto reports = book->match_order(order);
        
        // Push execution reports to queue
        for (const auto& report : reports) {
            if (!execution_queue_.push(report)) {
                std::cerr << "WARNING: Execution queue full!" << std::endl;
                break;
            }
            ++total_matches_;
        }
    }
    
    // Add remaining quantity to book
    if (order->remaining_quantity > 0 && type == OrderType::LIMIT) {
        book->add_order(order);
    } else {
        deallocate_order(order);
    }
    
    ++total_orders_;
}


void MatchingEngine::cancel_order(const char* symbol, uint64_t order_id) {
    OrderBook* book = get_book(symbol);
    if (book) {
        book->cancel_order(order_id);
    }
}

void MatchingEngine::modify_order(const char* symbol, uint64_t order_id, 
                                  uint32_t new_quantity) {
    OrderBook* book = get_book(symbol);
    if (book) {
        book->modify_order(order_id, new_quantity);
    }
}

OrderBook* MatchingEngine::get_book(const char* symbol) {
    auto it = books_.find(symbol);
    return (it != books_.end()) ? it->second.get() : nullptr;
}

void MatchingEngine::start() {
    running_.store(true, std::memory_order_release);
}

void MatchingEngine::stop() {
    running_.store(false, std::memory_order_release);
}

Order* MatchingEngine::allocate_order() {
    size_t idx = pool_index_.fetch_add(1, std::memory_order_relaxed);
    
    // CRITICAL: Check bounds
    if (idx >= config_.order_pool_size) {
        std::cerr << "ERROR: Order pool exhausted at index " << idx << std::endl;
        return nullptr;
    }
    
    return order_pool_[idx];
}

void MatchingEngine::deallocate_order(Order* order) {
    // Simple pool management - could be improved
    (void)order;
}

void MatchingEngine::setup_numa_affinity() {
#ifdef __linux__
    if (numa_available() >= 0) {
        numa_run_on_node(config_.numa_node);
        std::cout << "Set NUMA node affinity to " << config_.numa_node << std::endl;
    }
#endif
}

void MatchingEngine::setup_cpu_affinity() {
    set_cpu_affinity(config_.cpu_affinity);
    std::cout << "Set CPU affinity to core " << config_.cpu_affinity << std::endl;
}

} // namespace lob
