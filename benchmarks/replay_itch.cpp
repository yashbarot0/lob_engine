#include "matching_engine.hpp"
#include "feed_handler.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace lob;

struct BenchmarkResults {
    uint64_t total_messages;
    uint64_t total_orders;
    uint64_t total_matches;
    uint64_t elapsed_ns;
    double messages_per_sec;
    std::vector<uint64_t> order_latencies;
};

BenchmarkResults run_itch_benchmark(const std::string& filename, int cpu_core) {
    BenchmarkResults results{};
    
    EngineConfig config;
    config.order_pool_size = 10000000;
    config.cpu_affinity = cpu_core;
    config.enable_logging = false;
    
    MatchingEngine engine(config);
    engine.start();
    
    FeedHandler feed_handler(engine);
    
    uint64_t start_time = get_timestamp_ns();
    
    feed_handler.replay_itch_file(filename);
    
    uint64_t end_time = get_timestamp_ns();
    
    results.total_messages = feed_handler.get_messages_processed();
    results.total_orders = engine.get_total_orders();
    results.total_matches = engine.get_total_matches();
    results.elapsed_ns = end_time - start_time;
    results.messages_per_sec = (results.total_messages * 1e9) / results.elapsed_ns;
    
    return results;
}

void print_results(const BenchmarkResults& results) {
    std::cout << "\n=== ITCH Replay Benchmark Results ===" << std::endl;
    std::cout << "Total Messages: " << results.total_messages << std::endl;
    std::cout << "Total Orders: " << results.total_orders << std::endl;
    std::cout << "Total Matches: " << results.total_matches << std::endl;
    std::cout << "Elapsed Time: " << format_duration(results.elapsed_ns) << std::endl;
    std::cout << "Throughput: " << (results.messages_per_sec / 1e6) 
              << " million msg/sec" << std::endl;
    
    if (!results.order_latencies.empty()) {
        LatencyStats stats = calculate_latency_stats(results.order_latencies);
        
        std::cout << "\nOrder Processing Latency:" << std::endl;
        std::cout << "  Min: " << format_duration(stats.min_ns) << std::endl;
        std::cout << "  Mean: " << format_duration(stats.mean_ns) << std::endl;
        std::cout << "  P50: " << format_duration(stats.p50_ns) << std::endl;
        std::cout << "  P95: " << format_duration(stats.p95_ns) << std::endl;
        std::cout << "  P99: " << format_duration(stats.p99_ns) << std::endl;
        std::cout << "  P99.9: " << format_duration(stats.p999_ns) << std::endl;
        std::cout << "  Max: " << format_duration(stats.max_ns) << std::endl;
    }
    
    std::cout << "====================================\n" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <itch_file> [cpu_core]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    int cpu_core = (argc > 2) ? std::atoi(argv[2]) : 0;
    
    std::cout << "ITCH Market Data Replay Benchmark" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "File: " << filename << std::endl;
    std::cout << "CPU Core: " << cpu_core << std::endl;
    std::cout << "\n";
    
    BenchmarkResults results = run_itch_benchmark(filename, cpu_core);
    print_results(results);
    
    // Performance validation
    if (results.messages_per_sec > 100e6) {
        std::cout << "✓ Achieved >100M messages/sec target!" << std::endl;
    } else {
        std::cout << "✗ Did not achieve 100M messages/sec target" << std::endl;
    }
    
    return 0;
}
