#pragma once

#include <string>        // MOVE THIS FIRST
#include <unordered_map> // THEN THIS

#include "order_book.hpp"
#include "order.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace lob {

// Matching engine configuration
struct EngineConfig {
    size_t num_symbols = 100;
    size_t order_pool_size = 1000000;
    bool enable_logging = false;
    int cpu_affinity = -1; // -1 = no affinity
    int numa_node = -1;    // -1 = no preference
};

// Main matching engine
class MatchingEngine {
public:
    explicit MatchingEngine(const EngineConfig& config);
    ~MatchingEngine();
    
    // Order submission
    void submit_order(const char* symbol, uint64_t order_id, uint64_t timestamp,
                     uint32_t price, uint32_t quantity, Side side, OrderType type);
    
    void cancel_order(const char* symbol, uint64_t order_id);
    void modify_order(const char* symbol, uint64_t order_id, uint32_t new_quantity);
    
    // Book access
    OrderBook* get_book(const char* symbol);
    
    // Execution reports
    SPSCQueue<ExecutionReport, 65536>& get_execution_queue() { return execution_queue_; }
    
    // Statistics
    uint64_t get_total_orders() const noexcept { return total_orders_.load(); }
    uint64_t get_total_matches() const noexcept { return total_matches_.load(); }
    
    // Control
    void start();
    void stop();
    bool is_running() const noexcept { return running_.load(); }
    
private:
    EngineConfig config_;
    
    // Order books per symbol
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    
    // Order object pool (NUMA-aware allocation)
    std::vector<Order*> order_pool_;
    std::atomic<size_t> pool_index_;
    
    // Execution queue
    SPSCQueue<ExecutionReport, 65536> execution_queue_;
    
    // Statistics
    std::atomic<uint64_t> total_orders_;
    std::atomic<uint64_t> total_matches_;
    
    // Threading
    std::atomic<bool> running_;
    
    // Helpers
    Order* allocate_order();
    void deallocate_order(Order* order);
    void setup_numa_affinity();
    void setup_cpu_affinity();
};

} // namespace lob
