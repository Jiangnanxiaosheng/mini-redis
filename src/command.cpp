#include "command.hpp"

#include <sstream>

std::string Command::process(const std::string& cmd, Store& store) {
    if (cmd.find("SET ") == 0) {
        std::istringstream iss(cmd);
        std::string op, key, value;
        iss >> op >> key >> value;
        if (!key.empty() && !value.empty()) {
            store.set(key, value);
            return "+OK\r\n";
        }
        return "-ERR invalid SET command\r\n";
    } else if (cmd.find("GET ") == 0) {
        std::istringstream iss(cmd);
        std::string op, key;
        iss >> op >> key;
        if (!key.empty()) {
            std::string value = store.get(key);
            if (!value.empty()) {
                return "+" + value + "\r\n";
            }
            return "$-1\r\n";  // Nil response
        }
        return "-ERR invalid GET command\r\n";
    }
    return "-ERR unknown command\r\n";
}