#include "../include/matching_engine.hpp"
#include "../include/utils.hpp"
#include <gtest/gtest.h>

using namespace lob;

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        EngineConfig config;
        config.order_pool_size = 10000;
        config.enable_logging = false;
        engine = std::make_unique<MatchingEngine>(config);
        engine->start();
    }
    
    void TearDown() override {
        engine->stop();
        engine.reset();
    }
    
    std::unique_ptr<MatchingEngine> engine;
};

TEST_F(MatchingEngineTest, SubmitLimitOrder) {
    engine->submit_order("AAPL", 1, get_timestamp_ns(), 
                        100000, 100, Side::BUY, OrderType::LIMIT);
    
    EXPECT_EQ(engine->get_total_orders(), 1);
}

TEST_F(MatchingEngineTest, MultipleSymbols) {
    engine->submit_order("AAPL", 1, get_timestamp_ns(), 
                        100000, 100, Side::BUY, OrderType::LIMIT);
    engine->submit_order("MSFT", 2, get_timestamp_ns(), 
                        200000, 100, Side::BUY, OrderType::LIMIT);
    
    EXPECT_NE(engine->get_book("AAPL"), nullptr);
    EXPECT_NE(engine->get_book("MSFT"), nullptr);
    EXPECT_EQ(engine->get_total_orders(), 2);
}

TEST_F(MatchingEngineTest, OrderMatching) {
    engine->submit_order("AAPL", 1, get_timestamp_ns(), 
                        100000, 100, Side::SELL, OrderType::LIMIT);
    engine->submit_order("AAPL", 2, get_timestamp_ns(), 
                        100000, 50, Side::BUY, OrderType::LIMIT);
    
    EXPECT_EQ(engine->get_total_matches(), 1);
    
    ExecutionReport report;
    bool has_report = engine->get_execution_queue().pop(report);
    
    EXPECT_TRUE(has_report);
    EXPECT_EQ(report.executed_quantity, 50);
}

TEST_F(MatchingEngineTest, SPSCQueuePerformance) {
    constexpr size_t num_reports = 10000;
    
    for (size_t i = 0; i < num_reports; ++i) {
        ExecutionReport report(i, i, get_timestamp_ns(), 
                              100000, 100, Side::BUY, true);
        EXPECT_TRUE(engine->get_execution_queue().push(report));
    }
    
    size_t count = 0;
    ExecutionReport report;
    while (engine->get_execution_queue().pop(report)) {
        ++count;
    }
    
    EXPECT_EQ(count, num_reports);
}

TEST_F(MatchingEngineTest, HighVolumeStressTest) {
    constexpr size_t num_orders = 100000;
    
    for (size_t i = 0; i < num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        uint32_t price = 100000 + (i % 10) * 10;
        
        engine->submit_order("AAPL", i, get_timestamp_ns(),
                           price, 100, side, OrderType::LIMIT);
    }
    
    EXPECT_EQ(engine->get_total_orders(), num_orders);
    EXPECT_GT(engine->get_total_matches(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
