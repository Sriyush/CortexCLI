#pragma once

#include <cortex/tools/tool.hpp>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

namespace cortex {

class ShellTool : public Tool {
public:
    std::string GetName() const override { return "run_shell"; }
    std::string GetDescription() const override { return "Execute a bash command in the local terminal and get the output."; }
    
    nlohmann::json GetDefinition() const override {
        return {
            {"name", GetName()},
            {"description", GetDescription()},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"command", {
                        {"type", "string"},
                        {"description", "The bash command to execute."}
                    }}
                }},
                {"required", {"command"}}
            }}
        };
    }

    std::string Execute(const nlohmann::json& args) override {
        if (!args.contains("command")) return "Error: Missing 'command' argument.";
        std::string command = args["command"];

        std::array<char, 128> buffer;
        std::string result;
        
        // Use popen (standard C/C++) to run wait for shell
        // Note: For a production OS, we'd want a more robust PTY implementation
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            return "Error: popen() failed.";
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        return result.empty() ? "(Success - No Output)" : result;
    }
};

} // namespace cortex
