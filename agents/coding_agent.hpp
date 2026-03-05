#pragma once

#include "base_agent.hpp"

namespace cortex {

class CodingAgent : public BaseAgent {
public:
    CodingAgent(const std::string& name, std::shared_ptr<MessageBus> bus, std::shared_ptr<LLMClient> llm)
        : BaseAgent(name, "coder", bus, llm) {}

protected:
    void on_debate_turn(const Message& msg) override {
        if (!is_participating_) return;
        if (msg.header.sender == name_) return;

        std::thread([this, msg]() {
            if (history_.size() < 4) {
                std::string sender = msg.header.sender;
                std::string content = msg.payload.content.get<std::string>();
                
                std::string prompt = "You are " + name_ + ", an elite Software Architect and Coder.\n"
                                     "Your goal is to focus on implementation details, efficiency, and code structure.\n"
                                     "Context: " + sender + " said: \"" + content + "\"\n"
                                     "Provide a response focusing on the practical implementation and architectural impact.";

                std::string response = llm_->Generate(prompt, {});
 
                if (!is_participating_) {
                    std::cout << "[Agent:" << name_ << "] Skipping response because debate stopped.\n";
                    return;
                }
 
                Message reply;
                reply.header.msg_id = name_ + "_" + std::to_string(std::time(nullptr));
                reply.header.sender = name_;
                reply.header.type = "DEBATE_TURN";
                reply.payload.action = "speak";
                reply.payload.content = "[CODER] " + response;
                
                history_.push_back({{"round", history_.size()}});
                std::this_thread::sleep_for(std::chrono::seconds(2));
                bus_->Publish(reply);
            }
        }).detach();
    }
};

} // namespace cortex
