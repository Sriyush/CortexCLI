#pragma once

#include <cortex/agents/base_agent.hpp>

namespace cortex {

class ResearchAgent : public BaseAgent {
public:
    ResearchAgent(const std::string& name, std::shared_ptr<MessageBus> bus, std::shared_ptr<LLMClient> llm)
        : BaseAgent(name, "researcher", bus, llm) {}

protected:
    void on_debate_turn(const Message& msg) override {
        if (!is_participating_) return;
        if (msg.header.sender == name_) return;

        std::thread([this, msg]() {
            if (history_.size() < 4) {
                std::string sender = msg.header.sender;
                std::string content = msg.payload.content.get<std::string>();
                
                std::string prompt = "You are " + name_ + ", an expert Research Agent.\n"
                                     "Your goal is to provide deep technical insights and data-backed arguments.\n"
                                     "Context: " + sender + " said: \"" + content + "\"\n"
                                     "Research and provide a highly technical counter-point or supportive evidence.";

                GenerationResult result = llm_->Generate(prompt, {});
 
                if (!is_participating_) {
                    std::cout << "[Agent:" << name_ << "] Skipping response because debate stopped.\n";
                    return;
                }
 
                // Report tokens
                StatsManager::GetInstance().AddTokens(result.prompt_tokens + result.completion_tokens);
 
                Message reply;
                reply.header.msg_id = name_ + "_" + std::to_string(std::time(nullptr));
                reply.header.sender = name_;
                reply.header.type = "DEBATE_TURN";
                reply.payload.action = "speak";
                reply.payload.content = "[RESEARCHER] " + result.text;
                
                history_.push_back({{"round", history_.size()}});
                std::this_thread::sleep_for(std::chrono::seconds(2));
                bus_->Publish(reply);
            }
        }).detach();
    }
};

} // namespace cortex
