#pragma once
#include <atomic>
#include <string>

namespace cortex {

struct SystemStats {
    int64_t total_tokens = 0;
    int32_t running_agents = 0;
    int32_t active_tasks = 0;
    std::string memory_usage = "N/A";
};

class StatsManager {
public:
    static StatsManager& GetInstance() {
        static StatsManager instance;
        return instance;
    }

    void AddTokens(int64_t tokens) { total_tokens_ += tokens; }
    int64_t GetTotalTokens() const { return total_tokens_.load(); }

    void SetRunningAgents(int32_t count) { running_agents_ = count; }
    int32_t GetRunningAgents() const { return running_agents_.load(); }

    void SetActiveTasks(int32_t count) { active_tasks_ = count; }
    int32_t GetActiveTasks() const { return active_tasks_.load(); }

    static std::string GetMemoryUsage();

private:
    StatsManager() = default;
    std::atomic<int64_t> total_tokens_{0};
    std::atomic<int32_t> running_agents_{0};
    std::atomic<int32_t> active_tasks_{0};
};

} // namespace cortex
