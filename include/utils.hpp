#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <array>
#include <atomic>        // ADD THIS
#include <vector>        // ADD THIS
#include <cstring>

namespace lob {

// High-resolution timestamp
inline uint64_t get_timestamp_ns() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// CPU timing
inline uint64_t rdtsc() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Memory barriers
inline void memory_barrier() noexcept {
    __asm__ __volatile__("mfence" ::: "memory");
}

inline void compiler_barrier() noexcept {
    __asm__ __volatile__("" ::: "memory");
}

// CPU affinity
void set_cpu_affinity(int cpu);
void set_numa_node(int node);

// Huge pages
void* allocate_huge_pages(size_t size);
void deallocate_huge_pages(void* ptr, size_t size);

// Lock-free ring buffer logger
template<size_t Capacity>
class RingLogger {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
private:
    struct LogEntry {
        uint64_t timestamp;
        char message[120];
    };
    
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::array<LogEntry, Capacity> buffer_;
    
public:
    void log(const char* msg) noexcept {
        const size_t idx = head_.fetch_add(1, std::memory_order_relaxed) & (Capacity - 1);
        buffer_[idx].timestamp = get_timestamp_ns();
        std::strncpy(buffer_[idx].message, msg, sizeof(LogEntry::message) - 1);
        buffer_[idx].message[sizeof(LogEntry::message) - 1] = '\0';
    }
    
    void dump_to_file(const std::string& filename) const;
};

// Statistics
struct LatencyStats {
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t mean_ns;
    uint64_t p50_ns;
    uint64_t p95_ns;
    uint64_t p99_ns;
    uint64_t p999_ns;
    uint64_t count;
};

LatencyStats calculate_latency_stats(const std::vector<uint64_t>& latencies);

// Pretty printing
std::string format_price(uint32_t price_ticks);
std::string format_quantity(uint32_t quantity);
std::string format_duration(uint64_t nanoseconds);

} // namespace lob
