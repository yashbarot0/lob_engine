#include <iostream>
#include <map>
#include <list>
#include <memory>
#include <iomanip>

// Simple order structure
struct Order {
    uint64_t id;
    double price;
    uint32_t quantity;
    bool is_buy;
    
    Order(uint64_t id_, double p, uint32_t q, bool buy)
        : id(id_), price(p), quantity(q), is_buy(buy) {}
};

// Order book using std::map for price levels
class OrderBook {
private:
    // Buy orders: highest price first (reverse order)
    std::map<double, std::list<Order>, std::greater<double>> bids;
    // Sell orders: lowest price first (normal order)
    std::map<double, std::list<Order>> asks;
    
public:
    void add_order(uint64_t id, double price, uint32_t quantity, bool is_buy) {
        Order order(id, price, quantity, is_buy);
        
        if (is_buy) {
            // Try to match with existing sell orders
            while (order.quantity > 0 && !asks.empty()) {
                auto& [ask_price, ask_orders] = *asks.begin();
                
                if (order.price < ask_price) break; // No match
                
                auto& best_ask = ask_orders.front();
                uint32_t trade_qty = std::min(order.quantity, best_ask.quantity);
                
                std::cout << "TRADE: " << trade_qty << " @ $" << ask_price 
                         << " (Buy #" << order.id << " x Sell #" << best_ask.id << ")\n";
                
                order.quantity -= trade_qty;
                best_ask.quantity -= trade_qty;
                
                if (best_ask.quantity == 0) {
                    ask_orders.pop_front();
                    if (ask_orders.empty()) asks.erase(asks.begin());
                }
            }
            
            // Add remaining quantity to book
            if (order.quantity > 0) {
                bids[order.price].push_back(order);
            }
        } else {
            // Try to match with existing buy orders
            while (order.quantity > 0 && !bids.empty()) {
                auto& [bid_price, bid_orders] = *bids.begin();
                
                if (order.price > bid_price) break; // No match
                
                auto& best_bid = bid_orders.front();
                uint32_t trade_qty = std::min(order.quantity, best_bid.quantity);
                
                std::cout << "TRADE: " << trade_qty << " @ $" << bid_price 
                         << " (Sell #" << order.id << " x Buy #" << best_bid.id << ")\n";
                
                order.quantity -= trade_qty;
                best_bid.quantity -= trade_qty;
                
                if (best_bid.quantity == 0) {
                    bid_orders.pop_front();
                    if (bid_orders.empty()) bids.erase(bids.begin());
                }
            }
            
            // Add remaining quantity to book
            if (order.quantity > 0) {
                asks[order.price].push_back(order);
            }
        }
    }
    
    void print_book(int depth = 5) {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "ORDER BOOK\n";
        std::cout << std::string(50, '=') << "\n";
        
        // Print asks (top 5, highest to lowest)
        auto ask_it = asks.rbegin();
        for (int i = 0; i < depth && ask_it != asks.rend(); ++i, ++ask_it) {
            uint32_t total = 0;
            for (const auto& order : ask_it->second) total += order.quantity;
            std::cout << std::setw(10) << total << " @ $" << std::fixed 
                     << std::setprecision(2) << std::setw(8) << ask_it->first 
                     << " [ASK]\n";
        }
        
        std::cout << std::string(50, '-') << "\n";
        
        // Print bids (top 5)
        auto bid_it = bids.begin();
        for (int i = 0; i < depth && bid_it != bids.end(); ++i, ++bid_it) {
            uint32_t total = 0;
            for (const auto& order : bid_it->second) total += order.quantity;
            std::cout << std::setw(10) << total << " @ $" << std::fixed 
                     << std::setprecision(2) << std::setw(8) << bid_it->first 
                     << " [BID]\n";
        }
        std::cout << std::string(50, '=') << "\n\n";
    }
    
    void get_best_bid_ask() {
        if (!bids.empty()) {
            std::cout << "Best Bid: $" << bids.begin()->first << "\n";
        }
        if (!asks.empty()) {
            std::cout << "Best Ask: $" << asks.begin()->first << "\n";
        }
        if (!bids.empty() && !asks.empty()) {
            double spread = asks.begin()->first - bids.begin()->first;
            std::cout << "Spread: $" << spread << "\n";
        }
    }
};

int main() {
    std::cout << "=== Minimal Order Book Demo ===\n\n";
    
    OrderBook book;
    
    // Add some initial orders
    std::cout << "Adding initial orders...\n\n";
    
    book.add_order(1, 100.50, 100, false);  // Sell 100 @ $100.50
    book.add_order(2, 100.75, 150, false);  // Sell 150 @ $100.75
    book.add_order(3, 101.00, 200, false);  // Sell 200 @ $101.00
    
    book.add_order(4, 100.00, 100, true);   // Buy 100 @ $100.00
    book.add_order(5, 99.75, 150, true);    // Buy 150 @ $99.75
    book.add_order(6, 99.50, 200, true);    // Buy 200 @ $99.50
    
    book.print_book();
    book.get_best_bid_ask();
    
    // Add aggressive buy order that matches
    std::cout << "\nAdding aggressive BUY order: 250 @ $101.00\n\n";
    book.add_order(7, 101.00, 250, true);
    
    book.print_book();
    book.get_best_bid_ask();
    
    // Add aggressive sell order that matches
    std::cout << "\nAdding aggressive SELL order: 200 @ $99.50\n\n";
    book.add_order(8, 99.50, 200, false);
    
    book.print_book();
    book.get_best_bid_ask();
    
    return 0;
}
