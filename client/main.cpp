#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "../generated/marketdata.grpc.pb.h"
#include <queue>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReaderWriter;

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(6) << ms.count();
    
    return oss.str(); // Removed timezone offset to match server format
}

class MarketDataClient {
public:
    MarketDataClient(std::shared_ptr<Channel> channel)
        : stub_(MarketDataService::NewStub(channel)), running_(true) {}
        
    ~MarketDataClient() {
        running_ = false;
        if (subscription_thread_.joinable()) {
            subscription_thread_.join();
        }
    }
    
    void subscribeToInstrument(int instrument_id) {
        Subscription subscription;
        auto* subscribe = subscription.mutable_subscribe();
        subscribe->add_ids(instrument_id);
        
        subscription_queue_.push(subscription);
        std::cout << "Queued subscription to instrument " << instrument_id << std::endl;
    }
    
    void unsubscribeFromInstrument(int instrument_id) {
        Subscription subscription;
        auto* unsubscribe = subscription.mutable_unsubscribe();
        unsubscribe->add_ids(instrument_id);
        
        subscription_queue_.push(subscription);
        std::cout << "Queued unsubscription from instrument " << instrument_id << std::endl;
    }
    
    void start() {
        subscription_thread_ = std::thread(&MarketDataClient::runSubscriptionThread, this);
    }
    
    bool isRunning() const {
        return running_;
    }

private:
    void runSubscriptionThread() {
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<Subscription, OrderbookUpdate>> stream = 
            stub_->StreamOrderbookUpdates(&context);
            
        // Start reading thread
        std::thread reader([this, stream]() {
            OrderbookUpdate update;
            while (stream->Read(&update)) {
                if (update.has_snapshot()) {
                    handleSnapshot(update);
                } else if (update.has_incremental()) {
                    handleIncrementalUpdate(update);
                }
            }
            running_ = false;
        });
        
        // Process subscription queue
        try {
            while (running_) {
                // Check for new subscriptions
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (!subscription_queue_.empty()) {
                    Subscription subscription = subscription_queue_.front();
                    subscription_queue_.pop();
                    lock.unlock();
                    
                    // Send subscription
                    stream->Write(subscription);
                } else {
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in subscription thread: " << e.what() << std::endl;
        }
        
        // Finish writing
        stream->WritesDone();
        
        // Wait for reader thread
        if (reader.joinable()) {
            reader.join();
        }
        
        // Check status
        Status status = stream->Finish();
        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
        }
    }

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
        const auto& incremental = update.incremental();
        
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

    std::unique_ptr<MarketDataService::Stub> stub_;
    std::thread subscription_thread_;
    std::atomic<bool> running_;
    std::queue<Subscription> subscription_queue_;
    std::mutex queue_mutex_;
};

int main(int argc, char** argv) {
    // Parse command line arguments
    std::string server_address = "localhost:50051";
    if (argc > 1) {
        server_address = argv[1];
    }
    
    // Create channel and client
    MarketDataClient client(
        grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
        
    // Start client
    client.start();
    
    // Subscribe to a few instruments
    client.subscribeToInstrument(1);
    client.subscribeToInstrument(2);
    client.subscribeToInstrument(3);
    
    // Run until interrupted
    std::cout << "Client running. Press enter to quit." << std::endl;
    std::cin.get();
    
    return 0;
}