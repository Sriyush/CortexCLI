#include "dashboard.hpp"
#include "stats_manager.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cortex {

using namespace ftxui;

Dashboard::Dashboard(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus)
    : agent_manager_(agent_manager), bus_(bus) {
    
    AddLog("Cortex Dashboard Initialized.");
    AddLog("Waiting for protocol messages...");

    // Start all agents so they are listening to the bus in this process
    auto all_agents = agent_manager_.GetAllAgents();
    for (auto& agent : all_agents) {
        agent->Start();
    }

    // Subscribe to all messages for logging
    if (bus_) {
        bus_->Subscribe("", [this](const Message& msg) {
            std::stringstream ss;
            ss << "[" << msg.header.sender << "] ";
            if (!msg.payload.content.empty()) {
                ss << msg.payload.content;
            } else {
                ss << msg.payload.action;
            }
            AddLog(ss.str());
        });
    }
}

void Dashboard::AddLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << " | " << msg;
    log_entries_.push_back(ss.str());
    if (log_entries_.size() > 50) {
        log_entries_.erase(log_entries_.begin());
    }
}

Element Dashboard::RenderSidebar() {
    auto agents = agent_manager_.GetAllAgents();
    Elements items;
    items.push_back(text(" ACTIVE AGENTS ") | bold | color(Color::Yellow));
    items.push_back(separator());
    
    for (const auto& agent : agents) {
        bool idle = true; // In a real app, check agent->GetStatus()
        items.push_back(
            hbox({
                text(idle ? " ● " : " ⚡ ") | color(idle ? Color::Green : Color::Cyan),
                text(agent->GetName())
            })
        );
    }
    
    if (agents.empty()) {
        items.push_back(text("No agents active.") | dim);
    }
    
    return vbox(std::move(items)) | size(WIDTH, GREATER_THAN, 25) | border;
}

Element Dashboard::RenderLogs() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    Elements log_elems;
    if (log_entries_.empty()) {
        log_elems.push_back(text("No protocol activity yet...") | dim);
    } else {
        for (const auto& log : log_entries_) {
            log_elems.push_back(text(log));
        }
    }
    return vbox(std::move(log_elems)) | borderDouble | color(Color::GrayDark);
}

Element Dashboard::RenderMainContent() {
    Elements items;
    items.push_back(text("CORTEX OS - MULTI-AGENT ENVIRONMENT") | bold | center | color(Color::Cyan));
    items.push_back(separator());
    
    auto agents = agent_manager_.GetAllAgents();
    int running_count = 0;
    for (const auto& a : agents) if (a->GetState() == AgentState::RUNNING) running_count++;

    Elements status_items;
    status_items.push_back(text("System Status: OPERATIONAL") | color(Color::Green));
    status_items.push_back(text("Memory Usage: " + StatsManager::GetMemoryUsage()) | color(Color::Cyan));
    status_items.push_back(text("Tokens Used: " + std::to_string(StatsManager::GetInstance().GetTotalTokens())) | color(Color::Yellow));
    status_items.push_back(text("Agents Running: " + std::to_string(running_count)) | color(Color::Blue));
    status_items.push_back(text("Active Tasks: 0") | dim);
    
    items.push_back(vbox(std::move(status_items)) | border);
    items.push_back(filler());
    items.push_back(text("LIVE PROTOCOL LOGS") | dim);
    items.push_back(RenderLogs() | flex);

    return vbox(std::move(items)) | flex | border;
}

void Dashboard::Run() {
    auto screen = ScreenInteractive::TerminalOutput();
    
    auto renderer = Renderer([&] {
        Elements root_items;
        root_items.push_back(RenderSidebar());
        root_items.push_back(RenderMainContent());
        return hbox(std::move(root_items));
    });
    
    // Auto-refresh timer
    std::atomic<bool> refresh_ui = true;
    std::thread timer([&] {
        while (refresh_ui) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            screen.PostEvent(Event::Custom);
        }
    });

    // Run the message bus in a background thread
    std::thread bus_thread([&] {
        if (bus_) bus_->Run();
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            refresh_ui = false;
            if (bus_) bus_->Stop();
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    if (timer.joinable()) timer.join();
    if (bus_thread.joinable()) bus_thread.join();
}

} // namespace cortex
