#pragma once
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class Store {
public:
    Store(const std::string& aof_file);
    ~Store();

    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;

private:
    void logCommand(const std::vector<std::string_view>& command);
    void replayAof();

    std::unordered_map<std::string, std::string> data_;

    std::ofstream aof_;
    std::string aof_file_;
};