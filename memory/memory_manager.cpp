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
        ");";

    char* zErrMsg = 0;
    rc = sqlite3_exec(db_, sql, nullptr, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[MemoryManager] SQL error: " << zErrMsg << "\n";
        sqlite3_free(zErrMsg);
        return false;
    }

    std::cout << "[MemoryManager] Initialized database at: " << db_path_ << "\n";
    return true;
}

bool MemoryManager::SaveAgent(const AgentSnapshot& snapshot) {
    const char* sql = "INSERT OR REPLACE INTO agents (name, type, state, history) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    sqlite3_bind_text(stmt, 1, snapshot.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, snapshot.type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, snapshot.state.c_str(), -1, SQLITE_STATIC);
    std::string history_str = snapshot.history.dump();
    sqlite3_bind_text(stmt, 4, history_str.c_str(), -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool MemoryManager::LoadAgent(const std::string& name, AgentSnapshot& snapshot) {
    const char* sql = "SELECT name, type, state, history FROM agents WHERE name = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        snapshot.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        snapshot.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        snapshot.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        snapshot.history = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
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

} // namespace cortex
