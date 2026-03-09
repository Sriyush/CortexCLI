#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>

namespace cortex {

struct MessageHeader {
    std::string msg_id;
    std::string sender;
    std::string timestamp;
    std::string type;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MessageHeader, msg_id, sender, timestamp, type)
};

struct MessagePayload {
    std::string action;
    nlohmann::json params;
    std::string status;
    nlohmann::json content;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MessagePayload, action, params, status, content)
};

struct Message {
    MessageHeader header;
    MessagePayload payload;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Message, header, payload)

    static Message Create(const std::string& sender, 
                         const std::string& type, 
                         const std::string& action, 
                         const nlohmann::json& content = nlohmann::json(),
                         const nlohmann::json& params = nlohmann::json::object(),
                         const std::string& status = "success") {
        Message msg;
        
        // Timestamp in ISO8601
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
        
        msg.header.timestamp = ss.str();
        msg.header.sender = sender;
        msg.header.type = type;
        
        // Basic unique ID
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        std::stringstream id_ss;
        id_ss << sender << "_" << std::hex << dis(gen);
        msg.header.msg_id = id_ss.str();

        msg.payload.action = action;
        msg.payload.content = content;
        msg.payload.params = params;
        msg.payload.status = status;
        
        return msg;
    }

    std::string ToJson() const {
        return nlohmann::json(*this).dump();
    }

    static Message FromJson(const std::string& json_str) {
        return nlohmann::json::parse(json_str).get<Message>();
    }
};

} // namespace cortex
