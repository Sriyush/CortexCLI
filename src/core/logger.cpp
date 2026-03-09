#include <cortex/core/logger.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <deque>

namespace cortex {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {}

void Logger::Log(const std::string& sender, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(log_file_, std::ios::app);
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    file << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "] "
         << "[" << sender << "] " << message << std::endl;
}

void Logger::LogSystem(const std::string& message) {
    Log("System", message);
}

void Logger::LogToolCall(const std::string& agent, const std::string& tool, const std::string& args) {
    Log(agent, "TOOL CALL: " + tool + " ARGS: " + args);
}

void Logger::LogToolResult(const std::string& agent, const std::string& tool, const std::string& result) {
    Log(agent, "TOOL RESULT: " + tool + " -> " + (result.length() > 200 ? result.substr(0, 200) + "..." : result));
}

std::string Logger::GetLogs(int last_n_lines) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(log_file_);
    std::string line;
    std::deque<std::string> lines;

    while (std::getline(file, line)) {
        lines.push_back(line);
        if (lines.size() > (size_t)last_n_lines) {
            lines.pop_front();
        }
    }

    std::string result;
    for (const auto& l : lines) {
        result += l + "\n";
    }
    return result;
}

void Logger::ClearLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(log_file_, std::ios::trunc);
}

StreamLogger::StreamLogger(std::ostream& stream, const std::string& sender)
    : stream_(stream), sender_(sender) {
    old_buf_ = stream_.rdbuf(this);
}

StreamLogger::~StreamLogger() {
    stream_.rdbuf(old_buf_);
}

int StreamLogger::overflow(int c) {
    if (c != EOF) {
        if (c == '\n') {
            Logger::GetInstance().Log(sender_, buffer_);
            buffer_.clear();
        } else {
            buffer_ += static_cast<char>(c);
        }
    }
    return old_buf_->sputc(c);
}

int StreamLogger::sync() {
    return old_buf_->pubsync();
}

} // namespace cortex
