#include <cortex/llm/remote_client.hpp>
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

GenerationResult RemoteClient::Generate(const std::string& prompt, const GenerationOptions& options) {
    switch (provider_) {
        case RemoteProvider::OpenAI: return GenerateOpenAI(prompt, options);
        case RemoteProvider::Gemini: return GenerateGemini(prompt, options);
        case RemoteProvider::Claude: return GenerateClaude(prompt, options);
        default: {
            GenerationResult res;
            res.text = "[RemoteClient] Error: Unknown provider.";
            return res;
        }
    }
}

GenerationResult RemoteClient::GenerateOpenAI(const std::string& prompt, const GenerationOptions& options) {
    GenerationResult res_obj;
    httplib::SSLClient cli("api.openai.com");
    cli.set_connection_timeout(10);
    cli.set_read_timeout(120);

    std::string model = model_name_;
    if (model.empty()) model = "gpt-4o";

    // OpenAI Responses API format (https://developers.openai.com/api/docs/guides/text)
    json body = {
        {"model", model},
        {"input", prompt}
    };

    std::string body_str = body.dump();
    std::cout << "[RemoteClient] OpenAI Responses API | model=" << model << " | body_size=" << body_str.size() << "\n";

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/responses", headers, body_str, "application/json");

    if (!res) {
        std::cerr << "[RemoteClient] OpenAI connection failed.\n";
        res_obj.text = "[RemoteClient] OpenAI Error: Connection failed";
        return res_obj;
    }

    std::cout << "[RemoteClient] OpenAI status=" << res->status << "\n";

    if (res->status == 200) {
        try {
            auto j = json::parse(res->body);
            // output_text is a convenience field in the response
            if (j.contains("output_text")) {
                res_obj.text = j["output_text"].get<std::string>();
            } else if (j.contains("output") && j["output"].is_array() && !j["output"].empty()) {
                auto& msg = j["output"][0];
                if (msg.contains("content") && msg["content"].is_array() && !msg["content"].empty()) {
                    res_obj.text = msg["content"][0].value("text", "");
                }
            }
            if (j.contains("usage")) {
                res_obj.prompt_tokens = j["usage"].value("input_tokens", 0);
                res_obj.completion_tokens = j["usage"].value("output_tokens", 0);
            }
        } catch (const std::exception& e) {
            std::cerr << "[RemoteClient] OpenAI parse error: " << e.what() << "\n";
            res_obj.text = "[RemoteClient] OpenAI Error: Failed to parse response.";
        }
    } else {
        std::cerr << "[RemoteClient] OpenAI error (" << res->status << "): " << res->body << "\n";
        res_obj.text = "[RemoteClient] OpenAI Error " + std::to_string(res->status) + ": " + res->body;
    }
    return res_obj;
}

GenerationResult RemoteClient::GenerateGemini(const std::string& prompt, const GenerationOptions& options) {
    GenerationResult res_obj;
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
    std::cout << "[RemoteClient] Gemini API | model=" << model_name_ << "\n";
    auto res = cli.Post(path.c_str(), body.dump(), "application/json");

    if (!res) {
        res_obj.text = "[RemoteClient] Gemini Error: Connection failed";
        return res_obj;
    }

    std::cout << "[RemoteClient] Gemini status=" << res->status << "\n";

    if (res->status == 200) {
        try {
            auto j = json::parse(res->body);
            // Gemini format: candidates[0].content.parts[0].text
            res_obj.text = j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
            if (j.contains("usageMetadata")) {
                res_obj.prompt_tokens = j["usageMetadata"].value("promptTokenCount", 0);
                res_obj.completion_tokens = j["usageMetadata"].value("candidatesTokenCount", 0);
            }
        } catch (const std::exception& e) {
            std::cerr << "[RemoteClient] Gemini parse error: " << e.what() << "\n";
            res_obj.text = "[RemoteClient] Gemini Error: Failed to parse response.";
        }
        return res_obj;
    }
    std::cerr << "[RemoteClient] Gemini error (" << res->status << "): " << res->body << "\n";
    res_obj.text = "[RemoteClient] Gemini Error " + std::to_string(res->status) + ": " + res->body;
    return res_obj;
}

GenerationResult RemoteClient::GenerateClaude(const std::string& prompt, const GenerationOptions& options) {
    GenerationResult res_obj;
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
        res_obj.text = j["content"][0]["text"];
        if (j.contains("usage")) {
            res_obj.prompt_tokens = j["usage"].value("input_tokens", 0);
            res_obj.completion_tokens = j["usage"].value("output_tokens", 0);
        }
        return res_obj;
    }
    res_obj.text = "[RemoteClient] Claude API Error: " + (res ? std::to_string(res->status) : "Connection failed") + " " + (res ? res->body : "");
    return res_obj;
}

std::vector<std::string> RemoteClient::ListModels() {
    switch (provider_) {
        case RemoteProvider::OpenAI:
            return {
                "gpt-5",
                "gpt-5-mini",
                "gpt-5-nano",
                "gpt-4o",
                "gpt-4o-mini"
            };
        case RemoteProvider::Gemini:
            return {"gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash", "gemini-2.0-flash-lite"};
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
