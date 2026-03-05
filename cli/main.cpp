#include <iostream>
#include <string>
#include <memory>
#include <CLI/CLI.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "agent_manager.hpp"
#include "zeromq_bus.hpp"
#include "llama_client.hpp"
#include "memory_manager.hpp"
#include "debate_manager.hpp"
#include "dashboard.hpp"

using namespace cortex;

void StartDashboard(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus) {
    Dashboard dashboard(agent_manager, bus);
    dashboard.Run();
}

int main(int argc, char** argv) {
    // using namespace cortex; // removed

    // Initialize core components
    auto message_bus = std::make_shared<ZeroMQBus>();
    auto llm_client = std::make_shared<LlamaClient>();
    auto memory_manager = std::make_shared<MemoryManager>();
    
    if (!memory_manager->Initialize()) {
        std::cerr << "Failed to initialize memory manager.\n";
        return 1;
    }

    auto agent_manager = std::make_unique<AgentManager>(message_bus, llm_client, memory_manager);
    auto debate_manager = std::make_unique<DebateManager>(*agent_manager, message_bus);

    std::string bus_endpoint = "ipc:///tmp/cortex_bus";

    CLI::App app{"Cortex CLI - A Terminal Operating Environment for AI Agents"};

    bool start_dashboard = false;
    app.add_flag("-d,--dashboard", start_dashboard, "Start the interactive TUI dashboard");

    std::string model_path;
    app.add_option("-m,--model", model_path, "Path to the LLM model file (GGUF)");

    // -------------------------------------------------------------------------
    // "agent" subcommand
    // -------------------------------------------------------------------------
    auto agent_cmd = app.add_subcommand("agent", "Manage agents");
    
    // "agent create"
    auto agent_create_cmd = agent_cmd->add_subcommand("create", "Create a new agent");
    std::string create_agent_name;
    agent_create_cmd->add_option("name", create_agent_name, "Agent name to create");

    // "agent list"
    auto agent_list_cmd = agent_cmd->add_subcommand("list", "List all agents");

    // "agent start"
    auto agent_start_cmd = agent_cmd->add_subcommand("start", "Start an agent");
    std::string start_agent_name;
    agent_start_cmd->add_option("name", start_agent_name, "Agent name to start");

    // "agent stop"
    auto agent_stop_cmd = agent_cmd->add_subcommand("stop", "Stop an agent");
    std::string stop_agent_name;
    agent_stop_cmd->add_option("name", stop_agent_name, "Agent name to stop");

    // -------------------------------------------------------------------------
    // Top-level subcommands
    // -------------------------------------------------------------------------
    auto run_cmd = app.add_subcommand("run", "Run a task");
    std::string task_desc;
    run_cmd->add_option("task", task_desc, "Task description");

    auto debate = app.add_subcommand("debate", "Start a debate between agents");
    std::string debate_topic;
    std::vector<std::string> participants;
    debate->add_option("--topic", debate_topic, "The topic to debate")->required();
    debate->add_option("-p,--participants", participants, "Agents to participate in the debate");

    auto logs_cmd = app.add_subcommand("logs", "View logs for an agent");
    std::string logs_agent_name;
    logs_cmd->add_option("agent", logs_agent_name, "Agent name to view logs for");

    CLI11_PARSE(app, argc, argv);

    // Initial load of model if provided
    if (!model_path.empty()) {
        llm_client->LoadModel(model_path);
    }

    // If dashboard flag is passed
    if (start_dashboard) {
        message_bus->Connect(bus_endpoint, true);
        StartDashboard(*agent_manager, message_bus);
        return 0;
    }

    // Default connection for CLI commands
    message_bus->Connect(bus_endpoint, false);

    // Handle 'agent' subcommand hierarchy
    if (app.got_subcommand(agent_cmd)) {
        if (agent_cmd->got_subcommand(agent_create_cmd)) {
            std::cout << "[Core] Creating agent: " << (create_agent_name.empty() ? "unknown" : create_agent_name) << "\n";
            agent_manager->CreateAgent(create_agent_name, "generic");
        } else if (agent_cmd->got_subcommand(agent_list_cmd)) {
            auto agents = agent_manager->GetAllAgents();
            std::cout << "[Core] Listing " << agents.size() << " agents:\n";
            for (const auto& agent : agents) {
                std::cout << " - " << agent->GetName() << " [" << agent->GetType() << "]\n";
            }
        } else if (agent_cmd->got_subcommand(agent_start_cmd)) {
            std::cout << "[Core] Starting agent: " << (start_agent_name.empty() ? "unknown" : start_agent_name) << "\n";
            agent_manager->StartAgent(start_agent_name);
        } else if (agent_cmd->got_subcommand(agent_stop_cmd)) {
            std::cout << "[Core] Stopping agent: " << (stop_agent_name.empty() ? "unknown" : stop_agent_name) << "\n";
            agent_manager->StopAgent(stop_agent_name);
        } else {
            std::cout << agent_cmd->help();
        }
        return 0;
    }

    // Handle 'run' subcommand
    if (app.got_subcommand(run_cmd)) {
        std::cout << "[Core] Running task: " << (task_desc.empty() ? "unknown" : task_desc) << "\n";
        return 0;
    }

    // Handle 'debate' subcommand
    if (app.got_subcommand(debate)) {
        if (participants.empty()) {
            std::cout << "Error: No participants specified for the debate.\n";
            return 0;
        }
        // Give ZeroMQ a moment to establish connection
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        debate_manager->StartDebate(debate_topic, participants);
        // Wait to ensure ZeroMQ sends the message before the process exits
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 0;
    }

    // Handle 'logs' subcommand
    if (app.got_subcommand(logs_cmd)) {
        std::cout << "[Core] Displaying logs for: " << (logs_agent_name.empty() ? "unknown" : logs_agent_name) << "\n";
        return 0;
    }

    // Default target: if no arguments were provided or no flags triggering returns
    if (argc == 1) {
        std::cout << app.help();
    }

    return 0;
}
