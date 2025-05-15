#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "../generated/marketdata.grpc.pb.h"
#include "orderbook.hpp"

// Include the gRPC server-related classes and functions.
using grpc::Server; // The main class for gRPC servers.
using grpc::ServerBuilder; // A helper class for building gRPC servers.
using grpc::ServerContext; // Represents the context for a single RPC call.
using grpc::ServerReaderWriter; // A bidirectional stream interface for gRPC servers.
using grpc::Status; // Represents the result of an RPC call.


// Function to get the current timestamp in the format "YYYY-MM-DDTHH:MM:SS.ssssss"
std::string getCurrentTimestamp() {
    // Get the current time
    auto now = std::chrono::system_clock::now();
    // Get the microseconds part of the current time
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    
    // Convert the current time to a time_t object
    auto timer = std::chrono::system_clock::to_time_t(now);
    // Convert the time_t object to a tm object
    std::tm bt = *std::localtime(&timer);
    
    // Create an output string stream
    std::ostringstream oss;
    // Format the tm object to the desired format
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S");
    // Add the microseconds part to the string
    oss << '.' << std::setfill('0') << std::setw(6) << ms.count();
    
    // Return the formatted string
    return oss.str();
}

// Convert from OrderBookLevel to proto OrderbookLevel
OrderbookLevel convertToProto(const OrderBookLevel& level) {
    OrderbookLevel proto_level;
    proto_level.set_price(level.price);
    proto_level.set_quantity(level.quantity);
    proto_level.set_is_buy(level.is_buy);
    return proto_level;
}

// Convert OrderBook::IncrementalUpdate to proto OrderbookLevelUpdate
OrderbookLevelUpdate convertUpdateToProto(const OrderBook::IncrementalUpdate& update) {
    OrderbookLevelUpdate proto_update;
    
    // Set update type
    switch (update.type) {
        case OrderBook::IncrementalUpdate::Type::ADD:
            proto_update.set_update_type(OrderbookLevelUpdateType::ADD);
            break;
        case OrderBook::IncrementalUpdate::Type::REPLACE:
            proto_update.set_update_type(OrderbookLevelUpdateType::REPLACE);
            break;
        case OrderBook::IncrementalUpdate::Type::REMOVE:
            proto_update.set_update_type(OrderbookLevelUpdateType::REMOVE);
            break;
        default:
            proto_update.set_update_type(OrderbookLevelUpdateType::INVALID);
    }
    
    // Set level
    *proto_update.mutable_level() = convertToProto(update.level);
    
    return proto_update;
}

class MarketDataServiceImpl final : public MarketDataService::Service {
public:
    // Constructor
    MarketDataServiceImpl() : running_(true) {
        // Create orderbooks for different instruments
        for (int i = 1; i <= 10; i++) {
            orderbooks_[i] = std::make_unique<OrderBook>(10); // 10 levels depth
        }
        
        // Start the update thread
        update_thread_ = std::thread(&MarketDataServiceImpl::updateThread, this);
    }
    
    // Destructor
    ~MarketDataServiceImpl() {
        running_ = false;
        if (update_thread_.joinable()) {
            update_thread_.join();
        }
    }

// Function to decode URL-encoded strings
std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            std::string hex = encoded.substr(i + 1, 2);
            int value = std::stoi(hex, nullptr, 16);
            decoded += static_cast<char>(value);
            i += 2; // Skip the next two characters
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}


    // Stream orderbook updates
    Status StreamOrderbookUpdates(
        ServerContext* context,
        ServerReaderWriter<OrderbookUpdate, Subscription>* stream) override {
        
        // Get the peer's address
        auto peer = context->peer();
        std::cout << "Added client " <<urlDecode (peer) << std::endl;
        
        // Add client to active_streams map
        {
            std::lock_guard<std::mutex> lock(streams_mutex_);
            active_streams_[peer] = stream;
            stream_subscriptions_[peer] = std::unordered_set<int>();
        }
        
        try {
            Subscription subscription;
            while (stream->Read(&subscription)) {
                // Process subscription/unsubscription
                if (subscription.has_subscribe()) {
                    for (int id : subscription.subscribe().ids()) {
                        std::cout << urlDecode(peer) << " subscribed to " << id << std::endl;
                        
                        // Add to subscriptions
                        {
                            std::lock_guard<std::mutex> lock(streams_mutex_);
                            stream_subscriptions_[peer].insert(id);
                        }
                        
                        // Send snapshot
                        OrderbookUpdate snapshot_update;
                        snapshot_update.set_instrument_id(id);
                        auto* snapshot = snapshot_update.mutable_snapshot();
                        
                        // Populate snapshot from orderbook
                        auto book_snapshot = orderbooks_[id]->getSnapshot();
                        
                        // Add bids
                        for (const auto& bid : book_snapshot.bids) {
                            *snapshot->add_bids() = convertToProto(bid);
                        }
                        
                        // Add asks
                        for (const auto& ask : book_snapshot.asks) {
                            *snapshot->add_asks() = convertToProto(ask);
                        }
                        
                        stream->Write(snapshot_update);
                    }
                }
                
                // Check if the subscription has an unsubscribe method
                if (subscription.has_unsubscribe()) {
                    // Iterate through the ids of the unsubscribe method
                    for (int id : subscription.unsubscribe().ids()) {
                        // Print the peer and the id of the unsubscribe method
                        std::cout << urlDecode(peer) << " unsubscribed from " << id << std::endl;
                        
                        // Remove from subscriptions
                        {
                            std::lock_guard<std::mutex> lock(streams_mutex_);
                            stream_subscriptions_[peer].erase(id);
                        }
                        
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
            
            // Clean up on exception
            std::lock_guard<std::mutex> lock(streams_mutex_);
            active_streams_.erase(peer);
            stream_subscriptions_.erase(peer);
            
            std::cout << "Removed client " << peer << " due to error" << std::endl;
            return Status::OK;
        }
        
        // Clean up normally
        {
            std::lock_guard<std::mutex> lock(streams_mutex_);
            active_streams_.erase(peer);
            stream_subscriptions_.erase(peer);
        }
        
        std::cout << "Removed client " << urlDecode(peer) << std::endl;
        return Status::OK;
    }

private:
    // Update thread
    void updateThread() {
        while (running_) {
            // Generate random updates for each instrument
            for (auto& [id, orderbook] : orderbooks_) {
                auto update = orderbook->generateUpdate();
                
                // Convert to proto message
                OrderbookLevelUpdate proto_update = convertUpdateToProto(update);
                
                // Find clients subscribed to this instrument
                std::lock_guard<std::mutex> lock(streams_mutex_);
                for (auto& [peer, subscriptions] : stream_subscriptions_) {
                    if (subscriptions.find(id) != subscriptions.end()) {
                        // Client is subscribed to this instrument
                        auto stream = active_streams_[peer];
                        sendOrderbookUpdate(id, proto_update, stream);
                    }
                }
            }
            
            // Sleep for a bit to avoid flooding with updates
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // This function sends an orderbook update to a stream
    void sendOrderbookUpdate(int instrument_id, 
                           const OrderbookLevelUpdate& update,
                           ServerReaderWriter<OrderbookUpdate, Subscription>* stream) {
        // Create a new OrderbookUpdate message
        OrderbookUpdate msg;
        // Set the instrument ID of the message
        msg.set_instrument_id(instrument_id);
        // Set the incremental update of the message
        *msg.mutable_incremental() = update;
        
        // Print the current timestamp and the instrument ID
        // std::cout << "[" << getCurrentTimestamp() << "] Sending incremental for " 
        //           << instrument_id << std::endl;
        // // Print the update type and the orderbook level details
        // std::cout << update.update_type() << " - " 
        //           << "OrderbookLevel ( Price = " << update.level().price()
        //           << ", IsBuy = " << (update.level().is_buy() ? "True" : "False")
        //           << ", Quantity = " << update.level().quantity() << " )" << std::endl;
                  
        // Write the message to the stream
        stream->Write(msg);
    }

    std::unordered_map<int, std::unique_ptr<OrderBook>> orderbooks_;
    std::unordered_map<std::string, ServerReaderWriter<OrderbookUpdate, Subscription>*> active_streams_;
    std::unordered_map<std::string, std::unordered_set<int>> stream_subscriptions_;
    std::mutex streams_mutex_;
    std::thread update_thread_;
    std::atomic<bool> running_;
};

int main(int argc, char** argv) {
    // Define the server address
    std::string server_address("0.0.0.0:50051");
    // Create a MarketDataServiceImpl object
    MarketDataServiceImpl service;
    
    // Create a ServerBuilder object
    ServerBuilder builder;
    // Add a listening port to the server
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register the service with the server
    builder.RegisterService(&service);
    
    // Build and start the server
    std::unique_ptr<Server> server(builder.BuildAndStart());
    // Print a message to the console indicating the server is listening
    std::cout << "Server listening on " << server_address << std::endl;
    
    // Wait for the server to stop
    server->Wait();
    
    // Return 0 to indicate successful execution
    return 0;
}
