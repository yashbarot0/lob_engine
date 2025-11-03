#pragma once

#include "matching_engine.hpp"
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <functional>

namespace lob {

// NASDAQ ITCH 5.0 message types
enum class ITCHMessageType : uint8_t {
    SYSTEM_EVENT = 'S',
    STOCK_DIRECTORY = 'R',
    STOCK_TRADING_ACTION = 'H',
    REG_SHO_RESTRICTION = 'Y',
    MARKET_PARTICIPANT_POSITION = 'L',
    MWCB_DECLINE_LEVEL = 'V',
    MWCB_STATUS = 'W',
    IPO_QUOTING_PERIOD_UPDATE = 'K',
    ADD_ORDER = 'A',
    ADD_ORDER_MPID = 'F',
    ORDER_EXECUTED = 'E',
    ORDER_EXECUTED_WITH_PRICE = 'C',
    ORDER_CANCEL = 'X',
    ORDER_DELETE = 'D',
    ORDER_REPLACE = 'U',
    TRADE = 'P',
    CROSS_TRADE = 'Q',
    BROKEN_TRADE = 'B',
    NOII = 'I'
};

// ITCH Add Order message (type 'A')
struct ITCHAddOrder {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;       // nanoseconds
    uint64_t order_ref_num;
    char buy_sell_indicator;  // 'B' or 'S'
    uint32_t shares;
    char stock[8];
    uint32_t price;           // fixed-point 4 decimal places
} __attribute__((packed));

// ITCH Order Cancel message (type 'X')
struct ITCHOrderCancel {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_ref_num;
    uint32_t cancelled_shares;
} __attribute__((packed));

// ITCH Order Delete message (type 'D')
struct ITCHOrderDelete {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    uint64_t order_ref_num;
} __attribute__((packed));

// Feed handler for processing market data
class FeedHandler {
public:
    explicit FeedHandler(MatchingEngine& engine);
    ~FeedHandler();
    
    // File-based replay
    void replay_itch_file(const std::string& filename);
    
    // Real-time feed (placeholder for multicast/UDP)
    void start_live_feed(const std::string& interface, uint16_t port);
    void stop_live_feed();
    
    // Statistics
    uint64_t get_messages_processed() const noexcept { 
        return messages_processed_.load(); 
    }
    
    uint64_t get_messages_per_second() const noexcept;
    
private:
    MatchingEngine& engine_;
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> messages_processed_;
    std::atomic<uint64_t> last_timestamp_;
    
    std::thread feed_thread_;
    
    // Message parsing
    void process_message(uint8_t msg_type, const uint8_t* data, size_t length);
    void handle_add_order(const ITCHAddOrder& msg);
    void handle_order_cancel(const ITCHOrderCancel& msg);
    void handle_order_delete(const ITCHOrderDelete& msg);
    
    // Helpers
    static uint16_t parse_uint16(const uint8_t* data);
    static uint32_t parse_uint32(const uint8_t* data);
    static uint64_t parse_uint64(const uint8_t* data);
    static std::string parse_stock_symbol(const char* data);
};

} // namespace lob
