#include <cortex/core/debate_manager.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
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

    // Build the initial message using the new factory
    Message start_msg = Message::Create(
        "DebateManager", 
        "signal", 
        "debate_start", 
        nullptr, 
        {{"topic", topic}, {"participants", agent_names}}
    );

    // Publish the start message
    bus_->Publish(start_msg);
    
    std::cout << "[DebateManager] Debate initiated successfully.\n";
}

void DebateManager::StopDebate() {
    std::cout << "[DebateManager] Stopping all active debates.\n";
    
    Message stop_msg = Message::Create(
        "DebateManager",
        "signal",
        "debate_stop",
        "All agents have been notified to stop debating."
    );
    
    bus_->Publish(stop_msg);
    std::cout << "[DebateManager] Debate stop signal sent.\n";
}

} // namespace cortex
