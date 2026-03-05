#pragma once

#include <string>
#include <vector>
#include <memory>

namespace cortex {

struct GenerationOptions {
    int32_t n_predict = 128;
    float temp = 0.8f;
    float top_p = 0.95f;
};

class LLMClient {
public:
    virtual ~LLMClient() = default;

    virtual bool LoadModel(const std::string& model_path) = 0;
    virtual std::string Generate(const std::string& prompt, const GenerationOptions& options) = 0;
    virtual std::vector<std::string> ListModels() = 0;
    virtual bool RemoveModel(const std::string& model_name) = 0;
};

class LlamaClient : public LLMClient {
public:
    LlamaClient();
    ~LlamaClient();

    bool LoadModel(const std::string& model_path) override;
    std::string Generate(const std::string& prompt, const GenerationOptions& options) override;
    std::vector<std::string> ListModels() override { return {}; }
    bool RemoveModel(const std::string& /*model_name*/) override { return false; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cortex
