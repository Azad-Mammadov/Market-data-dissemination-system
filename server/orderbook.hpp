#pragma once
#include <vector>
#include <mutex>
#include <random>
#include <algorithm>

struct OrderBookLevel {
    double price;
    double quantity;
};

class OrderBook {
public:
    explicit OrderBook(int depth) : depth_(depth) {
        initializeBook();
    }

    struct Snapshot {
        std::vector<OrderBookLevel> bids;
        std::vector<OrderBookLevel> asks;
    };

    struct IncrementalUpdate {
        enum class Type { ADD, REPLACE, REMOVE };
        Type type;
        OrderBookLevel level;
        bool is_bid;
    };

    Snapshot getSnapshot() const {
        std::lock_guard<std::mutex> lock(mtx);
        return {bids_, asks_};
    }

    IncrementalUpdate generateUpdate() {
        std::lock_guard<std::mutex> lock(mtx);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> price_dis(100.0, 200.0);
        static std::uniform_real_distribution<> qty_dis(1.0, 100.0);
        static std::uniform_int_distribution<> action_dis(0, 2);
        
        IncrementalUpdate update;
        update.is_bid = (gen() % 2 == 0);
        
        auto& levels = update.is_bid ? bids_ : asks_;
        
        if (levels.empty() || action_dis(gen) == 0) {
            // Add new level
            update.type = IncrementalUpdate::Type::ADD;
            update.level = {price_dis(gen), qty_dis(gen)};
            levels.push_back(update.level);
        } else if (action_dis(gen) == 1 && levels.size() > 1) {
            // Remove level
            update.type = IncrementalUpdate::Type::REMOVE;
            std::uniform_int_distribution<> index_dis(0, levels.size()-1);
            int idx = index_dis(gen);
            update.level = levels[idx];
            levels.erase(levels.begin() + idx);
        } else {
            // Replace level
            update.type = IncrementalUpdate::Type::REPLACE;
            std::uniform_int_distribution<> index_dis(0, levels.size()-1);
            int idx = index_dis(gen);
            update.level = levels[idx];
            levels[idx].quantity = qty_dis(gen);
        }
        
        // Keep sorted (bids descending, asks ascending)
        if (update.is_bid) {
            std::sort(bids_.begin(), bids_.end(), 
                [](const auto& a, const auto& b) { return a.price > b.price; });
        } else {
            std::sort(asks_.begin(), asks_.end(), 
                [](const auto& a, const auto& b) { return a.price < b.price; });
        }
        
        // Trim to depth
        if (bids_.size() > depth_) bids_.resize(depth_);
        if (asks_.size() > depth_) asks_.resize(depth_);
        
        return update;
    }

private:
    void initializeBook() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dis(100.0, 200.0);
        std::uniform_real_distribution<> qty_dis(1.0, 100.0);

        for (int i = 0; i < depth_; ++i) {
            bids_.push_back({price_dis(gen), qty_dis(gen)});
            asks_.push_back({price_dis(gen), qty_dis(gen)});
        }

        // Sort bids (descending) and asks (ascending)
        std::sort(bids_.begin(), bids_.end(), 
            [](const auto& a, const auto& b) { return a.price > b.price; });
        std::sort(asks_.begin(), asks_.end(), 
            [](const auto& a, const auto& b) { return a.price < b.price; });
    }

    int depth_;
    std::vector<OrderBookLevel> bids_;
    std::vector<OrderBookLevel> asks_;
    mutable std::mutex mtx;
};