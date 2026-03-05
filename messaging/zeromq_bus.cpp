#include "zeromq_bus.hpp"
#include <iostream>

namespace cortex {

ZeroMQBus::ZeroMQBus() 
    : context_(1) {
}

ZeroMQBus::~ZeroMQBus() {
    Stop();
}

void ZeroMQBus::Connect(const std::string& endpoint, bool is_server) {
    is_hub_ = is_server;
    pub_socket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::pub);
    sub_socket_ = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::sub);

    // Ensure messages are flashed even if process exits
    int linger = 1000;
    pub_socket_->set(zmq::sockopt::linger, linger);

    if (is_server) {
        // The Hub binds to both
        pub_socket_->bind(endpoint + "_pub");
        sub_socket_->bind(endpoint + "_sub");
        // Hub MUST subscribe to everything to re-broadcast
        sub_socket_->set(zmq::sockopt::subscribe, "");
        sub_socket_->set(zmq::sockopt::rcvtimeo, 100); 
        std::cout << "[ZeroMQBus] Hub started at " << endpoint << "\n";
    } else {
        // Clients connect to the Hub's pub/sub
        sub_socket_->connect(endpoint + "_pub");
        pub_socket_->connect(endpoint + "_sub");
        sub_socket_->set(zmq::sockopt::rcvtimeo, 100);
        std::cout << "[ZeroMQBus] Client connected to " << endpoint << "\n";
    }
}
void ZeroMQBus::Publish(const Message& message) {
    if (!pub_socket_) return;

    std::string data = message.ToJson();
    zmq::message_t zmq_msg(data.size());
    memcpy(zmq_msg.data(), data.data(), data.size());
    
    // We prefix with the message type as a simple topic
    std::string topic = message.header.type;
    std::cout << "[ZeroMQBus] Publishing [" << topic << "] from " << message.header.sender << "\n";
    pub_socket_->send(zmq::buffer(topic), zmq::send_flags::sndmore);
    pub_socket_->send(zmq_msg, zmq::send_flags::none);

    // If we are a Hub, we also deliver locally because our SUB socket 
    // only listens to external clients on _sub.
    if (is_hub_) {
        for (auto& sub : subscriptions_) {
            if (sub.channel.empty() || sub.channel == topic) {
                sub.callback(message);
            }
        }
    }
}

void ZeroMQBus::Subscribe(const std::string& channel, std::function<void(const Message&)> callback) {
    if (!sub_socket_) return;

    sub_socket_->set(zmq::sockopt::subscribe, channel);
    subscriptions_.push_back({channel, callback});
}

void ZeroMQBus::Run() {
    running_ = true;
    if (!sub_socket_) return;

    while (running_) {
        zmq::message_t topic_msg;
        zmq::message_t content_msg;

        auto res1 = sub_socket_->recv(topic_msg, zmq::recv_flags::none);
        if (!res1) continue;
        auto res2 = sub_socket_->recv(content_msg, zmq::recv_flags::none);
        if (!res2) continue;

        if (res1 && res2) {
            std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
            std::string content(static_cast<char*>(content_msg.data()), content_msg.size());
            
            try {
                Message msg = Message::FromJson(content);
                std::cout << "[ZeroMQBus] Received [" << topic << "] from " << msg.header.sender << " on " << (is_hub_ ? "hub-sub" : "client-pub") << "\n";
                
                if (is_hub_) {
                    // Re-broadcast will ALSO trigger local callbacks
                    Publish(msg);
                } else {
                    // Clients just deliver locally
                    for (auto& sub : subscriptions_) {
                        if (sub.channel.empty() || sub.channel == topic) {
                            sub.callback(msg);
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[ZeroMQBus] Error parsing message: " << e.what() << "\n";
            }
        }
    }
}

void ZeroMQBus::Stop() {
    running_ = false;
}

} // namespace cortex
