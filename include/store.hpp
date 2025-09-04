#pragma once
#include <chrono>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class Store {
public:
    using time_point = std::chrono::system_clock::time_point;
    Store(const std::string& aof_file);
    ~Store();

    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;

    bool setExpire(const std::string& key, int seconds);
    void cleanupExpiredKeys();

private:
    void logCommand(const std::vector<std::string_view>& command);
    void replayAof();

    std::unordered_map<std::string, std::string> data_;
    std::unordered_map<std::string, time_point> expirations_;

    std::ofstream aof_;
    std::string aof_file_;
};