find_package(GTest)

if(GTest_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
    
    add_executable(test_order_book test_order_book.cpp ../src/order_book.cpp ../src/utils.cpp)
    target_link_libraries(test_order_book ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread numa)
    
    add_executable(test_matching_engine test_matching_engine.cpp 
                   ../src/order_book.cpp ../src/matching_engine.cpp 
                   ../src/feed_handler.cpp ../src/utils.cpp)
    target_link_libraries(test_matching_engine ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread numa)
    
    add_test(NAME OrderBookTests COMMAND test_order_book)
    add_test(NAME MatchingEngineTests COMMAND test_matching_engine)
else()
    message(WARNING "GoogleTest not found, tests will not be built")
endif()
