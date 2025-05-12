#include <iostream>
// Provides input and output stream functionality, such as printing to the console (e.g., std::cout).

#include <fstream>
// Enables file input and output operations, such as reading from or writing to files.

#include <memory>
// Provides smart pointers like std::shared_ptr and std::unique_ptr for managing dynamic memory safely.

#include <thread>
// Allows the creation and management of threads for concurrent execution.

#include <unordered_map>
// Implements a hash table-based associative container for key-value pairs, like a dictionary.

#include <chrono>
// Provides utilities for time-related operations, such as measuring durations or sleeping threads.

#include <mutex>
// Provides mutual exclusion primitives (e.g., std::mutex) for thread-safe access to shared resources.

#include <grpcpp/grpcpp.h>
// Includes the gRPC C++ library for building gRPC servers and clients.


#include "../generated/marketdata.grpc.pb.h"

// #include "../include/nlohmann/json.hpp" // Commented out for now

#include "orderbook.hpp"
// Includes the custom `OrderBook` class, which likely manages order book data for financial instruments.

#include <csignal>
// Provides signal handling functions.

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
// using json = nlohmann::json; // Commented out for now
using namespace marketdata;

struct Instrument {
    int id;
    std::string symbol;
    int depth;
};

// Thread-safe store of order books
std::unordered_map<int, std::shared_ptr<OrderBook>> orderBooks;
std::mutex orderBookMutex;

// Load instruments from config file
std::vector<Instrument> loadInstruments(const std::string& filename) {
    // Temporarily return an empty vector since JSON parsing is disabled
    std::cout << "[!] JSON functionality is temporarily disabled." << std::endl;
    return {};
}

class MarketDataServiceImpl final : public MarketDataService::Service {
public:
    Status Subscribe(ServerContext* context,
                     ServerReaderWriter<MarketDataMessage, SubscriptionRequest>* stream) override {
        SubscriptionRequest request;
        while (stream->Read(&request)) {
            int id = request.instrument_id();

            std::cout << "[+] Client subscribed to instrument: " << id << std::endl;

            // Send snapshot
            {
                std::lock_guard<std::mutex> lock(orderBookMutex);
                if (orderBooks.find(id) != orderBooks.end()) {
                    auto book = orderBooks[id];

                    Snapshot* snapshot = new Snapshot();
                    snapshot->set_instrument_id(id);
                    for (double b : book->getBids()) snapshot->add_bids(b);
                    for (double a : book->getAsks()) snapshot->add_asks(a);

                    MarketDataMessage msg;
                    msg.set_allocated_snapshot(snapshot);
                    stream->Write(msg);
                }
            }

            // Send periodic incremental updates
            std::thread([stream, id]() {
                while (true) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    {
                        std::lock_guard<std::mutex> lock(orderBookMutex);
                        if (orderBooks.find(id) != orderBooks.end()) {
                            auto book = orderBooks[id];
                            book->simulateUpdate();

                            IncrementalUpdate* update = new IncrementalUpdate();
                            update->set_instrument_id(id);
                            for (double b : book->getBids()) update->add_bid_changes(b);
                            for (double a : book->getAsks()) update->add_ask_changes(a);

                            MarketDataMessage msg;
                            msg.set_allocated_update(update);
                            stream->Write(msg);
                        }
                    }
                }
            }).detach();
        }

        return Status::OK;
    }
};

void RunServer(const std::vector<Instrument>& instruments) {
    // Initialize order books
    for (const auto& inst : instruments) {
        orderBooks[inst.id] = std::make_shared<OrderBook>(inst.depth);
    }

    std::string server_address("0.0.0.0:50051");
    MarketDataServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[*] Server listening on " << server_address << std::endl;

    server->Wait();
}

void HandleSignal(int signal) {
    std::cout << "Shutting down server..." << std::endl;
    exit(0);
}

int main() {
    signal(SIGINT, HandleSignal);
    auto instruments = loadInstruments("config.json");
    RunServer(instruments);
    return 0;
}

