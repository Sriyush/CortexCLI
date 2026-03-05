#include "debate_manager.hpp"
#include "zeromq_bus.hpp"
#include <iostream>
#include <ctime>

namespace cortex {

DebateManager::DebateManager(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus)
    : agent_manager_(agent_manager), bus_(bus) {}

void DebateManager::StartDebate(const std::string& topic, const std::vector<std::string>& agent_names) {
    std::cout << "[DebateManager] Starting debate on: " << topic << "\n";
    std::cout << "[DebateManager] Participants: ";
    for (const auto& name : agent_names) std::cout << name << " ";
    std::cout << "\n";

    // Build the initial message
    Message start_msg;
    start_msg.header.msg_id = "debate_init_" + std::to_string(std::time(nullptr));
    start_msg.header.sender = "DebateManager";
    start_msg.header.type = "DEBATE_START";
    
    start_msg.payload.action = "debate_start";
    start_msg.payload.params = {
        {"topic", topic},
        {"participants", agent_names}
    };

    // Publish the start message
    bus_->Publish(start_msg);

    // In a real implementation, this would involve waiting for agent responses
    // and orchestrating multiple rounds of discussion.
    // For the foundation, we just broadcast the intention.
    
    std::cout << "[DebateManager] Debate initiated successfully.\n";
}

void DebateManager::StopDebate() {
    std::cout << "[DebateManager] Stopping all active debates.\n";
    
    Message stop_msg;
    stop_msg.header.msg_id = "debate_stop_" + std::to_string(std::time(nullptr));
    stop_msg.header.sender = "DebateManager";
    stop_msg.header.type = "DEBATE_STOP";
    stop_msg.payload.action = "debate_stop";
    stop_msg.payload.content = "All agents have been notified to stop debating.";
    
    bus_->Publish(stop_msg);
    std::cout << "[DebateManager] Debate stop signal sent.\n";
}

} // namespace cortex
