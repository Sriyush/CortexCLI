#include "remote_client.hpp"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace cortex {

RemoteClient::RemoteClient(RemoteProvider provider, const std::string& api_key)
    : provider_(provider), api_key_(api_key) {}

RemoteClient::~RemoteClient() {}

RemoteProvider RemoteClient::StringToProvider(const std::string& provider_str) {
    if (provider_str == "openai") return RemoteProvider::OpenAI;
    if (provider_str == "gemini") return RemoteProvider::Gemini;
    if (provider_str == "claude") return RemoteProvider::Claude;
    return RemoteProvider::Unknown;
}

bool RemoteClient::LoadModel(const std::string& model_name) {
    if (model_name.empty()) return false;
    model_name_ = model_name;
    std::cout << "[RemoteClient] Provider: " << (int)provider_ << ", Model: " << model_name_ << "\n";
    return true;
}

std::string RemoteClient::Generate(const std::string& prompt, const GenerationOptions& options) {
    switch (provider_) {
        case RemoteProvider::OpenAI: return GenerateOpenAI(prompt, options);
        case RemoteProvider::Gemini: return GenerateGemini(prompt, options);
        case RemoteProvider::Claude: return GenerateClaude(prompt, options);
        default: return "[RemoteClient] Error: Unknown provider.";
    }
}

std::string RemoteClient::GenerateOpenAI(const std::string& prompt, const GenerationOptions& options) {
    httplib::SSLClient cli("api.openai.com");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(60);

    json body = {
        {"model", model_name_},
        {"messages", {{{"role", "user"}, {"content", prompt}}}},
        {"max_tokens", options.n_predict},
        {"temperature", options.temp},
        {"top_p", options.top_p}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/chat/completions", headers, body.dump(), "application/json");

    if (res && res->status == 200) {
        auto j = json::parse(res->body);
        return j["choices"][0]["message"]["content"];
    }
    return "[RemoteClient] OpenAI API Error: " + (res ? std::to_string(res->status) : "Connection failed") + " " + (res ? res->body : "");
}

std::string RemoteClient::GenerateGemini(const std::string& prompt, const GenerationOptions& options) {
    httplib::SSLClient cli("generativelanguage.googleapis.com");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(60);

    json body = {
        {"contents", {{{"parts", {{{"text", prompt}}}}}}},
        {"generationConfig", {
            {"maxOutputTokens", options.n_predict},
            {"temperature", options.temp},
            {"topP", options.top_p}
        }}
    };

    std::string path = "/v1beta/models/" + model_name_ + ":generateContent?key=" + api_key_;
    auto res = cli.Post(path.c_str(), body.dump(), "application/json");

    if (res && res->status == 200) {
        auto j = json::parse(res->body);
        return j["candidates"][0]["content"]["parts"][0]["text"];
    }
    return "[RemoteClient] Gemini API Error: " + (res ? std::to_string(res->status) : "Connection failed") + " " + (res ? res->body : "");
}

std::string RemoteClient::GenerateClaude(const std::string& prompt, const GenerationOptions& options) {
    httplib::SSLClient cli("api.anthropic.com");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(60);

    json body = {
        {"model", model_name_},
        {"messages", {{{"role", "user"}, {"content", prompt}}}},
        {"max_tokens", options.n_predict},
        {"temperature", options.temp},
        {"top_p", options.top_p}
    };

    httplib::Headers headers = {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, body.dump(), "application/json");

    if (res && res->status == 200) {
        auto j = json::parse(res->body);
        return j["content"][0]["text"];
    }
    return "[RemoteClient] Claude API Error: " + (res ? std::to_string(res->status) : "Connection failed") + " " + (res ? res->body : "");
}

std::vector<std::string> RemoteClient::ListModels() {
    switch (provider_) {
        case RemoteProvider::OpenAI:
            return {"gpt-4o", "gpt-4-turbo", "gpt-3.5-turbo"};
        case RemoteProvider::Gemini:
            return {"gemini-1.5-pro", "gemini-1.5-flash", "gemini-1.0-pro"};
        case RemoteProvider::Claude:
            return {"claude-3-5-sonnet-20240620", "claude-3-opus-20240229", "claude-3-haiku-20240307"};
        default:
            return {};
    }
}

bool RemoteClient::RemoveModel(const std::string& /*model_name*/) {
    return false;
}

} // namespace cortex
