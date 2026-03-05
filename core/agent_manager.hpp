#pragma once

#include "agent.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>

namespace cortex {

class MessageBus;
class LLMClient;
class MemoryManager;

class AgentManager {
public:
    AgentManager(std::shared_ptr<MessageBus> bus, 
                 std::shared_ptr<LLMClient> llm,
                 std::shared_ptr<MemoryManager> memory);
    ~AgentManager();

    // Prevent copying
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;

    // Create an agent of a specific type
    std::shared_ptr<Agent> CreateAgent(const std::string& name, const std::string& type);
    
    // Retrieve an agent by name
    std::shared_ptr<Agent> GetAgent(const std::string& name) const;

    // Remove an agent
    bool RemoveAgent(const std::string& name);

    // Get all agents
    std::vector<std::shared_ptr<Agent>> GetAllAgents() const;

    // Start/Stop agents
    bool StartAgent(const std::string& name);
    bool StopAgent(const std::string& name);

private:
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<LLMClient> llm_;
    std::shared_ptr<MemoryManager> memory_;
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
};

} // namespace cortex
