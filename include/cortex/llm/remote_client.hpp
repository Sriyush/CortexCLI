#pragma once

#include <cortex/llm/llm_client.hpp>
#include <string>
#include <memory>
#include <map>

namespace cortex {

enum class RemoteProvider {
    OpenAI,
    Gemini,
    Claude,
    Unknown
};

class RemoteClient : public LLMClient {
public:
    RemoteClient(RemoteProvider provider, const std::string& api_key);
    ~RemoteClient();

    bool LoadModel(const std::string& model_name) override;
    GenerationResult Generate(const std::string& prompt, const GenerationOptions& options) override;
    std::vector<std::string> ListModels() override;
    bool RemoveModel(const std::string& model_name) override;

    static RemoteProvider StringToProvider(const std::string& provider_str);

private:
    GenerationResult GenerateOpenAI(const std::string& prompt, const GenerationOptions& options);
    GenerationResult GenerateGemini(const std::string& prompt, const GenerationOptions& options);
    GenerationResult GenerateClaude(const std::string& prompt, const GenerationOptions& options);

    RemoteProvider provider_;
    std::string api_key_;
    std::string model_name_;
};

} // namespace cortex
