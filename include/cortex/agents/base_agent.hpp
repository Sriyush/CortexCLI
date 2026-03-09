#pragma once

#include <cortex/core/agent.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
#include <cortex/llm/llm_client.hpp>
#include <cortex/tools/tool.hpp>
#include <cortex/core/logger.hpp>
#include <cortex/core/stats_manager.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <chrono>

namespace cortex {

class BaseAgent : public Agent {
public:
    BaseAgent(const std::string& name, const std::string& type, 
              std::shared_ptr<MessageBus> bus, 
              std::shared_ptr<LLMClient> llm)
        : name_(name), type_(type), bus_(bus), llm_(llm), state_(AgentState::IDLE), 
          is_participating_(false) {}

    std::string GetName() const override { return name_; }
    std::string GetType() const override { return type_; }
    AgentState GetState() const override { return state_; }

    void Start() override {
        state_ = AgentState::RUNNING;
        std::cout << "[Agent:" << name_ << "] Started.\n";
        
        // Subscribe to messages directed at this agent or general signals
        bus_->Subscribe(name_, [this](const Message& msg) {
            this->on_message_received(msg);
        });

        // Participate in debates or workflows (signals)
        bus_->Subscribe("signal", [this](const Message& msg) {
            if (msg.payload.action == "debate_start") {
                this->on_debate_start(msg);
            } else if (msg.payload.action == "debate_stop") {
                this->is_participating_ = false;
                std::cout << "[Agent:" << name_ << "] Debate stopped by manager.\n";
            } else if (msg.payload.action == "task_assigned") {
                this->on_task_assigned(msg);
            }
        });

        // Custom debate turn topic for routing efficiency
        bus_->Subscribe("request", [this](const Message& msg) {
            if (msg.payload.action == "speak") {
                this->on_debate_turn(msg);
            }
        });
    }

    void Stop() override {
        state_ = AgentState::STOPPED;
        std::cout << "[Agent:" << name_ << "] Stopped.\n";
    }

    void Pause() override { state_ = AgentState::PAUSED; }
    void Resume() override { state_ = AgentState::RUNNING; }

    void Tick() override {
        if (state_ != AgentState::RUNNING) return;
        
        // The Tick logic will handle periodic background tasks if needed
    }

    void RegisterTool(std::shared_ptr<Tool> tool) {
        tools_[tool->GetName()] = tool;
    }

    virtual std::string ExecuteTool(const std::string& tool_name, const nlohmann::json& args) {
        auto it = tools_.find(tool_name);
        if (it != tools_.end()) {
            std::cout << "[Agent:" << name_ << "] Executing tool: " << tool_name << "\n";
            return it->second->Execute(args);
        }
        return "Error: Tool '" + tool_name + "' not found.";
    }

protected:
    std::string name_;
    std::string type_;
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<LLMClient> llm_;
    AgentState state_;
    bool is_participating_;
    std::map<std::string, std::shared_ptr<Tool>> tools_;
    std::vector<nlohmann::json> history_;
    std::string system_prompt_;

    virtual void on_message_received(const Message& msg) {
        std::cout << "[Agent:" << name_ << "] Message received: " << msg.payload.action << "\n";
    }

    virtual void on_debate_start(const Message& msg) {
        // Check if we are a participant
        if (msg.payload.params.contains("participants")) {
            bool participating = false;
            for (const auto& p : msg.payload.params["participants"]) {
                std::string p_str = p.get<std::string>();
                std::string name_lower = name_;
                std::string p_lower = p_str;
                for (auto &c : name_lower) c = std::tolower(c);
                for (auto &c : p_lower) c = std::tolower(c);
                if (p_lower == name_lower) { participating = true; break; }
            }
            is_participating_ = participating;
            if (!is_participating_) return;

            std::string topic = msg.payload.params.value("topic", "unknown");
            std::cout << "[Agent:" << name_ << "] Participating in debate: " << topic << "\n";
            
            // Send an introductory message
            Message intro = Message::Create(
                name_,
                "request",
                "speak",
                "I am " + name_ + " and I am ready to discuss topic: " + topic
            );
            
            std::cout << "[Agent:" << name_ << "] Sending intro message...\n";
            bus_->Publish(intro);
        }
    }

    virtual void on_task_assigned(const Message& msg) {
        std::string target_agent = msg.payload.params.value("agent_name", "");
        if (!target_agent.empty()) {
            std::string n_lower = name_;
            std::string t_lower = target_agent;
            for (auto &c : n_lower) c = std::tolower(c);
            for (auto &c : t_lower) c = std::tolower(c);
            if (n_lower != t_lower) return;
        }

        if (state_ == AgentState::BUSY) {
            std::cout << "[Agent:" << name_ << "] Warning: Agent is already busy. Ignoring new task.\n";
            return;
        }

        std::string task_id = msg.payload.params.value("task_id", "unknown");
        std::string description = msg.payload.content.get<std::string>();
        
        std::cout << "[Agent:" << name_ << "] Task assigned (" << task_id << "): " << description << "\n";
        
        state_ = AgentState::BUSY;
        // Run assignment in background
        std::thread([this, task_id, description]() {
            std::string system_instructions = system_prompt_;
            if (system_instructions.empty()) {
                system_instructions = 
                    "### INSTRUCTIONS\n"
                    "Objective: " + description + "\n\n"
                    "CRITICAL RULES:\n"
                    "- Answer the question DIRECTLY. Output ONLY the answer, nothing else.\n"
                    "- Do NOT describe tools, do NOT explain steps, do NOT write meta-commentary.\n"
                    "- Do NOT output JSON unless you are making a real tool call.\n"
                    "- Only use tools if the task REQUIRES reading/writing files or running commands.\n"
                    "- For knowledge questions (facts, code, math, writing), answer immediately.\n\n"
                    "Tool format (ONLY when needed): ```json\n{\"tool\": \"name\", \"args\": {}}\n```\n"
                    "Answer:";
            }

            std::string conversation_history = "";
            std::string final_output = "";
            int max_turns = 10;

            for (int turn = 0; turn < max_turns; ++turn) {
                std::string full_prompt = system_instructions + "\n" + conversation_history;
                GenerationResult result = llm_->Generate(full_prompt, {});
                StatsManager::GetInstance().AddTokens(result.prompt_tokens + result.completion_tokens);
                
                std::string response = result.text;
                Logger::GetInstance().Log(name_, "Turn " + std::to_string(turn) + " Response: " + response);
                
                // Detect REAL tool calls (handle ```json, ``` or raw {)
                size_t json_start = response.find("```json");
                size_t json_end = std::string::npos;
                std::string json_str = "";

                if (json_start != std::string::npos) {
                    json_end = response.find("```", json_start + 7);
                    if (json_end != std::string::npos) {
                        json_str = response.substr(json_start + 7, json_end - (json_start + 7));
                    }
                } else if ((json_start = response.find("{")) != std::string::npos) {
                    // Look for a JSON-like block starting with { and ending with }
                    size_t last_bracket = response.find_last_of("}");
                    if (last_bracket != std::string::npos && last_bracket > json_start) {
                        json_str = response.substr(json_start, last_bracket - json_start + 1);
                    }
                }

                if (!json_str.empty()) {
                    try {
                        auto j = nlohmann::json::parse(json_str);
                        if (j.contains("tool") && j.contains("args")) {
                            std::string tool_name = j["tool"].get<std::string>();
                            if (tool_name == "read_file" || tool_name == "write_file" || tool_name == "run_shell") {
                                Logger::GetInstance().LogToolCall(name_, tool_name, j["args"].dump());
                                std::string tool_result = this->ExecuteTool(j["tool"], j["args"]);
                                Logger::GetInstance().LogToolResult(name_, tool_name, tool_result);
                                conversation_history += "\nAssistant: " + response + "\nResult: " + tool_result + "\nAnswer:";
                                continue; 
                            }
                        }
                    } catch (...) {
                        // Not valid JSON, treat as normal text
                    }
                }

                // This is the final answer - clean it up
                final_output = response;
                
                // Strip TASK_COMPLETE prefix
                if (final_output.find("TASK_COMPLETE:") == 0) final_output = final_output.substr(14);
                else if (final_output.find("TASK_COMPLETE: ") == 0) final_output = final_output.substr(15);
                
                // Strip any hallucinated JSON tool blocks from the output
                size_t block_start = final_output.find("```json");
                if (block_start != std::string::npos) {
                    size_t block_end = final_output.find("```", block_start + 7);
                    if (block_end != std::string::npos) {
                        // Remove the entire JSON block
                        final_output = final_output.substr(0, block_start) + final_output.substr(block_end + 3);
                    }
                }
                
                // Trim
                final_output.erase(0, final_output.find_first_not_of(" \t\n\r"));
                final_output.erase(final_output.find_last_not_of(" \t\n\r") + 1);
                break;
            }

            // Report completion back to WorkflowManager
            Message update = Message::Create(name_, "signal", "TASK_UPDATE", final_output, {{"task_id", task_id}});
            bus_->Publish(update);
            state_ = AgentState::RUNNING;
        }).detach();
    }

    virtual void on_debate_turn(const Message& msg) {
        if (!is_participating_) return;
        if (msg.header.sender == name_) return; // Don't respond to self

        // Run reasoning in a background thread to avoid blocking the MessageBus
        std::thread([this, msg]() {
            // Check history to avoid infinite loops in mock
            if (history_.size() < 3) {
                // Construct a prompt for the LLM
                std::string sender = msg.header.sender;
                std::string content = msg.payload.content.get<std::string>();
                std::string prompt = "You are " + name_ + ", an AI agent participating in a debate.\n"
                                     "Context: " + sender + " said: \"" + content + "\"\n"
                                     "Provide a concise and critical response to their point.";

                // Generate response using the integrated AI engine
                GenerationResult result = llm_->Generate(prompt, {});
  
                if (!is_participating_) {
                    std::cout << "[Agent:" << name_ << "] Skipping response because debate stopped.\n";
                    return;
                }

                // Report tokens
                StatsManager::GetInstance().AddTokens(result.prompt_tokens + result.completion_tokens);
  
                Message reply = Message::Create(
                    name_,
                    "request",
                    "speak",
                    result.text
                );
                
                // Add to local history to track rounds
                history_.push_back({{"round", history_.size()}});
                
                // Sleep a bit to simulate thinking and avoid flood
                std::this_thread::sleep_for(std::chrono::seconds(2));
                bus_->Publish(reply);
            } else {
                std::cout << "[Agent:" << name_ << "] Maximum debate rounds reached.\n";
            }
        }).detach();
    }

};

} // namespace cortex
