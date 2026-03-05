#pragma once

#include "agent.hpp"
#include "zeromq_bus.hpp"
#include "llama_client.hpp"
#include "tool.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <chrono>

namespace cortex {

class BaseAgent : public Agent {
public:
    BaseAgent(const std::string& name, const std::string& type, 
              std::shared_ptr<MessageBus> bus, 
              std::shared_ptr<LLMClient> llm)
        : name_(name), type_(type), bus_(bus), llm_(llm), state_(AgentState::IDLE), 
          is_participating_(false) {}

    std::string GetName() const override { return name_; }
    std::string GetType() const override { return type_; }
    AgentState GetState() const override { return state_; }

    void Start() override {
        state_ = AgentState::RUNNING;
        std::cout << "[Agent:" << name_ << "] Started.\n";
        
        // Subscribe to messages directed at this agent or general tasks
        bus_->Subscribe(name_, [this](const Message& msg) {
            this->on_message_received(msg);
        });

        // Participate in debates
        bus_->Subscribe("DEBATE_START", [this](const Message& msg) {
            this->on_debate_start(msg);
        });

        bus_->Subscribe("DEBATE_TURN", [this](const Message& msg) {
            this->on_debate_turn(msg);
        });
    }

    void Stop() override {
        state_ = AgentState::STOPPED;
        std::cout << "[Agent:" << name_ << "] Stopped.\n";
    }

    void Pause() override { state_ = AgentState::PAUSED; }
    void Resume() override { state_ = AgentState::RUNNING; }

    void Tick() override {
        if (state_ != AgentState::RUNNING) return;
        
        // The Tick logic will handle periodic background tasks if needed
    }

    void RegisterTool(std::shared_ptr<Tool> tool) {
        tools_[tool->GetName()] = tool;
    }

protected:
    virtual void on_message_received(const Message& msg) {
        std::cout << "[Agent:" << name_ << "] Message received: " << msg.payload.action << "\n";
    }

    virtual void on_debate_start(const Message& msg) {
        // Check if we are a participant
        if (msg.payload.params.contains("participants")) {
            bool participating = false;
            for (const auto& p : msg.payload.params["participants"]) {
                if (p == name_) { participating = true; break; }
            }
            is_participating_ = participating;
            if (!is_participating_) return;

            std::string topic = msg.payload.params.value("topic", "unknown");
            std::cout << "[Agent:" << name_ << "] Participating in debate: " << topic << "\n";
            
            // Send an introductory message
            Message intro;
            intro.header.msg_id = name_ + "_" + std::to_string(std::time(nullptr));
            intro.header.sender = name_;
            intro.header.type = "DEBATE_TURN";
            intro.payload.action = "speak";
            intro.payload.content = "I am " + name_ + " and I am ready to discuss topic: " + topic;
            
            std::cout << "[Agent:" << name_ << "] Sending intro message...\n";
            bus_->Publish(intro);
        }
    }

    virtual void on_debate_turn(const Message& msg) {
        if (!is_participating_) return;
        if (msg.header.sender == name_) return; // Don't respond to self

        // Run reasoning in a background thread to avoid blocking the MessageBus
        std::thread([this, msg]() {
            // Check history to avoid infinite loops in mock
            if (history_.size() < 3) {
                // Construct a prompt for the LLM
                std::string sender = msg.header.sender;
                std::string content = msg.payload.content.get<std::string>();
                std::string prompt = "You are " + name_ + ", an AI agent participating in a debate.\n"
                                     "Context: " + sender + " said: \"" + content + "\"\n"
                                     "Provide a concise and critical response to their point.";

                // Generate response using the integrated AI engine
                std::string response = llm_->Generate(prompt, {});

                Message reply;
                reply.header.msg_id = name_ + "_reply_" + std::to_string(std::time(nullptr));
                reply.header.sender = name_;
                reply.header.type = "DEBATE_TURN";
                reply.payload.action = "speak";
                reply.payload.content = response;
                
                // Add to local history to track rounds
                history_.push_back({{"round", history_.size()}});
                
                // Sleep a bit to simulate thinking and avoid flood
                std::this_thread::sleep_for(std::chrono::seconds(2));
                bus_->Publish(reply);
            } else {
                std::cout << "[Agent:" << name_ << "] Maximum debate rounds reached.\n";
            }
        }).detach();
    }

    std::string name_;
    std::string type_;
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<LLMClient> llm_;
    AgentState state_;
    bool is_participating_;
    
    std::map<std::string, std::shared_ptr<Tool>> tools_;
    std::vector<nlohmann::json> history_;
};

} // namespace cortex
