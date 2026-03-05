#include "memory_manager.hpp"
#include <iostream>

namespace cortex {

MemoryManager::MemoryManager(const std::string& db_path) : db_(nullptr), db_path_(db_path) {}

MemoryManager::~MemoryManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool MemoryManager::Initialize() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc) {
        std::cerr << "[MemoryManager] Can't open database: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS agents ("
        "  name TEXT PRIMARY KEY,"
        "  type TEXT,"
        "  state TEXT,"
        "  history TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  channel TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  payload TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS config ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ");";

    char* zErrMsg = 0;
    rc = sqlite3_exec(db_, sql, nullptr, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[MemoryManager] SQL error: " << zErrMsg << "\n";
        sqlite3_free(zErrMsg);
        return false;
    }
    std::cout << "[MemoryManager] Primary tables ensured.\n";
    
    // Add columns if they don't exist (SQLite doesn't support IF NOT EXISTS for ADD COLUMN)
    const char* alter_sql1 = "ALTER TABLE agents ADD COLUMN llm_provider TEXT;";
    const char* alter_sql2 = "ALTER TABLE agents ADD COLUMN llm_model TEXT;";
    
    sqlite3_exec(db_, alter_sql1, nullptr, 0, nullptr); // Ignore errors (e.g. if column exists)
    sqlite3_exec(db_, alter_sql2, nullptr, 0, nullptr); // Ignore errors
    
    std::cout << "[MemoryManager] Initialized database at: " << db_path_ << "\n";
    return true;
}

bool MemoryManager::SaveAgent(const AgentSnapshot& snapshot) {
    const char* sql = "INSERT OR REPLACE INTO agents (name, type, state, llm_provider, llm_model, history) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    sqlite3_bind_text(stmt, 1, snapshot.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, snapshot.type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, snapshot.state.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, snapshot.llm_provider.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, snapshot.llm_model.c_str(), -1, SQLITE_STATIC);
    std::string history_str = snapshot.history.dump();
    sqlite3_bind_text(stmt, 6, history_str.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool MemoryManager::LoadAgent(const std::string& name, AgentSnapshot& snapshot) {
    const char* sql = "SELECT name, type, state, llm_provider, llm_model, history FROM agents WHERE name = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        snapshot.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        snapshot.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        snapshot.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* prov = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* mod = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        snapshot.llm_provider = prov ? prov : "";
        snapshot.llm_model = mod ? mod : "";
        snapshot.history = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool MemoryManager::DeleteAgent(const std::string& name) {
    const char* sql = "DELETE FROM agents WHERE name = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<std::string> MemoryManager::GetAllAgentNames() {
    std::vector<std::string> names;
    const char* sql = "SELECT name FROM agents;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        names.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    
    sqlite3_finalize(stmt);
    return names;
}

bool MemoryManager::SaveMessage(const std::string& channel, const nlohmann::json& message) {
    const char* sql = "INSERT INTO messages (channel, payload) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, channel.c_str(), -1, SQLITE_STATIC);
    std::string payload = message.dump();
    sqlite3_bind_text(stmt, 2, payload.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool MemoryManager::SaveConfig(const Config& config) {
    const char* sql = "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    
    auto save_key = [&](const std::string& k, const std::string& v) {
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, k.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, v.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    };

    save_key("default_provider", config.default_provider);
    save_key("openai_key", config.openai_key);
    save_key("gemini_key", config.gemini_key);
    save_key("claude_key", config.claude_key);
    save_key("ollama_url", config.ollama_url);
    
    return true;
}

bool MemoryManager::LoadConfig(Config& config) {
    const char* sql = "SELECT value FROM config WHERE key = ?;";
    sqlite3_stmt* stmt;

    auto load_key = [&](const std::string& k, std::string& v) {
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, k.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    };

    load_key("default_provider", config.default_provider);
    load_key("openai_key", config.openai_key);
    load_key("gemini_key", config.gemini_key);
    load_key("claude_key", config.claude_key);
    load_key("ollama_url", config.ollama_url);

    return true;
}

} // namespace cortex
