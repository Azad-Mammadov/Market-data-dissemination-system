#pragma once
// Ensures the header file is included only once during compilation to prevent redefinition errors.

#include <vector>
// Provides the `std::vector` container, which is used to store the bids and asks in the order book.

#include <mutex>
// Provides the `std::mutex` class for thread-safe access to shared resources (e.g., bids and asks).

#include <random>
// Provides utilities for generating random numbers, which might be used in `simulateUpdate` to simulate order book changes.

class OrderBook {
private:
    std::vector<double> bids; // Stores the bid prices in the order book.
    std::vector<double> asks; // Stores the ask prices in the order book.
    std::mutex mtx; // Ensures thread-safe access to the bids and asks.

public:
    void simulateUpdate(); // Simulates updates to the order book (e.g., changes in bids and asks).
    std::vector<double> getBids(); // Returns the current list of bid prices.
    std::vector<double> getAsks(); // Returns the current list of ask prices.
};
