#pragma once

#include <cortex/llm/llm_client.hpp>
#include <string>
#include <memory>

namespace cortex {

class OllamaClient : public LLMClient {
public:
    OllamaClient(const std::string& base_url = "http://localhost:11434");
    ~OllamaClient();

    bool LoadModel(const std::string& model_name) override;
    GenerationResult Generate(const std::string& prompt, const GenerationOptions& options) override;
    std::vector<std::string> ListModels() override;
    bool RemoveModel(const std::string& model_name) override;

private:
    std::string base_url_;
    std::string model_name_;
};

} // namespace cortex
