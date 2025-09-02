#include "store.hpp"

#include <filesystem>
#include <iostream>

#include "command.hpp"
Store::Store(const std::string& aof_file) : aof_file_(aof_file) {
    if (std::filesystem::exists(aof_file)) {
        replayAof();
        std::cout << "replay end...\n";
    }

    aof_.open(aof_file, std::ios::app);
    if (!aof_.is_open()) {
        throw std::runtime_error("Failed to open AOF file: " + aof_file);
    }

    std::cout << "da ying...\n";
    for (auto x : data_) {
        std::cout << x.first << " " << x.second;
    }
    std::cout << "da ying end...\n";
}

Store::~Store() {
    if (aof_.is_open()) {
        aof_.close();
    }
}

void Store::set(const std::string& key, const std::string& value) {
    data_[key] = value;

    // Log SET command in RESP format
    std::vector<std::string_view> command = {"SET", key, value};
    logCommand(command);
}

std::string Store::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return "";  // Return empty string for missing keys
}

void Store::logCommand(const std::vector<std::string_view>& command) {
    if (!aof_.is_open()) {
        return;
    }

    aof_ << "*" << command.size() << "\r\n";
    for (const auto& arg : command) {
        aof_ << "$" << arg.size() << "\r\n" << arg << "\r\n";
    }
    aof_.flush();
}

void Store::replayAof() {
    std::ifstream file(aof_file_);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open AOF file for replay: " + aof_file_);
    }

    std::string buffer;
    std::string line;
    while (std::getline(file, line)) {
        buffer += line + "\r\n";
        size_t consumed = 0;
        while (consumed < buffer.size()) {
            size_t bytes_consumed = 0;
            std::string response = Command::process(buffer, bytes_consumed, *this);
            if (bytes_consumed == 0) {
                break;
            }
            buffer.erase(0, bytes_consumed);
        }
    }
    std::cout << "replayAof\n";

    file.close();
}
