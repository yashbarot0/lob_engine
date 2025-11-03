#include "feed_handler.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <arpa/inet.h>

namespace lob {

FeedHandler::FeedHandler(MatchingEngine& engine)
    : engine_(engine), running_(false), messages_processed_(0), last_timestamp_(0) {
}

FeedHandler::~FeedHandler() {
    stop_live_feed();
}

void FeedHandler::replay_itch_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ITCH file: " << filename << std::endl;
        return;
    }
    
    std::cout << "Replaying ITCH file: " << filename << std::endl;
    
    uint64_t start_time = get_timestamp_ns();
    uint64_t message_count = 0;
    
    while (file) {
        // Read message length (2 bytes)
        uint16_t msg_length;
        file.read(reinterpret_cast<char*>(&msg_length), sizeof(msg_length));
        if (!file) break;
        
        msg_length = ntohs(msg_length);
        
        // Read message type (1 byte)
        uint8_t msg_type;
        file.read(reinterpret_cast<char*>(&msg_type), sizeof(msg_type));
        if (!file) break;
        
        // Read message data
        std::vector<uint8_t> data(msg_length - 1);
        file.read(reinterpret_cast<char*>(data.data()), msg_length - 1);
        if (!file) break;
        
        // Process message
        process_message(msg_type, data.data(), msg_length - 1);
        
        ++message_count;
        
        if (message_count % 1000000 == 0) {
            uint64_t elapsed = get_timestamp_ns() - start_time;
            double msg_per_sec = (message_count * 1e9) / elapsed;
            std::cout << "Processed " << message_count 
                     << " messages (" << (msg_per_sec / 1e6) << "M msg/s)" << std::endl;
        }
    }
    
    uint64_t elapsed = get_timestamp_ns() - start_time;
    double msg_per_sec = (message_count * 1e9) / elapsed;
    
    std::cout << "\nReplay complete:" << std::endl;
    std::cout << "  Total messages: " << message_count << std::endl;
    std::cout << "  Elapsed time: " << format_duration(elapsed) << std::endl;
    std::cout << "  Throughput: " << (msg_per_sec / 1e6) << " million msg/s" << std::endl;
    
    messages_processed_.store(message_count);
}

void FeedHandler::process_message(uint8_t msg_type, const uint8_t* data, size_t length) {
    switch (static_cast<ITCHMessageType>(msg_type)) {
        case ITCHMessageType::ADD_ORDER: {
            if (length >= sizeof(ITCHAddOrder)) {
                const auto* msg = reinterpret_cast<const ITCHAddOrder*>(data);
                handle_add_order(*msg);
            }
            break;
        }
        
        case ITCHMessageType::ORDER_CANCEL: {
            if (length >= sizeof(ITCHOrderCancel)) {
                const auto* msg = reinterpret_cast<const ITCHOrderCancel*>(data);
                handle_order_cancel(*msg);
            }
            break;
        }
        
        case ITCHMessageType::ORDER_DELETE: {
            if (length >= sizeof(ITCHOrderDelete)) {
                const auto* msg = reinterpret_cast<const ITCHOrderDelete*>(data);
                handle_order_delete(*msg);
            }
            break;
        }
        
        default:
            // Ignore other message types for now
            break;
    }
}

void FeedHandler::handle_add_order(const ITCHAddOrder& msg) {
    std::string symbol = parse_stock_symbol(msg.stock);
    
    Side side = (msg.buy_sell_indicator == 'B') ? Side::BUY : Side::SELL;
    uint64_t timestamp = __builtin_bswap64(msg.timestamp);
    uint64_t order_id = __builtin_bswap64(msg.order_ref_num);
    uint32_t price = __builtin_bswap32(msg.price);
    uint32_t quantity = __builtin_bswap32(msg.shares);
    
    engine_.submit_order(symbol.c_str(), order_id, timestamp, 
                        price, quantity, side, OrderType::LIMIT);
}

void FeedHandler::handle_order_cancel(const ITCHOrderCancel& msg) {
    uint64_t order_id = __builtin_bswap64(msg.order_ref_num);
    
    // Note: We'd need to track order_id to symbol mapping in production
    // For now, this is a simplified implementation
    (void)order_id;
}

void FeedHandler::handle_order_delete(const ITCHOrderDelete& msg) {
    uint64_t order_id = __builtin_bswap64(msg.order_ref_num);
    (void)order_id;
}

uint16_t FeedHandler::parse_uint16(const uint8_t* data) {
    return ntohs(*reinterpret_cast<const uint16_t*>(data));
}

uint32_t FeedHandler::parse_uint32(const uint8_t* data) {
    return ntohl(*reinterpret_cast<const uint32_t*>(data));
}

uint64_t FeedHandler::parse_uint64(const uint8_t* data) {
    return __builtin_bswap64(*reinterpret_cast<const uint64_t*>(data));
}

std::string FeedHandler::parse_stock_symbol(const char* data) {
    std::string symbol(data, 8);
    // Trim trailing spaces
    size_t end = symbol.find_last_not_of(' ');
    return (end != std::string::npos) ? symbol.substr(0, end + 1) : symbol;
}

void FeedHandler::start_live_feed(const std::string& interface, uint16_t port) {
    std::cout << "Live feed not implemented yet (interface: " << interface 
              << ", port: " << port << ")" << std::endl;
    // TODO: Implement UDP multicast receiver
}

void FeedHandler::stop_live_feed() {
    running_.store(false);
    if (feed_thread_.joinable()) {
        feed_thread_.join();
    }
}

uint64_t FeedHandler::get_messages_per_second() const noexcept {
    // Simplified - would need proper timing in production
    return 0;
}

} // namespace lob
