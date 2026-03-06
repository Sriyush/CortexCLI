#pragma once

#include <string>
#include <nlohmann/json.hpp>

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

    std::string ToJson() const {
        return nlohmann::json(*this).dump();
    }

    static Message FromJson(const std::string& json_str) {
        return nlohmann::json::parse(json_str).get<Message>();
    }
};

} // namespace cortex
