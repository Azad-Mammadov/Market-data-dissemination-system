#pragma once
// Ensures the header file is included only once during compilation.

#include <vector>
#include <mutex>
#include <random>

class OrderBook {
public:
    // Default constructor
    OrderBook() = default;

    // Constructor with depth parameter
    explicit OrderBook(int depth) : depth_(depth) {
        // Initialize bids and asks with random values for the given depth
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(1.0, 100.0);

        for (int i = 0; i < depth_; ++i) {
            bids.push_back(dis(gen));
            asks.push_back(dis(gen));
        }
    }

    // Returns the current list of bid prices
    std::vector<double> getBids() const {
        std::lock_guard<std::mutex> lock(mtx);
        return bids;
    }

    // Returns the current list of ask prices
    std::vector<double> getAsks() const {
        std::lock_guard<std::mutex> lock(mtx);
        return asks;
    }

    // Simulates updates to the order book (e.g., changes in bids and asks)
    void simulateUpdate() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!bids.empty() && !asks.empty()) {
            bids[0] += 0.1; // Example: Increment the first bid
            asks[0] -= 0.1; // Example: Decrement the first ask
        }
    }

private:
    int depth_ = 0; // Depth of the order book
    std::vector<double> bids; // Stores the bid prices
    std::vector<double> asks; // Stores the ask prices
    std::mutex mtx; // Ensures thread-safe access to bids and asks
};
