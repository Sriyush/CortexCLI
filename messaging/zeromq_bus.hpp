#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <zmq.hpp>
#include "message.hpp"

namespace cortex {

class MessageBus {
public:
    virtual ~MessageBus() = default;

    virtual void Publish(const Message& message) = 0;
    virtual void Subscribe(const std::string& channel, std::function<void(const Message&)> callback) = 0;
    
    // Connect to the bus
    virtual void Connect(const std::string& endpoint, bool is_server = false) = 0;

    // Run the event loop for receiving messages
    virtual void Run() = 0;
    virtual void Stop() = 0;
};

class ZeroMQBus : public MessageBus {
public:
    ZeroMQBus();
    ~ZeroMQBus();

    void Publish(const Message& message) override;
    void Subscribe(const std::string& channel, std::function<void(const Message&)> callback) override;
    void Connect(const std::string& endpoint, bool is_server = false) override;

    // Run the event loop for receiving messages
    void Run();
    void Stop();

private:
    zmq::context_t context_;
    std::unique_ptr<zmq::socket_t> pub_socket_;
    std::unique_ptr<zmq::socket_t> sub_socket_;
    bool running_ = false;
    bool is_hub_ = false;
    
    struct Subscription {
        std::string channel;
        std::function<void(const Message&)> callback;
    };
    std::vector<Subscription> subscriptions_;

};

} // namespace cortex
