#include "llama_client.hpp"
#include "llama.h"
#include <iostream>
#include <vector>

namespace cortex {

struct LlamaClient::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;

    ~Impl() {
        if (ctx) {
            llama_free(ctx);
            ctx = nullptr;
        }
        if (model) {
            llama_model_free(model);
            model = nullptr;
        }
    }
};

LlamaClient::LlamaClient() : impl_(std::make_unique<Impl>()) {
    llama_backend_init();
}

LlamaClient::~LlamaClient() {
    llama_backend_free();
}

bool LlamaClient::LoadModel(const std::string& model_path) {
    if (model_path.empty()) return false;

    std::cout << "[LlamaClient] Loading model from: " << model_path << "...\n";
    
    llama_model_params model_params = llama_model_default_params();
    impl_->model = llama_model_load_from_file(model_path.c_str(), model_params);
    
    if (!impl_->model) {
        std::cerr << "[LlamaClient] Error: Failed to load model from " << model_path << "\n";
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048; // Default context size
    impl_->ctx = llama_init_from_model(impl_->model, ctx_params);

    if (!impl_->ctx) {
        std::cerr << "[LlamaClient] Error: Failed to initialize context from model.\n";
        return false;
    }

    std::cout << "[LlamaClient] Model loaded successfully.\n";
    return true;
}

std::string LlamaClient::Generate(const std::string& prompt, const GenerationOptions& options) {
    if (!impl_->ctx) {
        return "[Simulated LLM] Re: " + prompt.substr(0, 30) + "... (Model not loaded)";
    }

    const struct llama_vocab* vocab = llama_model_get_vocab(impl_->model);

    // 1. Tokenize
    std::vector<llama_token> tokens(prompt.size() + 2);
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true);
    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true);
    }
    tokens.resize(n_tokens);

    // 2. Clear KV cache for sequence 0
    llama_memory_t mem = llama_get_memory(impl_->ctx);
    llama_memory_seq_rm(mem, 0, -1, -1);

    // 3. Prepare batch
    llama_batch batch = llama_batch_init(std::max(n_tokens, options.n_predict), 0, 1);
    
    // Add prompt tokens to batch
    for (int i = 0; i < n_tokens; ++i) {
        batch.token[batch.n_tokens] = tokens[i];
        batch.pos[batch.n_tokens]   = i;
        batch.n_seq_id[batch.n_tokens] = 1;
        batch.seq_id[batch.n_tokens][0] = 0;
        batch.logits[batch.n_tokens] = (i == n_tokens - 1);
        batch.n_tokens++;
    }

    std::string result = "";
    int n_cur = n_tokens;

    // 4. Decode loop
    for (int i = 0; i < options.n_predict; ++i) {
        if (llama_decode(impl_->ctx, batch) != 0) {
            std::cerr << "[LlamaClient] Error: llama_decode failed\n";
            break;
        }

        // Sampling (Greedy)
        auto* logits = llama_get_logits_ith(impl_->ctx, batch.n_tokens - 1);
        llama_token new_token = 0;
        float max_logit = -1e10;
        int32_t n_vocab = llama_vocab_n_tokens(vocab);
        for (int v = 0; v < n_vocab; ++v) {
            if (logits[v] > max_logit) {
                max_logit = logits[v];
                new_token = v;
            }
        }

        if (new_token == llama_vocab_eos(vocab)) break;

        // Convert token to piece
        char buf[128];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, false);
        if (n > 0) {
            result.append(buf, n);
            // Proactive console output for easier debugging in the terminal
            std::cout << buf << std::flush;
        }

        // Prepare next batch (single token)
        batch.n_tokens = 0;
        batch.token[batch.n_tokens] = new_token;
        batch.pos[batch.n_tokens]   = n_cur;
        batch.n_seq_id[batch.n_tokens] = 1;
        batch.seq_id[batch.n_tokens][0] = 0;
        batch.logits[batch.n_tokens] = true;
        batch.n_tokens++;
        n_cur++;
    }
    std::cout << "\n";

    llama_batch_free(batch);
    return result;
}

} // namespace cortex
