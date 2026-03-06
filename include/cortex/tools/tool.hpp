#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace cortex {

/**
 * @brief Base class for all tools that agents can execute.
 */
class Tool {
public:
    virtual ~Tool() = default;

    /**
     * @brief Unique name of the tool.
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Description of what the tool does.
     */
    virtual std::string GetDescription() const = 0;

    /**
     * @brief JSON schema definition for the tool's parameters.
     */
    virtual nlohmann::json GetDefinition() const = 0;

    /**
     * @brief Executes the tool with the provided JSON arguments.
     * @return Result of the tool execution as a string (usually JSON).
     */
    virtual std::string Execute(const nlohmann::json& args) = 0;
};

} // namespace cortex
