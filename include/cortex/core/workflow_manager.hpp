#pragma once

#include <cortex/core/agent_manager.hpp>
#include <cortex/messaging/message.hpp>
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace cortex {

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED
};

struct AgentTask {
    std::string task_id;
    std::string agent_name;
    std::string description;
    TaskStatus status = TaskStatus::PENDING;
    std::string output;
};

class WorkflowManager {
public:
    WorkflowManager(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus);
    ~WorkflowManager();

    /**
     * @brief Initiates a team workflow for a specific goal.
     * @param goal The high-level goal to achieve.
     * @param agent_names List of agents to participate in the team.
     */
    void StartWorkflow(const std::string& goal, const std::vector<std::string>& agent_names);
    
    /**
     * @brief Aborts the current workflow.
     */
    void AbortWorkflow();

    /**
     * @brief Check if a workflow is currently running.
     */
    bool IsActive() const { return is_active_; }

private:
    void handle_task_update(const Message& msg);
    void move_to_next_phase();

    AgentManager& agent_manager_;
    std::shared_ptr<MessageBus> bus_;
    
    std::string current_goal_;
    std::vector<std::string> team_agents_;
    std::vector<AgentTask> tasks_;
    
    int current_phase_ = 0; // 0: Research, 1: Implementation, 2: Review
    bool is_active_ = false;
};

} // namespace cortex
