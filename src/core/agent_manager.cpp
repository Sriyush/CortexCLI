#include <cortex/core/agent_manager.hpp>
#include <cortex/agents/base_agent.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
#include <cortex/llm/llm_client.hpp>
#include <cortex/memory/memory_manager.hpp>
#include <cortex/tools/file_tools.hpp>
#include <cortex/tools/shell_tools.hpp>
#include <cortex/agents/research_agent.hpp>
#include <cortex/agents/coding_agent.hpp>
#include <cortex/agents/critic_agent.hpp>
#include <cortex/agents/orchestrator_agent.hpp>
#include <cortex/llm/ollama_client.hpp>
#include <cortex/llm/remote_client.hpp>
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
                std::shared_ptr<LLMClient> agent_llm = llm_;
                if (!snapshot.llm_provider.empty() && !snapshot.llm_model.empty()) {
                    if (snapshot.llm_provider == "ollama") {
                        Config config;
                        memory_->LoadConfig(config);
                        agent_llm = std::make_shared<OllamaClient>(config.ollama_url);
                        agent_llm->LoadModel(snapshot.llm_model);
                    } else {
                        RemoteProvider rp = RemoteClient::StringToProvider(snapshot.llm_provider);
                        if (rp != RemoteProvider::Unknown) {
                            Config config;
                            memory_->LoadConfig(config);
                            std::string key = "";
                            if (snapshot.llm_provider == "openai") key = config.openai_key;
                            else if (snapshot.llm_provider == "gemini") key = config.gemini_key;
                            else if (snapshot.llm_provider == "claude") key = config.claude_key;
                            
                            if (!key.empty()) {
                                agent_llm = std::make_shared<RemoteClient>(rp, key);
                                agent_llm->LoadModel(snapshot.llm_model);
                            } else {
                                std::cerr << "[AgentManager] WARNING: No API key found for agent '" << name 
                                          << "' (provider: " << snapshot.llm_provider 
                                          << "). Run 'cortex auth -p " << snapshot.llm_provider 
                                          << " -k YOUR_KEY' to fix. Falling back to default LLM.\n";
                            }
                        }
                    }
                }

                if (snapshot.type == "researcher") {
                    agent = std::make_shared<ResearchAgent>(snapshot.name, bus_, agent_llm);
                } else if (snapshot.type == "coder") {
                    agent = std::make_shared<CodingAgent>(snapshot.name, bus_, agent_llm);
                } else if (snapshot.type == "critic") {
                    agent = std::make_shared<CriticAgent>(snapshot.name, bus_, agent_llm);
                } else if (snapshot.type == "orchestrator") {
                    agent = std::make_shared<OrchestratorAgent>(snapshot.name, bus_, agent_llm);
                } else {
                    agent = std::make_shared<BaseAgent>(snapshot.name, snapshot.type, bus_, agent_llm);
                }

                if (snapshot.llm_model.empty()) {
                   auto models = agent_llm->ListModels();
                   if (!models.empty()) {
                       agent_llm->LoadModel(models[0]);
                   }
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

std::shared_ptr<Agent> AgentManager::CreateAgent(const std::string& name, const std::string& type, 
                                                const std::string& provider, const std::string& model) {
    std::cout << "[AgentManager] Creating agent '" << name << "' of type '" << type << "'\n";
    if (!provider.empty()) {
        std::cout << "[AgentManager] Agent '" << name << "' will use provider: " << provider << ", model: " << model << "\n";
    }
    
    std::shared_ptr<LLMClient> agent_llm = llm_;
    if (!provider.empty() && !model.empty()) {
        if (provider == "ollama") {
            Config config;
            memory_->LoadConfig(config);
            agent_llm = std::make_shared<OllamaClient>(config.ollama_url);
            agent_llm->LoadModel(model);
        } else {
            RemoteProvider rp = RemoteClient::StringToProvider(provider);
            if (rp != RemoteProvider::Unknown) {
                Config config;
                memory_->LoadConfig(config);
                std::string key = "";
                if (provider == "openai") key = config.openai_key;
                else if (provider == "gemini") key = config.gemini_key;
                else if (provider == "claude") key = config.claude_key;
                
                if (!key.empty()) {
                    agent_llm = std::make_shared<RemoteClient>(rp, key);
                    agent_llm->LoadModel(model);
                }
            }
        }
    }

    std::shared_ptr<BaseAgent> agent;
    if (type == "researcher") {
        agent = std::make_shared<ResearchAgent>(name, bus_, agent_llm);
    } else if (type == "coder") {
        agent = std::make_shared<CodingAgent>(name, bus_, agent_llm);
    } else if (type == "critic") {
        agent = std::make_shared<CriticAgent>(name, bus_, agent_llm);
    } else if (type == "orchestrator") {
        agent = std::make_shared<OrchestratorAgent>(name, bus_, agent_llm);
    } else {
        agent = std::make_shared<BaseAgent>(name, type, bus_, agent_llm);
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
        snapshot.llm_provider = provider;
        snapshot.llm_model = model;
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
    if (memory_) {
        memory_->DeleteAgent(name);
    }
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
