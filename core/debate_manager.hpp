#pragma once

#include "agent_manager.hpp"
#include <string>
#include <vector>

namespace cortex {

class DebateManager {
public:
    DebateManager(AgentManager& agent_manager, std::shared_ptr<MessageBus> bus);

    /**
     * @brief Initiates a debate between the specified agents.
     * @param topic The topic or task to debate.
     * @param agent_names List of agents to participate.
     */
    void StartDebate(const std::string& topic, const std::vector<std::string>& agent_names);
    void StopDebate();

private:
    AgentManager& agent_manager_;
    std::shared_ptr<MessageBus> bus_;
};

} // namespace cortex
