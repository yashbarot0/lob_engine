cmake_minimum_required(VERSION 3.15)
project(lob_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for low latency
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -funroll-loops -finline-functions")

# Link-time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Additional performance flags
add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_options(-mavx2 -mfma) # AVX2 support

# Threading
find_package(Threads REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/order_book.cpp
    src/matching_engine.cpp
    src/feed_handler.cpp
    src/utils.cpp
)

# Main executable
add_executable(lob_engine src/main.cpp ${SOURCES})
target_link_libraries(lob_engine PRIVATE Threads::Threads numa)

# Benchmarks
add_executable(replay_itch benchmarks/replay_itch.cpp ${SOURCES})
target_link_libraries(replay_itch PRIVATE Threads::Threads numa)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Install targets
install(TARGETS lob_engine DESTINATION bin)
