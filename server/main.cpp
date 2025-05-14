#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
// ... other includes remain the same ...

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(6) << ms.count();
    
    return oss.str();
}

class MarketDataServiceImpl final : public MarketDataService::Service {
public:
    Status StreamOrderbookUpdates(
        ServerContext* context,
        ServerReaderWriter<OrderbookUpdate, Subscription>* stream) override {
        
        auto peer = context->peer();
        std::cout << "Added client " << peer << std::endl;
        
        try {
            Subscription subscription;
            while (stream->Read(&subscription)) {
                // Process subscription/unsubscription
                if (subscription.has_subscribe()) {
                    for (int id : subscription.subscribe().ids()) {
                        std::cout << peer << " subscribed to " << id << std::endl;
                        
                        // Send snapshot
                        OrderbookUpdate snapshot_update;
                        snapshot_update.set_instrument_id(id);
                        auto* snapshot = snapshot_update.mutable_snapshot();
                        // ... populate snapshot ...
                        stream->Write(snapshot_update);
                    }
                }
                
                if (subscription.has_unsubscribe()) {
                    for (int id : subscription.unsubscribe().ids()) {
                        std::cout << peer << " unsubscribed from " << id << std::endl;
                        
                        // Send empty snapshot
                        OrderbookUpdate empty_update;
                        empty_update.set_instrument_id(id);
                        empty_update.mutable_snapshot(); // Empty snapshot
                        stream->Write(empty_update);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in StreamOrderbookUpdates: " << e.what() << std::endl;
        }
        
        std::cout << "Removed client " << peer << std::endl;
        return Status::OK;
    }

private:
    void sendOrderbookUpdate(int instrument_id, 
                           const OrderbookLevelUpdate& update,
                           ServerReaderWriter<OrderbookUpdate, Subscription>* stream) {
        OrderbookUpdate msg;
        msg.set_instrument_id(instrument_id);
        *msg.mutable_incremental()->mutable_update() = update;
        
        std::cout << "[" << getCurrentTimestamp() << "] Sending incremental for " 
                  << instrument_id << std::endl;
        std::cout << update.update_type() << " - " 
                  << "OrderbookLevel ( Price = " << update.level().price()
                  << ", IsBuy = " << (update.level().is_buy() ? "True" : "False")
                  << ", Quantity = " << update.level().quantity() << " )" << std::endl;
                  
        stream->Write(msg);
    }
};