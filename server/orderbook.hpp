#pragma once
#include <vector>
#include <mutex>
#include <random>
#include <algorithm>

struct OrderBookLevel {
    int32_t price;      // Changed from double to int32_t to match proto
    uint32_t quantity;  // Changed from double to uint32_t to match proto
    bool is_buy;        // Added missing field to match proto
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
    };

    Snapshot getSnapshot() const {
        std::lock_guard<std::mutex> lock(mtx);
        return {bids_, asks_};
    }

    IncrementalUpdate generateUpdate() {
        std::lock_guard<std::mutex> lock(mtx);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int32_t> price_dis(100, 200);  // Changed to int32_t
        static std::uniform_int_distribution<uint32_t> qty_dis(1, 100);     // Changed to uint32_t
        static std::uniform_int_distribution<> action_dis(0, 2);
        
        IncrementalUpdate update;
        update.level.is_buy = (gen() % 2 == 0);  // Set is_buy field
        
        auto& levels = update.level.is_buy ? bids_ : asks_;
        
        int action = action_dis(gen);
      
        if (levels.empty() || action == 0) {
            // Add new level
            update.type = IncrementalUpdate::Type::ADD;
            update.level.price = price_dis(gen);
            update.level.quantity = qty_dis(gen);
            levels.push_back(update.level);
        } else if (action == 1 && levels.size() > 1) {
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
        if (update.level.is_buy) {
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
        std::uniform_int_distribution<int32_t> price_dis(100, 200);  // Changed to int32_t
        std::uniform_int_distribution<uint32_t> qty_dis(1, 100);     // Changed to uint32_t

        for (int i = 0; i < depth_; ++i) {
            bids_.push_back({price_dis(gen), qty_dis(gen), true});   // Set is_buy = true for bids
            asks_.push_back({price_dis(gen), qty_dis(gen), false});  // Set is_buy = false for asks
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