#include "utils.hpp"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

#ifdef __linux__
#include <sched.h>
#include <numa.h>
#include <sys/mman.h>
#endif

namespace lob {

void set_cpu_affinity(int cpu) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    pthread_t thread = pthread_self();
    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        std::cerr << "Failed to set CPU affinity to core " << cpu << std::endl;
    }
#else
    (void)cpu;
    std::cerr << "CPU affinity not supported on this platform" << std::endl;
#endif
}

void set_numa_node(int node) {
#ifdef __linux__
    if (numa_available() >= 0) {
        numa_run_on_node(node);
    } else {
        std::cerr << "NUMA not available" << std::endl;
    }
#else
    (void)node;
    std::cerr << "NUMA not supported on this platform" << std::endl;
#endif
}

void* allocate_huge_pages(size_t size) {
#ifdef __linux__
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to allocate huge pages, falling back to regular pages" 
                  << std::endl;
        ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    
    return (ptr == MAP_FAILED) ? nullptr : ptr;
#else
    return malloc(size);
#endif
}

void deallocate_huge_pages(void* ptr, size_t size) {
#ifdef __linux__
    munmap(ptr, size);
#else
    (void)size;
    free(ptr);
#endif
}

LatencyStats calculate_latency_stats(const std::vector<uint64_t>& latencies) {
    LatencyStats stats{};
    
    if (latencies.empty()) {
        return stats;
    }
    
    std::vector<uint64_t> sorted = latencies;
    std::sort(sorted.begin(), sorted.end());
    
    stats.count = sorted.size();
    stats.min_ns = sorted.front();
    stats.max_ns = sorted.back();
    stats.mean_ns = std::accumulate(sorted.begin(), sorted.end(), 0ULL) / sorted.size();
    
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(p * sorted.size());
        return sorted[std::min(idx, sorted.size() - 1)];
    };
    
    stats.p50_ns = percentile(0.50);
    stats.p95_ns = percentile(0.95);
    stats.p99_ns = percentile(0.99);
    stats.p999_ns = percentile(0.999);
    
    return stats;
}

std::string format_price(uint32_t price_ticks) {
    // Assuming 4 decimal places
    double price = price_ticks / 10000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << price;
    return oss.str();
}

std::string format_quantity(uint32_t quantity) {
    std::ostringstream oss;
    oss << quantity;
    return oss.str();
}

std::string format_duration(uint64_t nanoseconds) {
    if (nanoseconds < 1000) {
        return std::to_string(nanoseconds) + " ns";
    } else if (nanoseconds < 1000000) {
        return std::to_string(nanoseconds / 1000.0) + " μs";
    } else if (nanoseconds < 1000000000) {
        return std::to_string(nanoseconds / 1000000.0) + " ms";
    } else {
        return std::to_string(nanoseconds / 1000000000.0) + " s";
    }
}

template<size_t Capacity>
void RingLogger<Capacity>::dump_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        return;
    }
    
    size_t count = std::min(head_.load(std::memory_order_acquire), Capacity);
    
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = buffer_[i];
        file << entry.timestamp << ": " << entry.message << "\n";
    }
    
    std::cout << "Dumped " << count << " log entries to " << filename << std::endl;
}

// Explicit template instantiation
template class RingLogger<65536>;

} // namespace lob
