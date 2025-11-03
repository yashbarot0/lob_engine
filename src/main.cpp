#include "matching_engine.hpp"
#include "feed_handler.hpp"
#include "utils.hpp"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace lob;

void print_book_state(const OrderBook* book) {
    if (!book) return;
    
    std::cout << "\n=== Order Book State ===" << std::endl;
    
    auto* best_ask = book->get_best_ask();
    auto* best_bid = book->get_best_bid();
    
    if (best_ask) {
        std::cout << "Best Ask: " << format_price(best_ask->price) 
                  << " (" << best_ask->total_volume << " shares, "
                  << best_ask->order_count << " orders)" << std::endl;
    }
    
    if (best_bid) {
        std::cout << "Best Bid: " << format_price(best_bid->price)
                  << " (" << best_bid->total_volume << " shares, "
                  << best_bid->order_count << " orders)" << std::endl;
    }
    
    if (best_ask && best_bid) {
        std::cout << "Spread: " << format_price(book->get_spread()) << std::endl;
    }
    
    std::cout << "Total Orders: " << book->get_order_count() << std::endl;
    std::cout << "Total Matches: " << book->get_match_count() << std::endl;
    std::cout << "========================\n" << std::endl;
}

void run_synthetic_benchmark() {
    std::cout << "\n=== Running Synthetic Benchmark ===" << std::endl;
    
    EngineConfig config;
    config.num_symbols = 10;
    config.order_pool_size = 1000000;
    config.enable_logging = false;
    config.cpu_affinity = 0;
    
    // ALLOCATE ON HEAP instead of stack
    auto engine = std::make_unique<MatchingEngine>(config);
    engine->start();
    
    const char* symbol = "AAPL";
    const size_t num_orders = 1000000;
    
    // Allocate latencies on heap too
    auto latencies = std::make_unique<std::vector<uint64_t>>();
    latencies->reserve(num_orders);
    
    std::cout << "Submitting " << num_orders << " orders..." << std::endl;
    
    uint64_t start_time = get_timestamp_ns();
    
    for (size_t i = 0; i < num_orders; ++i) {
        uint64_t order_start = rdtsc();
        
        // Alternate buy and sell orders
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        uint32_t base_price = 1000000; // $100.00
        uint32_t price = base_price + (i % 100) * 100; // Price variation
        uint32_t quantity = 100 + (i % 900);
        
        engine->submit_order(symbol, i, get_timestamp_ns(), 
                          price, quantity, side, OrderType::LIMIT);
        
        uint64_t order_end = rdtsc();
        latencies->push_back(order_end - order_start);
    }
    
    uint64_t end_time = get_timestamp_ns();
    uint64_t elapsed_ns = end_time - start_time;
    
    // Calculate statistics
    double orders_per_sec = (num_orders * 1e9) / elapsed_ns;
    LatencyStats stats = calculate_latency_stats(*latencies);
    
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total Orders: " << num_orders << std::endl;
    std::cout << "Elapsed Time: " << format_duration(elapsed_ns) << std::endl;
    std::cout << "Throughput: " << (orders_per_sec / 1e6) << " million orders/sec" << std::endl;
    std::cout << "\nLatency Statistics (cycles):" << std::endl;
    std::cout << "  Min: " << stats.min_ns << std::endl;
    std::cout << "  Mean: " << stats.mean_ns << std::endl;
    std::cout << "  P50: " << stats.p50_ns << std::endl;
    std::cout << "  P95: " << stats.p95_ns << std::endl;
    std::cout << "  P99: " << stats.p99_ns << std::endl;
    std::cout << "  P99.9: " << stats.p999_ns << std::endl;
    std::cout << "  Max: " << stats.max_ns << std::endl;
    std::cout << "========================\n" << std::endl;
    
    // Print book state
    print_book_state(engine->get_book(symbol));
    
    // Process execution reports
    size_t report_count = 0;
    ExecutionReport report;
    while (engine->get_execution_queue().pop(report)) {
        ++report_count;
    }
    
    std::cout << "Total Execution Reports: " << report_count << std::endl;
    std::cout << "Total Matches: " << engine->get_total_matches() << std::endl;
}


int main(int argc, char** argv) {
    std::cout << "Ultra-Low-Latency Limit Order Book & Matching Engine" << std::endl;
    std::cout << "====================================================\n" << std::endl;
    
    if (argc > 1) {
        std::string filename = argv[1];
        std::cout << "Replaying ITCH file: " << filename << std::endl;
        
        EngineConfig config;
        config.cpu_affinity = 0;
        
        MatchingEngine engine(config);
        engine.start();
        
        FeedHandler feed_handler(engine);
        feed_handler.replay_itch_file(filename);
        
        std::cout << "\nEngine Statistics:" << std::endl;
        std::cout << "  Total Orders: " << engine.get_total_orders() << std::endl;
        std::cout << "  Total Matches: " << engine.get_total_matches() << std::endl;
    } else {
        std::cout << "No ITCH file provided, running synthetic benchmark\n" << std::endl;
        run_synthetic_benchmark();
    }
    
    return 0;
}
