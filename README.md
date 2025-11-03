# Ultra-Low-Latency Limit Order Book & Matching Engine

A production-grade, high-performance limit order book and matching engine implemented in C++17, optimized for sub-100µs latency on modern CPUs (AMD EPYC, Intel Xeon).

## Features

- **Sub-100µs End-to-End Latency**: Market data ingestion → matching → execution reporting
- **Lock-Free Architecture**: SPSC queues, atomic operations, memory barriers
- **NUMA-Aware Memory Management**: Cache-aligned (64-byte), huge pages (2MB)
- **Price-Time Priority Matching**: FIFO queue discipline
- **NASDAQ ITCH 5.0 Support**: Real-time market data processing
- **Comprehensive Testing**: GoogleTest suite with unit and integration tests
- **Extensive Benchmarking**: 100M+ message throughput validation

## Architecture

### Core Components

1. **Order Book (`OrderBook`)**: Binary search tree of price levels, each containing a doubly-linked list of orders
2. **Matching Engine (`MatchingEngine`)**: Central coordinator with NUMA-aware order pool
3. **Feed Handler (`FeedHandler`)**: NASDAQ ITCH message parser and replay engine
4. **Lock-Free Structures**: SPSC queue for execution reports

### Data Structures

- **Intrusive Doubly-Linked Lists**: O(1) insertion/deletion, minimal allocations
- **Binary Search Tree**: Price levels organized for fast best bid/ask lookup
- **SPSC Queue**: Single-producer, single-consumer lock-free queue
- **Order Pool**: Pre-allocated, NUMA-aware memory for orders

## Build Instructions

### Prerequisites

