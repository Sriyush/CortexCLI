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
    nlohmann::json history;
};

class MemoryManager {
public:
    MemoryManager(const std::string& db_path = "cortex.db");
    ~MemoryManager();

    bool Initialize();
    
    bool SaveAgent(const AgentSnapshot& snapshot);
    bool LoadAgent(const std::string& name, AgentSnapshot& snapshot);
    std::vector<std::string> GetAllAgentNames();
    
    bool SaveMessage(const std::string& channel, const nlohmann::json& message);

private:
    sqlite3* db_;
    std::string db_path_;
};

} // namespace cortex
