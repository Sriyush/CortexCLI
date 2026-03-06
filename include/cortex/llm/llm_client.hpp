#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace cortex {

struct GenerationOptions {
    int32_t n_predict = 128;
    float temp = 0.8f;
    float top_p = 0.95f;
};

struct GenerationResult {
    std::string text;
    int32_t prompt_tokens = 0;
    int32_t completion_tokens = 0;
};

class LLMClient {
public:
    virtual ~LLMClient() = default;

    virtual bool LoadModel(const std::string& model_path) = 0;
    virtual GenerationResult Generate(const std::string& prompt, const GenerationOptions& options) = 0;
    virtual std::vector<std::string> ListModels() = 0;
    virtual bool RemoveModel(const std::string& model_name) = 0;
};

} // namespace cortex
