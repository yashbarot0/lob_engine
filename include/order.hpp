#pragma once

#include <cstdint>
#include <cstddef> 
#include <atomic>
#include <array>

namespace lob {

// Cache line size for alignment
constexpr size_t CACHE_LINE_SIZE = 64;

// Order side
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Order type
enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    CANCEL = 2
};

// Order structure - intrusive doubly-linked list node
struct alignas(CACHE_LINE_SIZE) Order {
    uint64_t order_id;
    uint64_t timestamp;  // nanoseconds since epoch
    uint32_t price;      // Price in ticks (fixed-point)
    uint32_t quantity;
    uint32_t remaining_quantity;
    Side side;
    OrderType type;
    
    // Intrusive list pointers
    Order* next;
    Order* prev;
    
    // Parent price level pointer
    class PriceLevel* parent_level;
    
    Order() noexcept 
        : order_id(0), timestamp(0), price(0), quantity(0),
          remaining_quantity(0), side(Side::BUY), type(OrderType::LIMIT),
          next(nullptr), prev(nullptr), parent_level(nullptr) {}
    
    Order(uint64_t id, uint64_t ts, uint32_t p, uint32_t qty, Side s, OrderType t) noexcept
        : order_id(id), timestamp(ts), price(p), quantity(qty),
          remaining_quantity(qty), side(s), type(t),
          next(nullptr), prev(nullptr), parent_level(nullptr) {}
};

// Execution report
struct alignas(CACHE_LINE_SIZE) ExecutionReport {
    uint64_t order_id;
    uint64_t match_id;
    uint64_t timestamp;
    uint32_t price;
    uint32_t executed_quantity;
    Side side;
    bool is_full_fill;
    
    ExecutionReport() noexcept = default;
    
    ExecutionReport(uint64_t oid, uint64_t mid, uint64_t ts, 
                   uint32_t p, uint32_t qty, Side s, bool full) noexcept
        : order_id(oid), match_id(mid), timestamp(ts), price(p),
          executed_quantity(qty), side(s), is_full_fill(full) {}
};

// Lock-free SPSC queue for execution reports
template<typename T, size_t Capacity>
class alignas(CACHE_LINE_SIZE) SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_;
    
public:
    SPSCQueue() noexcept = default;
    
    // Disable copy and move
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    
    bool push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Capacity - 1);
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }
        
        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer_[tail];
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == 
               head_.load(std::memory_order_acquire);
    }
    
    size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head >= tail) ? (head - tail) : (Capacity - tail + head);
    }
};

} // namespace lob
