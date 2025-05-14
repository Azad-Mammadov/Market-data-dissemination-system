#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
// ... other includes remain the same ...

class MarketDataClient {
public:
    // ... existing methods remain the same ...

private:
    void handleSnapshot(const OrderbookUpdate& update) {
        auto timestamp = getCurrentTimestamp();
        
        std::cout << "[" << timestamp << "] Received (empty: " 
                  << (update.snapshot().bids().empty() && update.snapshot().asks().empty())
                  << ") snapshot for " << update.instrument_id() << std::endl;
        
        if (!update.snapshot().asks().empty()) {
            std::cout << "-- Asks --" << std::endl;
            for (const auto& ask : update.snapshot().asks()) {
                printOrderbookLevel(ask);
            }
        }
        
        if (!update.snapshot().bids().empty()) {
            std::cout << "-- Bids --" << std::endl;
            for (const auto& bid : update.snapshot().bids()) {
                printOrderbookLevel(bid);
            }
        }
    }

    void handleIncrementalUpdate(const OrderbookUpdate& update) {
        auto timestamp = getCurrentTimestamp();
        const auto& incremental = update.incremental().update();
        
        std::cout << "[" << timestamp << "] Received incremental for " 
                  << update.instrument_id() << std::endl;
        std::cout << incremental.update_type() << " - ";
        printOrderbookLevel(incremental.level());
    }

    void printOrderbookLevel(const OrderbookLevel& level) {
        std::cout << "OrderbookLevel ( Price = " << level.price()
                  << ", IsBuy = " << (level.is_buy() ? "True" : "False")
                  << ", Quantity = " << level.quantity() << " )" << std::endl;
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()) % 1000000;
        
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&timer);
        
        std::ostringstream oss;
        oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(6) << ms.count();
        oss << "-05:00"; // Timezone offset
        
        return oss.str();
    }
};