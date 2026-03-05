#pragma once

#include <string>
#include <memory>
#include <vector>

namespace cortex {

enum class AgentState {
    IDLE,
    RUNNING,
    PAUSED,
    STOPPED,
    ERROR
};

class Agent {
public:
    virtual ~Agent() = default;

    virtual std::string GetName() const = 0;
    virtual std::string GetType() const = 0;
    virtual AgentState GetState() const = 0;
    
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;

    // The main reasoning loop of the agent
    virtual void Tick() = 0;
};

} // namespace cortex
