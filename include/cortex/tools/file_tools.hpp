#pragma once

#include <cortex/tools/tool.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace cortex {

class ReadFileTool : public Tool {
public:
    std::string GetName() const override { return "read_file"; }
    std::string GetDescription() const override { return "Read the contents of a file from the local filesystem."; }
    
    nlohmann::json GetDefinition() const override {
        return {
            {"name", GetName()},
            {"description", GetDescription()},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {
                        {"type", "string"},
                        {"description", "The absolute or relative path to the file to read."}
                    }}
                }},
                {"required", {"path"}}
            }}
        };
    }

    std::string Execute(const nlohmann::json& args) override {
        if (!args.contains("path")) return "Error: Missing 'path' argument.";
        std::string path = args["path"];

        if (!std::filesystem::exists(path)) {
            return "Error: File does not exist at path: " + path;
        }

        try {
            std::ifstream file(path);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        } catch (const std::exception& e) {
            return "Error reading file: " + std::string(e.what());
        }
    }
};

class WriteFileTool : public Tool {
public:
    std::string GetName() const override { return "write_file"; }
    std::string GetDescription() const override { return "Write content to a file on the local filesystem."; }
    
    nlohmann::json GetDefinition() const override {
        return {
            {"name", GetName()},
            {"description", GetDescription()},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {
                        {"type", "string"},
                        {"description", "The path where the file should be written."}
                    }},
                    {"content", {
                        {"type", "string"},
                        {"description", "The content to write into the file."}
                    }}
                }},
                {"required", {"path", "content"}}
            }}
        };
    }

    std::string Execute(const nlohmann::json& args) override {
        if (!args.contains("path") || !args.contains("content")) {
            return "Error: Missing 'path' or 'content' argument.";
        }
        std::string path = args["path"];
        std::string content = args["content"];

        try {
            std::ofstream file(path);
            if (!file.is_open()) return "Error: Could not open file for writing: " + path;
            file << content;
            return "Successfully wrote to " + path;
        } catch (const std::exception& e) {
            return "Error writing file: " + std::string(e.what());
        }
    }
};

} // namespace cortex
