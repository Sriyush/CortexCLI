#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

namespace cortex {

struct AgentSnapshot {
    std::string name;
    std::string type;
    std::string state;
    std::string llm_provider;
    std::string llm_model;
    nlohmann::json history;
};

struct Config {
    std::string default_provider = "ollama";
    std::string openai_key;
    std::string gemini_key;
    std::string claude_key;
    std::string ollama_url = "http://localhost:11434";
};

class MemoryManager {
public:
    MemoryManager(const std::string& db_path = "cortex.db");
    ~MemoryManager();

    bool Initialize();
    
    bool SaveAgent(const AgentSnapshot& snapshot);
    bool LoadAgent(const std::string& name, AgentSnapshot& snapshot);
    bool DeleteAgent(const std::string& name);
    std::vector<std::string> GetAllAgentNames();
    
    bool SaveMessage(const std::string& channel, const nlohmann::json& message);

    bool SaveConfig(const Config& config);
    bool LoadConfig(Config& config);

    sqlite3* GetDB() const { return db_; }

private:
    sqlite3* db_;
    std::string db_path_;
};

} // namespace cortex
