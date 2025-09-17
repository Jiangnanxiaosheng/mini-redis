#pragma once
#include <chrono>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class Store {
public:
    using time_point = std::chrono::system_clock::time_point;
    Store(const std::string& aof_file);
    ~Store();

    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;

    void vecSet(const std::string& key, const std::vector<float>& vec);
    std::vector<float> vecGet(const std::string& key) const;

    bool setExpire(const std::string& key, int seconds);
    void cleanupExpiredKeys();

    void forEachVector(
        const std::function<void(const std::string&, const std::vector<float>&)>& callback) const;

private:
    void logCommand(const std::vector<std::string_view>& command);
    void replayAof();

    std::unordered_map<std::string, std::variant<std::string, std::vector<float>>> data_;
    std::unordered_map<std::string, time_point> expirations_;

    std::ofstream aof_;
    std::string aof_file_;
};