#pragma once

#include <cortex/agents/base_agent.hpp>

namespace cortex {

class OrchestratorAgent : public BaseAgent {
public:
    OrchestratorAgent(const std::string& name, std::shared_ptr<MessageBus> bus, std::shared_ptr<LLMClient> llm)
        : BaseAgent(name, "orchestrator", bus, llm) {
        
        // Orchestrator has a special system prompt for delegation
        system_prompt_ = 
            "### ORCHESTRATOR INSTRUCTIONS\n"
            "Objective: Break down the user's request and delegate tasks to specialized agents.\n"
            "Available Agents: phi (coder), qween (researcher).\n\n"
            "CRITICAL RULES:\n"
            "- Determine if the task needs research, coding, or both.\n"
            "- Delegate tasks by outputting: ```json\n{\"tool\": \"delegate\", \"args\": {\"agent\": \"phi\", \"task\": \"...\"}}\n```\n"
            "- You can also use standard tools: read_file, write_file, run_shell.\n"
            "- After all sub-tasks are done, summarize the result and say TASK_COMPLETE.\n"
            "- NO meta-talk. Be direct.\n\n"
            "Answer:";
    }

    std::string ExecuteTool(const std::string& name, const nlohmann::json& args) override {
        if (name == "delegate") {
            if (!args.contains("agent") || !args.contains("task")) {
                return "Error: Missing 'agent' or 'task' for delegation.";
            }
            std::string target_agent = args["agent"];
            std::string sub_task = args["task"];
            
            Logger::GetInstance().Log(name_, "Delegating to " + target_agent + ": " + sub_task);
            
            // Assign task via message bus
            std::string sub_task_id = "sub_" + std::to_string(std::time(nullptr));
            bool sub_done = false;
            std::string sub_result = "";

            // Temporarily subscribe to signals to catch the sub-task completion
            bus_->Subscribe("signal", [&](const Message& msg) {
                if (msg.payload.action == "TASK_UPDATE") {
                    if (msg.payload.params.contains("task_id") && msg.payload.params["task_id"] == sub_task_id) {
                        sub_result = msg.payload.content.get<std::string>();
                        sub_done = true;
                    }
                }
            });

            Message msg = Message::Create(name_, "signal", "task_assigned", 
                                         sub_task, {{"task_id", sub_task_id}, {"agent_name", target_agent}});
            bus_->Publish(msg);

            // Synchronous wait for sub-task (within the orchestrator's thread)
            int timeout = 0;
            while (!sub_done && timeout < 300) { // 5 min timeout
                std::this_thread::sleep_for(std::chrono::seconds(1));
                timeout++;
            }

            // Unsubscribe
            // Note: Our current ZMQ bus might need an Unsubscribe method. 
            // For now, we'll let it be as this is a background thread.

            if (!sub_done) return "Error: Sub-task delegated to " + target_agent + " timed out.";
            return "Result from " + target_agent + ": " + sub_result;
        }
        
        return BaseAgent::ExecuteTool(name, args);
    }
};

} // namespace cortex
