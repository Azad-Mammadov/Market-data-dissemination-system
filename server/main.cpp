#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "../generated/marketdata.grpc.pb.h"
#include "orderbook.hpp"
#include <csignal>
// #include "../include/nlohmann/json.hpp" // Commented out for now

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
/*
std::vector<Instrument> loadInstruments(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return {};
    }

    json config;
    file >> config;

    std::vector<Instrument> instruments;
    for (const auto& item : config["instruments"]) {
        instruments.push_back({
            item["id"].get<int>(),
            item["symbol"].get<std::string>(),
            item["depth"].get<int>()
        });
    }

    return instruments;
}
*/

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
            std::thread([stream, id, context]() {
                while (!context->IsCancelled()) {
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
    // auto instruments = loadInstruments("config.json"); // Commented out for now
    std::vector<Instrument> instruments = {
        {1, "AAPL", 10},
        {2, "GOOGL", 10},
        {3, "MSFT", 10}
    };
    RunServer(instruments);
    return 0;
}

