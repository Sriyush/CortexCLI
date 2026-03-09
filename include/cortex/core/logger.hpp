#pragma once

#include <string>
#include <mutex>
#include <fstream>

namespace cortex {

class Logger {
public:
    static Logger& GetInstance();

    void Log(const std::string& sender, const std::string& message);
    void LogSystem(const std::string& message);
    void LogToolCall(const std::string& agent, const std::string& tool, const std::string& args);
    void LogToolResult(const std::string& agent, const std::string& tool, const std::string& result);
    
    std::string GetLogs(int last_n_lines = 50);
    void ClearLogs();

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    std::string log_file_ = "logs.txt";
};

class StreamLogger : public std::streambuf {
public:
    StreamLogger(std::ostream& stream, const std::string& sender);
    ~StreamLogger();

protected:
    int overflow(int c) override;
    int sync() override;

private:
    std::ostream& stream_;
    std::streambuf* old_buf_;
    std::string sender_;
    std::string buffer_;
};

} // namespace cortex
