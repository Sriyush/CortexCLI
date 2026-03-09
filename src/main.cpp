#include <iostream>
#include <string>
#include <memory>
#include <CLI/CLI.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cortex/core/agent_manager.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
#include <cortex/llm/llm_client.hpp>
#include <cortex/llm/ollama_client.hpp>
#include <cortex/llm/remote_client.hpp>
#include <cortex/memory/memory_manager.hpp>
#include <cortex/core/debate_manager.hpp>
#include <cortex/core/workflow_manager.hpp>
#include <cortex/tui/dashboard.hpp>
#include <cortex/core/logger.hpp>

using namespace cortex;
//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
void StartDashboard(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus) {
    Dashboard dashboard(agent_manager, bus);
    dashboard.Run();
}

int main(int argc, char** argv) {
    CLI::App app{"Cortex CLI - A Terminal Operating Environment for AI Agents"};

    bool start_dashboard = false;
    app.add_flag("-d,--dashboard", start_dashboard, "Start the interactive TUI dashboard");

    std::string model_name;
    app.add_option("-m,--model", model_name, "Model name (e.g. tinyllama, gpt-4, gemini-1.5-pro)");

    std::string provider_name = "ollama";
    app.add_option("--provider", provider_name, "LLM Provider (ollama, openai, gemini, claude)")
        ->check(CLI::IsMember({"ollama", "openai", "gemini", "claude"}));

    std::string ollama_url = "http://localhost:11434";
    app.add_option("--ollama-url", ollama_url, "Ollama API URL (default: http://localhost:11434)");

    std::string openai_key = "";
    app.add_option("--openai-key", openai_key, "OpenAI API Key (or set OPENAI_API_KEY env)");

    std::string gemini_key = "";
    app.add_option("--gemini-key", gemini_key, "Gemini API Key (or set GEMINI_API_KEY env)");

    std::string claude_key = "";
    app.add_option("--claude-key", claude_key, "Claude API Key (or set CLAUDE_API_KEY env)");

    // ... (rest of subcommands unchanged)
    // -------------------------------------------------------------------------
    // "agent" subcommand
    // -------------------------------------------------------------------------
    auto agent_cmd = app.add_subcommand("agent", "Manage agents");
    // ...
    
    // "agent create"
    auto agent_create_cmd = agent_cmd->add_subcommand("create", "Create a new agent");
    std::string create_agent_name;
    std::string create_agent_type = "generic";
    bool create_agent_ollama = false;
    std::string create_agent_model = "";

    agent_create_cmd->add_option("name", create_agent_name, "Agent name to create")->required();
    agent_create_cmd->add_option("type", create_agent_type, "Agent type (researcher, coder, critic, generic)");
    agent_create_cmd->add_flag("--ollama", create_agent_ollama, "Use Ollama for this agent");
    std::string create_agent_provider = "";
    agent_create_cmd->add_option("--provider", create_agent_provider, "Provider for this agent (ollama, openai, gemini, claude)");
    agent_create_cmd->add_option("-m,--model", create_agent_model, "Specific model to use");

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
 
    // "agent delete"
    auto agent_delete_cmd = agent_cmd->add_subcommand("delete", "Delete an agent");
    std::vector<std::string> delete_agent_names;
    agent_delete_cmd->add_option("name", delete_agent_names, "Agent name(s) to delete");
    agent_delete_cmd->add_option("-p,--participants", delete_agent_names, "Agent name(s) to delete");
 
    // "pdel" as top-level alias for agent delete
    auto pdel_cmd = app.add_subcommand("pdel", "Delete agent(s) (alias for agent delete)");
    pdel_cmd->add_option("name", delete_agent_names, "Agent name(s) to delete");
    pdel_cmd->add_option("-p,--participants", delete_agent_names, "Agent name(s) to delete");

    // -------------------------------------------------------------------------
    // Top-level subcommands
    // -------------------------------------------------------------------------
    auto run_cmd = app.add_subcommand("run", "Run a single task using an agent");
    std::string task_desc;
    std::string run_agent_name;
    std::string run_output_file;
    run_cmd->add_option("task", task_desc, "Task description")->required();
    run_cmd->add_option("-a,--agent", run_agent_name, "Name of the agent to run the task (defaults to first available)");
    run_cmd->add_option("-o,--output", run_output_file, "File to save the output to (optional)");

    auto debate = app.add_subcommand("debate", "Start a debate between agents");
    auto debate_start_cmd = debate->add_subcommand("start", "Start a new debate");
    auto debate_stop_cmd = debate->add_subcommand("stop", "Stop all active debates");
 
    std::string debate_topic;
    std::vector<std::string> participants;
    debate_start_cmd->add_option("--topic", debate_topic, "The topic to debate")->required();
    debate_start_cmd->add_option("-p,--participants", participants, "Agents to participate in the debate");

    auto work_cmd = app.add_subcommand("work", "Start a team-based workflow");
    std::string work_goal;
    std::vector<std::string> work_agents;
    work_cmd->add_option("-g,--goal", work_goal, "The high-level goal")->required();
    work_cmd->add_option("-a,--agents", work_agents, "Agents to participate in the team");

    auto logs_cmd = app.add_subcommand("logs", "View logs for an agent or global activities");
    std::string logs_agent_name;
    int logs_last_n = 50;
    bool clear_logs = false;
    logs_cmd->add_option("agent", logs_agent_name, "Agent name to view logs for (optional)");
    logs_cmd->add_option("-n", logs_last_n, "Number of lines to show (default: 50)");
    logs_cmd->add_flag("--clear", clear_logs, "Clear all logs");

    auto auth_cmd = app.add_subcommand("auth", "Configure LLM providers and API keys");
    std::string auth_provider_name;
    std::string auth_api_key;
    auth_cmd->add_option("-p,--provider", auth_provider_name, "Provider to configure");
    auth_cmd->add_option("-k,--key", auth_api_key, "API key for the provider");
 
    auto model_cmd = app.add_subcommand("model", "Manage LLM models");
    auto model_list_cmd = model_cmd->add_subcommand("list", "List available models");
    auto model_remove_cmd = model_cmd->add_subcommand("remove", "Remove a model");
    model_remove_cmd->alias("rm");
    model_remove_cmd->alias("delete");
    std::string remove_model_name;
    model_remove_cmd->add_option("name", remove_model_name, "Model name to remove")->required();

    CLI11_PARSE(app, argc, argv);

    // Initialize memory manager first to load config
    auto memory_manager = std::make_shared<MemoryManager>();
    if (!memory_manager->Initialize()) {
        std::cerr << "Failed to initialize memory manager.\n";
        return 1;
    }

    Config stored_config;
    memory_manager->LoadConfig(stored_config);

    // Handle 'auth' subcommand
    if (app.got_subcommand(auth_cmd)) {
        Config new_config = stored_config;

        if (!auth_provider_name.empty()) {
            new_config.default_provider = auth_provider_name;
            if (!auth_api_key.empty()) {
                if (auth_provider_name == "openai") new_config.openai_key = auth_api_key;
                else if (auth_provider_name == "gemini") new_config.gemini_key = auth_api_key;
                else if (auth_provider_name == "claude") new_config.claude_key = auth_api_key;
            }
            std::cout << "[Core] Configuration updated for provider: " << auth_provider_name << "\n";
        } else {
            std::cout << "\n--- Cortex Auth Configuration ---\n";
            std::cout << "This will save your provider preferences to " << sqlite3_db_filename(memory_manager->GetDB(), "main") << "\n\n";
            
            std::cout << "Select LLM Provider:\n";
            std::cout << "  1. Ollama (Local)\n";
            std::cout << "  2. OpenAI (Remote API)\n";
            std::cout << "  3. Gemini (Remote API)\n";
            std::cout << "  4. Claude (Remote API)\n";
            std::cout << "Enter choice [1-4] (current: " << stored_config.default_provider << "): " << std::flush;
            
            std::string input;
            std::getline(std::cin, input);
            
            if (!input.empty()) {
                try {
                    int choice = std::stoi(input);
                    if (choice == 1) new_config.default_provider = "ollama";
                    else if (choice == 2) new_config.default_provider = "openai";
                    else if (choice == 3) new_config.default_provider = "gemini";
                    else if (choice == 4) new_config.default_provider = "claude";
                    else std::cout << "Invalid choice, keeping current.\n";
                } catch (...) {
                    std::cout << "Invalid input, keeping current.\n";
                }
            }

            if (new_config.default_provider == "ollama") {
                std::cout << "Enter Ollama URL [default: " << new_config.ollama_url << "]: " << std::flush;
                std::string url;
                std::getline(std::cin, url);
                if (!url.empty()) new_config.ollama_url = url;
            } else {
                std::string current_key = "";
                if (new_config.default_provider == "openai") current_key = new_config.openai_key;
                else if (new_config.default_provider == "gemini") current_key = new_config.gemini_key;
                else if (new_config.default_provider == "claude") current_key = new_config.claude_key;

                std::cout << "Enter API Key for " << new_config.default_provider;
                if (!current_key.empty()) {
                    std::cout << " [current: " << current_key.substr(0, 4) << "..." << current_key.substr(current_key.length() - 4) << "]";
                }
                std::cout << ": " << std::flush;
                
                std::string key;
                std::getline(std::cin, key);
                if (!key.empty()) {
                    if (new_config.default_provider == "openai") new_config.openai_key = key;
                    else if (new_config.default_provider == "gemini") new_config.gemini_key = key;
                    else if (new_config.default_provider == "claude") new_config.claude_key = key;
                }
            }
        }

        memory_manager->SaveConfig(new_config);
        std::cout << "\n[Success] Configuration saved!\n";
        std::cout << "You can now run cortex without extra flags.\n";
        return 0;
    }

    // Determine final config based on priority: Flag > Env > Stored
    std::string final_provider = provider_name != "ollama" ? provider_name : stored_config.default_provider;
    
    std::string final_key = "";
    if (final_provider == "openai") {
        final_key = openai_key;
        if (final_key.empty()) final_key = getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
        if (final_key.empty()) final_key = stored_config.openai_key;
    } else if (final_provider == "gemini") {
        final_key = gemini_key;
        if (final_key.empty()) final_key = getenv("GEMINI_API_KEY") ? getenv("GEMINI_API_KEY") : "";
        if (final_key.empty()) final_key = stored_config.gemini_key;
    } else if (final_provider == "claude") {
        final_key = claude_key;
        if (final_key.empty()) final_key = getenv("CLAUDE_API_KEY") ? getenv("CLAUDE_API_KEY") : "";
        if (final_key.empty()) final_key = stored_config.claude_key;
    }

    std::string final_ollama_url = (ollama_url != "http://localhost:11434") ? ollama_url : stored_config.ollama_url;

    // Initialize core components
    auto message_bus = std::make_shared<ZeroMQBus>();
    std::shared_ptr<LLMClient> llm_client;

    if (final_provider == "ollama") {
        llm_client = std::make_shared<OllamaClient>(final_ollama_url);
    } else {
        RemoteProvider rp = RemoteClient::StringToProvider(final_provider);
        if (final_key.empty()) {
            std::cerr << "Error: No API key found for provider '" << final_provider << "'. Run 'cortex auth' to configure.\n";
            return 1;
        }
        llm_client = std::make_shared<RemoteClient>(rp, final_key);
    }

    auto agent_manager = std::make_unique<AgentManager>(message_bus, llm_client, memory_manager);
    auto debate_manager = std::make_unique<DebateManager>(*agent_manager, message_bus);

    std::string bus_endpoint = "ipc:///tmp/cortex_bus";

    // Initial load of model if provided, or pick default for Ollama
    if (!model_name.empty()) {
        llm_client->LoadModel(model_name);
    } else if (final_provider == "ollama") {
        auto models = llm_client->ListModels();
        if (!models.empty()) {
            std::cout << "[Core] No model specified, using first available: " << models[0] << "\n";
            llm_client->LoadModel(models[0]);
        }
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
            std::string provider = create_agent_provider;
            std::string model = create_agent_model;
            
            if (create_agent_ollama) {
                provider = "ollama";
                if (model.empty()) {
                    // Try to list models and pick the first one
                    auto models = llm_client->ListModels();
                    if (!models.empty()) {
                        model = models[0];
                        std::cout << "[Core] No model specified for Ollama agent, using default: " << model << "\n";
                    } else {
                        std::cerr << "[Core] Warning: No Ollama models found. Please ensure Ollama is running and has models installed.\n";
                    }
                }
            }

            std::cout << "[Core] Creating agent: " << create_agent_name << " (" << create_agent_type << ")\n";
            agent_manager->CreateAgent(create_agent_name, create_agent_type, provider, model);
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
        } else if (agent_cmd->got_subcommand(agent_delete_cmd) || app.got_subcommand(pdel_cmd)) {
            for (const auto& name : delete_agent_names) {
                std::cout << "[Core] Deleting agent: " << name << "\n";
                if (agent_manager->RemoveAgent(name)) {
                    std::cout << "[Core] Successfully deleted agent '" << name << "'.\n";
                } else {
                    std::cout << "[Core] Error: Agent '" << name << "' not found.\n";
                }
            }
        } else {
            std::cout << agent_cmd->help();
        }
        return 0;
    }

    // Handle 'auth' subcommand
    if (app.got_subcommand(auth_cmd)) {
        // ... (auth logic)
    }
 
    // Handle 'model' subcommand
    if (app.got_subcommand(model_cmd)) {
        if (model_cmd->got_subcommand(model_list_cmd)) {
            auto models = llm_client->ListModels();
            std::cout << "[Core] Available models:\n";
            for (const auto& m : models) {
                std::cout << " - " << m << "\n";
            }
        } else if (model_cmd->got_subcommand(model_remove_cmd)) {
            std::cout << "[Core] Removing model: " << remove_model_name << "\n";
            if (llm_client->RemoveModel(remove_model_name)) {
                std::cout << "[Core] Model removed.\n";
            } else {
                std::cout << "[Core] Failed to remove model.\n";
            }
        }
        return 0;
    }
 
    // Handle 'run' subcommand
    if (app.got_subcommand(run_cmd)) {
        std::string target_agent = run_agent_name;
        if (target_agent.empty()) {
            auto agents = agent_manager->GetAllAgents();
            if (agents.empty()) {
                std::cerr << "Error: No agents available. Create one with 'cortex agent create'.\n";
                return 1;
            }
            target_agent = agents[0]->GetName();
            std::cout << "[Core] No agent specified, using first available: " << target_agent << "\n";
        } else {
            if (!agent_manager->GetAgent(target_agent)) {
                std::cerr << "Error: Agent '" << target_agent << "' not found.\n";
                return 1;
            }
        }

        std::cout << "[Core] Running task with agent '" << target_agent << "': " << task_desc << "\n";

        // Start message bus in background
        std::thread bus_thread([&]() {
            message_bus->Run();
        });

        // Give ZeroMQ a moment to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Start the agent so it can receive task assignments in THIS process
        agent_manager->StartAgent(target_agent);

        std::string task_id = "task_run_" + std::to_string(std::time(nullptr));
        std::string result_content = "";
        bool task_done = false;

        // Subscribe to TASK_UPDATE for this specific task
        message_bus->Subscribe("signal", [&](const Message& msg) {
            if (msg.payload.action == "TASK_UPDATE") {
                if (msg.payload.params.contains("task_id") && msg.payload.params["task_id"] == task_id) {
                    result_content = msg.payload.content.get<std::string>();
                    task_done = true;
                }
            }
        });

        // Send task assignment
        Message msg = Message::Create("CLI", "signal", "task_assigned", 
                                     task_desc, {{"task_id", task_id}, {"agent_name", target_agent}});
        message_bus->Publish(msg);

        std::cout << "[Core] Waiting for agent response...\n";
        while (!task_done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "\n--- Task Result ---\n" << result_content << "\n-------------------\n";

        if (!run_output_file.empty()) {
            try {
                std::ofstream ofs(run_output_file);
                if (ofs.is_open()) {
                    ofs << result_content;
                    std::cout << "[Core] Output saved to: " << run_output_file << "\n";
                } else {
                    std::cerr << "[Core] Error: Could not open file for writing: " << run_output_file << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "[Core] Error writing to file: " << e.what() << "\n";
            }
        }

        message_bus->Stop();
        if (bus_thread.joinable()) bus_thread.join();
        return 0;
    }

    // Handle 'debate' subcommand
    if (app.got_subcommand(debate)) {
        if (debate->got_subcommand(debate_stop_cmd)) {
            // Give ZeroMQ a moment to establish connection
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            debate_manager->StopDebate();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 0;
        }
        
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

    // Handle 'work' subcommand
    if (app.got_subcommand("work")) {
        auto workflow_manager = std::make_unique<WorkflowManager>(*agent_manager, message_bus);
        if (work_agents.empty()) {
            std::cout << "Error: No agents specified for the team.\n";
            return 0;
        }

        // Handle comma-separated names within the vector
        std::vector<std::string> final_work_agents;
        for (const auto& agent_list : work_agents) {
            std::stringstream ss(agent_list);
            std::string item;
            while (std::getline(ss, item, ',')) {
                // Trim whitespace
                item.erase(0, item.find_first_not_of(" \t\n\r"));
                item.erase(item.find_last_not_of(" \t\n\r") + 1);
                if (!item.empty()) {
                    final_work_agents.push_back(item);
                }
            }
        }

        if (final_work_agents.empty()) {
            std::cout << "Error: No valid agents specified.\n";
            return 0;
        }

        // Start the message bus in a background thread to receive updates
        std::thread bus_thread([&]() {
            message_bus->Run();
        });

        // Give ZeroMQ a moment to establish connection
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        workflow_manager->StartWorkflow(work_goal, final_work_agents);
        
        std::cout << "[Core] Team workflow started. Waiting for completion... (Press Ctrl+C to abort)\n";
        
        // Wait until workflow is done
        while (workflow_manager->IsActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        message_bus->Stop();
        if (bus_thread.joinable()) bus_thread.join();

        std::cout << "[Core] Workflow finished.\n";
        return 0;
    }

    // Handle 'logs' subcommand
    if (app.got_subcommand(logs_cmd)) {
        if (clear_logs) {
            Logger::GetInstance().ClearLogs();
            std::cout << "[Core] Logs cleared.\n";
            return 0;
        }
        
        std::cout << "--- Cortex Logs ---\n";
        std::cout << Logger::GetInstance().GetLogs(logs_last_n);
        return 0;
    }

    // Default target: if no arguments were provided or no flags triggering returns
    if (argc == 1) {
        std::cout << app.help();
    }

    return 0;
}
