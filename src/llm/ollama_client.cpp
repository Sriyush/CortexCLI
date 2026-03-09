#include <cortex/llm/ollama_client.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace cortex {

OllamaClient::OllamaClient(const std::string& base_url) : base_url_(base_url) {}

OllamaClient::~OllamaClient() {}

bool OllamaClient::LoadModel(const std::string& model_name) {
    if (model_name.empty()) return false;
    
    std::cout << "[OllamaClient] Checking for model: " << model_name << "...\n";
    
    httplib::Client cli(base_url_);
    cli.set_connection_timeout(5); // 5 seconds connection timeout
    cli.set_read_timeout(10);       // 10 seconds read timeout for model check
    auto res = cli.Post("/api/show", json({{"name", model_name}}).dump(), "application/json");
    
    if (res && res->status == 200) {
        model_name_ = model_name;
        std::cout << "[OllamaClient] Model '" << model_name << "' is available.\n";
        return true;
    } else {
        std::cerr << "[OllamaClient] Error: Model '" << model_name << "' not found or Ollama is not running (please run ollama serve to keep ollama running).\n";
        return false;
    }
}

GenerationResult OllamaClient::Generate(const std::string& prompt, const GenerationOptions& options) {
    GenerationResult result;
    if (model_name_.empty()) {
        result.text = "[OllamaClient] Error: Model not loaded.";
        return result;
    }

    httplib::Client cli(base_url_);
    cli.set_read_timeout(300); // Longer timeout for model loading + generation

    json request_body = {
        {"model", model_name_},
        {"prompt", prompt},
        {"stream", false},
        {"options", {
            {"num_predict", options.n_predict},
            {"temperature", options.temp},
            {"top_p", options.top_p}
        }}
    };

    auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            auto response_json = json::parse(res->body);
            result.text = response_json.value("response", "");
            result.prompt_tokens = response_json.value("prompt_eval_count", 0);
            result.completion_tokens = response_json.value("eval_count", 0);
        } catch (const std::exception& e) {
            std::cerr << "[OllamaClient] JSON parse error: " << e.what() << "\n";
            result.text = "[OllamaClient] Error: Failed to parse response.";
        }
    } else {
        std::cerr << "[OllamaClient] HTTP error: " << (res ? std::to_string(res->status) : "Connection failed") << "\n";
        result.text = "[OllamaClient] Error: Request failed.";
    }
    return result;
}

std::vector<std::string> OllamaClient::ListModels() {
    std::vector<std::string> models;
    httplib::Client cli(base_url_);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(10);

    auto res = cli.Get("/api/tags");
    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            if (j.contains("models") && j["models"].is_array()) {
                for (const auto& model : j["models"]) {
                    if (model.contains("name")) {
                        models.push_back(model["name"]);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[OllamaClient] JSON parse error in ListModels: " << e.what() << "\n";
        }
    } else {
        std::cerr << "[OllamaClient] Failed to list models: " << (res ? std::to_string(res->status) : "Connection failed") << "\n";
    }
    return models;
}

bool OllamaClient::RemoveModel(const std::string& model_name) {
    httplib::Client cli(base_url_);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(10);

    json body = {{"name", model_name}};
    auto res = cli.Delete("/api/delete", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "[OllamaClient] Model '" << model_name << "' removed successfully.\n";
        return true;
    } else {
        std::cerr << "[OllamaClient] Failed to remove model '" << model_name << "': " 
                  << (res ? std::to_string(res->status) : "Connection failed") << "\n";
        return false;
    }
}

} // namespace cortex
