#include "store.hpp"

#include <filesystem>
#include <iostream>
#include <iterator>

#include "command.hpp"
Store::Store(const std::string& aof_file) : aof_file_(aof_file) {
    if (std::filesystem::exists(aof_file)) {
        replayAof();
    }

    aof_.open(aof_file, std::ios::app | std::ios::binary);
    if (!aof_.is_open()) {
        throw std::runtime_error("Failed to open AOF file: " + aof_file);
    }

    // std::cout << "print persistent data...\n";
    // for (auto x : data_) {
    //     std::cout << x.first << " " << x.second << "\n";
    // }
    // std::cout << "print persistent data...\n";
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
    auto it = expirations_.find(key);
    if (it != expirations_.end()) {
        if (std::chrono::system_clock::now() >= it->second) {
            return "";  // Key has expired
        }
    }
    auto data_it = data_.find(key);
    if (data_it != data_.end()) {
        return data_it->second;
    }
    return "";  // Return empty string for missing keys
}

bool Store::setExpire(const std::string& key, int seconds) {
    if (seconds <= 0 || data_.find(key) == data_.end()) {
        return false;  // 无效的过期时间或键不存在
    }

    auto expire_time = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    expirations_[key] = expire_time;

    // Log EXPIRE command in RESP format
    std::vector<std::string_view> command = {"EXPIRE", key, std::to_string(seconds)};
    logCommand(command);
    return true;
}

void Store::cleanupExpiredKeys() {
    auto now = std::chrono::system_clock::now();
    for (auto it = expirations_.begin(); it != expirations_.end();) {
        if (now >= it->second) {
            data_.erase(it->first);
            it = expirations_.erase(it);
        } else {
            ++it;
        }
    }
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
    std::ifstream file(aof_file_, std::ios::binary);  // 修正：使用 binary 模式打开
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open AOF file for replay: " + aof_file_);
    }

    // std::string buffer;
    // std::string line;
    // while (std::getline(file, line)) {
    //     buffer += line + "\r\n";
    //     size_t consumed = 0;
    //     while (consumed < buffer.size()) {
    //         size_t bytes_consumed = 0;
    //         std::string response = Command::process(buffer, bytes_consumed, *this);
    //         if (bytes_consumed == 0) {
    //             break;
    //         }
    //         buffer.erase(0, bytes_consumed);
    //     }
    // }

    // 修正：一次性读取整个文件到 buffer，确保连续字节流
    std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 逐步解析 buffer 中的 RESP 消息
    size_t offset = 0;
    while (offset < buffer.size()) {
        size_t consumed = 0;
        Client dummy_client;  // New: Create a dummy Client for AOF replay
        // 调用 process 解析当前 offset 开始的 RESP 命令（忽略 response，因为 replay 只重建 store）
        Command::process(std::string_view(buffer).substr(offset), consumed, *this, dummy_client);
        if (consumed == 0) {
            // 错误处理：如果无法消耗字节，记录日志并跳出（防止无限循环）
            std::cerr << "Error replaying AOF at offset " << offset
                      << ": invalid or incomplete RESP message\n";
            break;
        }
        offset += consumed;
    }

    // std::cout << "AOF replay completed successfully\n";

    file.close();
}
