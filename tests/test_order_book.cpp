#include "../include/order_book.hpp"
#include "../include/utils.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace lob;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<OrderBook>();
    }
    
    void TearDown() override {
        book.reset();
    }
    
    std::unique_ptr<OrderBook> book;
};

TEST_F(OrderBookTest, AddBuyOrder) {
    Order order(1, get_timestamp_ns(), 100000, 100, Side::BUY, OrderType::LIMIT);
    book->add_order(&order);
    
    EXPECT_EQ(book->get_order_count(), 1);
    EXPECT_NE(book->get_best_bid(), nullptr);
    EXPECT_EQ(book->get_best_bid()->price, 100000);
    EXPECT_EQ(book->get_best_bid()->total_volume, 100);
}

TEST_F(OrderBookTest, AddSellOrder) {
    Order order(1, get_timestamp_ns(), 100100, 100, Side::SELL, OrderType::LIMIT);
    book->add_order(&order);
    
    EXPECT_EQ(book->get_order_count(), 1);
    EXPECT_NE(book->get_best_ask(), nullptr);
    EXPECT_EQ(book->get_best_ask()->price, 100100);
    EXPECT_EQ(book->get_best_ask()->total_volume, 100);
}

TEST_F(OrderBookTest, SpreadCalculation) {
    Order buy(1, get_timestamp_ns(), 100000, 100, Side::BUY, OrderType::LIMIT);
    Order sell(2, get_timestamp_ns(), 100100, 100, Side::SELL, OrderType::LIMIT);
    
    book->add_order(&buy);
    book->add_order(&sell);
    
    EXPECT_EQ(book->get_spread(), 100);
}

TEST_F(OrderBookTest, MatchingOrders) {
    // Add passive sell order
    Order sell(1, get_timestamp_ns(), 100000, 100, Side::SELL, OrderType::LIMIT);
    book->add_order(&sell);
    
    // Add aggressive buy order
    Order buy(2, get_timestamp_ns(), 100000, 50, Side::BUY, OrderType::LIMIT);
    auto reports = book->match_order(&buy);
    
    EXPECT_EQ(reports.size(), 1);
    EXPECT_EQ(reports[0].executed_quantity, 50);
    EXPECT_EQ(reports[0].price, 100000);
}

TEST_F(OrderBookTest, PartialFill) {
    Order sell(1, get_timestamp_ns(), 100000, 100, Side::SELL, OrderType::LIMIT);
    book->add_order(&sell);
    
    Order buy(2, get_timestamp_ns(), 100000, 150, Side::BUY, OrderType::LIMIT);
    auto reports = book->match_order(&buy);
    
    EXPECT_EQ(reports.size(), 1);
    EXPECT_EQ(reports[0].executed_quantity, 100);
    EXPECT_EQ(buy.remaining_quantity, 50);
}

TEST_F(OrderBookTest, PriceTimePriority) {
    // Add multiple orders at same price
    Order sell1(1, get_timestamp_ns(), 100000, 50, Side::SELL, OrderType::LIMIT);
    Order sell2(2, get_timestamp_ns() + 100, 100000, 50, Side::SELL, OrderType::LIMIT);
    
    book->add_order(&sell1);
    book->add_order(&sell2);
    
    // Aggressive buy should match first order first (FIFO)
    Order buy(3, get_timestamp_ns(), 100000, 60, Side::BUY, OrderType::LIMIT);
    auto reports = book->match_order(&buy);
    
    EXPECT_EQ(reports.size(), 2);
    EXPECT_EQ(reports[0].order_id, 3); // Aggressive order ID
}

TEST_F(OrderBookTest, CancelOrder) {
    Order order(1, get_timestamp_ns(), 100000, 100, Side::BUY, OrderType::LIMIT);
    book->add_order(&order);
    
    EXPECT_EQ(book->get_order_count(), 1);
    
    book->cancel_order(1);
    
    EXPECT_EQ(book->get_order_count(), 0);
    EXPECT_EQ(book->get_best_bid(), nullptr);
}

TEST_F(OrderBookTest, ModifyOrder) {
    Order order(1, get_timestamp_ns(), 100000, 100, Side::BUY, OrderType::LIMIT);
    book->add_order(&order);
    
    book->modify_order(1, 150);
    
    EXPECT_EQ(book->get_best_bid()->total_volume, 150);
}

TEST_F(OrderBookTest, MultiplePriceLevels) {
    Order buy1(1, get_timestamp_ns(), 100000, 100, Side::BUY, OrderType::LIMIT);
    Order buy2(2, get_timestamp_ns(), 99900, 100, Side::BUY, OrderType::LIMIT);
    Order buy3(3, get_timestamp_ns(), 99800, 100, Side::BUY, OrderType::LIMIT);
    
    book->add_order(&buy1);
    book->add_order(&buy2);
    book->add_order(&buy3);
    
    EXPECT_EQ(book->get_order_count(), 3);
    EXPECT_EQ(book->get_best_bid()->price, 100000);
    EXPECT_EQ(book->get_total_bid_volume(), 300);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
