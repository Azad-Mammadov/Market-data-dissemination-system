#include <iostream>
#include <memory>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "../proto/marketdata.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using namespace marketdata;

class MarketDataClient {
public:
    MarketDataClient(std::shared_ptr<Channel> channel)
        : stub_(MarketDataService::NewStub(channel)) {}

    void SubscribeToInstrument(int instrument_id) {
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<SubscriptionRequest, MarketDataMessage>> stream(
            stub_->Subscribe(&context));

        // Send subscription request
        SubscriptionRequest request;
        request.set_instrument_id(instrument_id);
        stream->Write(request);

        // Thread to read incoming messages
        std::thread reader([&stream]() {
            MarketDataMessage msg;
            while (stream->Read(&msg)) {
                if (msg.has_snapshot()) {
                    const Snapshot& snap = msg.snapshot();
                    std::cout << "\n[Snapshot] Instrument ID: " << snap.instrument_id() << std::endl;
                    std::cout << "Bids: ";
                    for (const auto& b : snap.bids()) std::cout << b << " ";
                    std::cout << "\nAsks: ";
                    for (const auto& a : snap.asks()) std::cout << a << " ";
                    std::cout << std::endl;
                } else if (msg.has_update()) {
                    const IncrementalUpdate& upd = msg.update();
                    std::cout << "\n[Update] Instrument ID: " << upd.instrument_id() << std::endl;
                    std::cout << "Bid changes: ";
                    for (const auto& b : upd.bid_changes()) std::cout << b << " ";
                    std::cout << "\nAsk changes: ";
                    for (const auto& a : upd.ask_changes()) std::cout << a << " ";
                    std::cout << std::endl;
                }
            }
        });

        reader.join(); // Wait for reader to finish (optional: add cancellation logic)
        Status status = stream->Finish();
        if (!status.ok()) {
            std::cerr << "[!] RPC failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<MarketDataService::Stub> stub_;
};

int main(int argc, char** argv) {
    std::string target = "localhost:50051";
    int instrument_id = 1; // default ID

    if (argc > 1) {
        instrument_id = std::stoi(argv[1]);
    }

    MarketDataClient client(grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    client.SubscribeToInstrument(instrument_id);

    return 0;
}
