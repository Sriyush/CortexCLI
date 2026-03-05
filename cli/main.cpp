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
#include "ollama_client.hpp"
#include "remote_client.hpp"
#include "memory_manager.hpp"
#include "debate_manager.hpp"
#include "dashboard.hpp"

using namespace cortex;

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
    auto run_cmd = app.add_subcommand("run", "Run a task");
    std::string task_desc;
    run_cmd->add_option("task", task_desc, "Task description");

    auto debate = app.add_subcommand("debate", "Start a debate between agents");
    auto debate_start_cmd = debate->add_subcommand("start", "Start a new debate");
    auto debate_stop_cmd = debate->add_subcommand("stop", "Stop all active debates");
 
    std::string debate_topic;
    std::vector<std::string> participants;
    debate_start_cmd->add_option("--topic", debate_topic, "The topic to debate")->required();
    debate_start_cmd->add_option("-p,--participants", participants, "Agents to participate in the debate");

    auto logs_cmd = app.add_subcommand("logs", "View logs for an agent");
    std::string logs_agent_name;
    logs_cmd->add_option("agent", logs_agent_name, "Agent name to view logs for");

    auto auth_cmd = app.add_subcommand("auth", "Configure LLM providers and API keys");
 
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
        
        Config new_config = stored_config;
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

    // Initial load of model if provided
    if (!model_name.empty()) {
        llm_client->LoadModel(model_name);
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
            std::string provider = "";
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
        std::cout << "[Core] Running task: " << (task_desc.empty() ? "unknown" : task_desc) << "\n";
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
