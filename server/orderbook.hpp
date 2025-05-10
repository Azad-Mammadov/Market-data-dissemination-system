#pragma once
#include <vector>
#include <mutex>
#include <random>

class OrderBook {
private:
    std::vector<double> bids;
    std::vector<double> asks;
    std::mutex mtx;

public:
    void simulateUpdate();
    std::vector<double> getBids();
    std::vector<double> getAsks();
};
