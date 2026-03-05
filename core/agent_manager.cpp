#include "agent_manager.hpp"
#include "base_agent.hpp"
#include "zeromq_bus.hpp"
#include "llama_client.hpp"
#include "memory_manager.hpp"
#include "file_tools.hpp"
#include "shell_tools.hpp"
#include "research_agent.hpp"
#include "coding_agent.hpp"
#include "critic_agent.hpp"
#include <iostream>

namespace cortex {

AgentManager::AgentManager(std::shared_ptr<MessageBus> bus, 
                           std::shared_ptr<LLMClient> llm,
                           std::shared_ptr<MemoryManager> memory)
    : bus_(bus), llm_(llm), memory_(memory) {
    
    // Load existing agents from database
    if (memory_) {
        auto names = memory_->GetAllAgentNames();
        for (const auto& name : names) {
            AgentSnapshot snapshot;
            if (memory_->LoadAgent(name, snapshot)) {
                std::shared_ptr<BaseAgent> agent;
                if (snapshot.type == "researcher") {
                    agent = std::make_shared<ResearchAgent>(snapshot.name, bus_, llm_);
                } else if (snapshot.type == "coder") {
                    agent = std::make_shared<CodingAgent>(snapshot.name, bus_, llm_);
                } else if (snapshot.type == "critic") {
                    agent = std::make_shared<CriticAgent>(snapshot.name, bus_, llm_);
                } else {
                    agent = std::make_shared<BaseAgent>(snapshot.name, snapshot.type, bus_, llm_);
                }
                
                // Register default tools
                agent->RegisterTool(std::make_shared<ReadFileTool>());
                agent->RegisterTool(std::make_shared<WriteFileTool>());
                agent->RegisterTool(std::make_shared<ShellTool>());
                
                agents_[name] = agent;
            }
        }
    }
}

AgentManager::~AgentManager() {
    // Cleanup
}

std::shared_ptr<Agent> AgentManager::CreateAgent(const std::string& name, const std::string& type) {
    std::cout << "[AgentManager] Creating agent '" << name << "' of type '" << type << "'\n";
    
    std::shared_ptr<BaseAgent> agent;
    if (type == "researcher") {
        agent = std::make_shared<ResearchAgent>(name, bus_, llm_);
    } else if (type == "coder") {
        agent = std::make_shared<CodingAgent>(name, bus_, llm_);
    } else if (type == "critic") {
        agent = std::make_shared<CriticAgent>(name, bus_, llm_);
    } else {
        agent = std::make_shared<BaseAgent>(name, type, bus_, llm_);
    }
    
    // Register default tools
    agent->RegisterTool(std::make_shared<ReadFileTool>());
    agent->RegisterTool(std::make_shared<WriteFileTool>());
    agent->RegisterTool(std::make_shared<ShellTool>());
    
    agents_[name] = agent;

    // Save newly created agent to database
    if (memory_) {
        AgentSnapshot snapshot;
        snapshot.name = name;
        snapshot.type = type;
        snapshot.state = "IDLE";
        snapshot.history = nlohmann::json::array();
        memory_->SaveAgent(snapshot);
    }

    return agent;
}

std::shared_ptr<Agent> AgentManager::GetAgent(const std::string& name) const {
    auto it = agents_.find(name);
    if (it != agents_.end()) {
        return it->second;
    }
    return nullptr;
}

bool AgentManager::RemoveAgent(const std::string& name) {
    return agents_.erase(name) > 0;
}

std::vector<std::shared_ptr<Agent>> AgentManager::GetAllAgents() const {
    std::vector<std::shared_ptr<Agent>> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        result.push_back(agent);
    }
    return result;
}

bool AgentManager::StartAgent(const std::string& name) {
    auto agent = GetAgent(name);
    if (agent) {
        agent->Start();
        return true;
    }
    return false;
}

bool AgentManager::StopAgent(const std::string& name) {
    auto agent = GetAgent(name);
    if (agent) {
        agent->Stop();
        return true;
    }
    return false;
}

} // namespace cortex
