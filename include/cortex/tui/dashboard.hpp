#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <cortex/core/agent_manager.hpp>
#include <cortex/messaging/zeromq_bus.hpp>
#include <vector>
#include <string>
#include <mutex>

namespace cortex {

class Dashboard {
public:
    Dashboard(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus);
    
    void Run();

private:
    AgentManager& agent_manager_;
    std::shared_ptr<MessageBus> bus_;
    
    std::vector<std::string> log_entries_;
    std::mutex log_mutex_;
    
    void AddLog(const std::string& msg);
    ftxui::Element RenderSidebar();
    ftxui::Element RenderMainContent();
    ftxui::Element RenderLogs();
};

} // namespace cortex
