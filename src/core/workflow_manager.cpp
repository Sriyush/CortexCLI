#include <cortex/core/workflow_manager.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
#include <iostream>
#include <algorithm>

namespace cortex {

WorkflowManager::WorkflowManager(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus)
    : agent_manager_(agent_manager), bus_(bus) {
    
    // Subscribe to task updates via signals
    bus_->Subscribe("signal", [this](const Message& msg) {
        if (msg.payload.action == "TASK_UPDATE") {
            this->handle_task_update(msg);
        }
    });
}

WorkflowManager::~WorkflowManager() {}

void WorkflowManager::StartWorkflow(const std::string& goal, const std::vector<std::string>& agent_names) {
    current_goal_ = goal;
    team_agents_ = agent_names;
    is_active_ = true;
    current_phase_ = 0;
    tasks_.clear();

    std::cout << "[WorkflowManager] Starting workflow: " << goal << "\n";
    
    Message status_msg = Message::Create("WorkflowManager", "signal", "workflow_status", 
                                        "Starting workflow: " + goal);
    bus_->Publish(status_msg);

    move_to_next_phase();
}

void WorkflowManager::AbortWorkflow() {
    is_active_ = false;
    std::cout << "[WorkflowManager] Workflow aborted.\n";
    
    Message abort_msg = Message::Create("WorkflowManager", "signal", "workflow_abort", "Workflow was cancelled by user.");
    bus_->Publish(abort_msg);
}

void WorkflowManager::handle_task_update(const Message& msg) {
    if (!is_active_) return;

    std::string task_id = msg.payload.params.value("task_id", "");
    std::string status_str = msg.payload.status;

    auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const AgentTask& t) {
        return t.task_id == task_id;
    });

    if (it != tasks_.end()) {
        if (status_str == "success") it->status = TaskStatus::COMPLETED;
        else if (status_str == "error") it->status = TaskStatus::FAILED;
        
        it->output = msg.payload.content.dump();
        
        std::cout << "[WorkflowManager] Task " << task_id << " updated to " << status_str << "\n";
        
        Message status_msg = Message::Create("WorkflowManager", "signal", "task_completed", 
                                            "Task " + task_id + " by " + it->agent_name + " completed.");
        bus_->Publish(status_msg);

        // Check if all tasks in current phase are done
        bool all_done = std::all_of(tasks_.begin(), tasks_.end(), [](const AgentTask& t) {
            return t.status == TaskStatus::COMPLETED;
        });

        if (all_done) {
            move_to_next_phase();
        }
    }
}

void WorkflowManager::move_to_next_phase() {
    if (!is_active_) return;

    if (current_phase_ == 0) {
        // Find a researcher
        std::string researcher = "";
        std::cout << "[WorkflowManager] Phase 0: Searching for researcher in team: ";
        for (const auto& name : team_agents_) {
            std::cout << name << " ";
        }
        std::cout << "\n";

        for (const auto& name : team_agents_) {
            auto agent = agent_manager_.GetAgent(name);
            if (agent) {
                std::cout << "[WorkflowManager] Found agent '" << name << "' of type '" << agent->GetType() << "'\n";
                if (agent->GetType() == "researcher") {
                    researcher = name;
                    break;
                }
            } else {
                std::cout << "[WorkflowManager] Agent '" << name << "' not found in AgentManager.\n";
            }
        }

        if (researcher.empty()) {
            std::cout << "[WorkflowManager] Error: No researcher found in team.\n";
            is_active_ = false;
            return;
        }

        std::cout << "[WorkflowManager] Phase 0: Researching goal...\n";
        AgentTask task;
        task.task_id = "task_research_" + std::to_string(std::time(nullptr));
        task.agent_name = researcher;
        task.description = "Analyze the goal and break it down into implementation sub-tasks: " + current_goal_;
        task.status = TaskStatus::RUNNING;
        tasks_.push_back(task);

        Message msg = Message::Create("WorkflowManager", "signal", "task_assigned", 
                                     task.description, {{"task_id", task.task_id}, {"agent_name", task.agent_name}});
        bus_->Publish(msg);
        Message status_msg = Message::Create("WorkflowManager", "signal", "workflow_status", 
                                            "Phase 0 (Research) complete. Transitioning to Phase 1.");
        bus_->Publish(status_msg);

        current_phase_ = 1;
    } else if (current_phase_ == 1) {
        std::cout << "[WorkflowManager] Phase 1: Implementing sub-tasks...\n";
        // In a real scenario, we'd parse the researcher's output to find sub-tasks.
        // For now, we assign a generic implementation task to all coders.
        tasks_.clear();
        for (const auto& name : team_agents_) {
            auto agent = agent_manager_.GetAgent(name);
            if (agent && agent->GetType() == "coder") {
                AgentTask task;
                task.task_id = "task_impl_" + name + "_" + std::to_string(std::time(nullptr));
                task.agent_name = name;
                task.description = "Implement parts of the goal: " + current_goal_;
                task.status = TaskStatus::RUNNING;
                tasks_.push_back(task);

                Message msg = Message::Create("WorkflowManager", "signal", "task_assigned", 
                                             task.description, {{"task_id", task.task_id}, {"agent_name", task.agent_name}});
                bus_->Publish(msg);
            }
        }
        
        if (tasks_.empty()) {
            std::cout << "[WorkflowManager] Error: No coders found in team.\n";
            is_active_ = false;
            return;
        }
        Message status_msg = Message::Create("WorkflowManager", "signal", "workflow_status", 
                                            "Phase 1 (Implementation) complete. Transitioning to Phase 2.");
        bus_->Publish(status_msg);

        current_phase_ = 2;
    } else if (current_phase_ == 2) {
        std::cout << "[WorkflowManager] Phase 2: Reviewing work...\n";
        tasks_.clear();
        for (const auto& name : team_agents_) {
            auto agent = agent_manager_.GetAgent(name);
            if (agent && agent->GetType() == "critic") {
                AgentTask task;
                task.task_id = "task_review_" + name + "_" + std::to_string(std::time(nullptr));
                task.agent_name = name;
                task.description = "Review and test the implementation for: " + current_goal_;
                task.status = TaskStatus::RUNNING;
                tasks_.push_back(task);

                Message msg = Message::Create("WorkflowManager", "signal", "task_assigned", 
                                             task.description, {{"task_id", task.task_id}, {"agent_name", task.agent_name}});
                bus_->Publish(msg);
            }
        }
        
        if (tasks_.empty()) {
            std::cout << "[WorkflowManager] No critics found. Workflow complete.\n";
            is_active_ = false;
        } else {
            current_phase_ = 3;
        }
    } else {
        std::cout << "[WorkflowManager] Workflow complete!\n";
        is_active_ = false;
    }
}

} // namespace cortex
