#include "command.hpp"

#include <sstream>
#include <vector>

// Initialize static member
std::unordered_map<std::string_view, Command::Handler> Command::handlers_;

void Command::registerCommand(std::string_view name, Handler handler) { handlers_[name] = handler; }

// Helper to split command into tokens
static std::vector<std::string_view> tokenize(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = input.find(' ');
    while (end != std::string_view::npos) {
        if (end > start) {
            tokens.push_back(input.substr(start, end - start));
        }
        start = end + 1;
        end = input.find(' ', start);
    }
    if (start < input.size()) {
        tokens.push_back(input.substr(start));
    }
    return tokens;
}

std::string Command::process(const std::string& cmd, Store& store) {
    // Trim trailing \r\n or whitespace
    std::string_view input(cmd);
    input.remove_suffix(input.ends_with("\r\n") ? 2 : 0);

    // Tokenize command
    auto tokens = tokenize(input);
    if (tokens.empty()) {
        return "-ERR empty command\r\n";
    }

    // Look up handler
    auto it = handlers_.find(tokens[0]);
    if (it == handlers_.end()) {
        return "-ERR unknown command '" + std::string(tokens[0]) + "'\r\n";
    }

    // Execute handler
    return it->second(tokens, store);
}

// Register SET and GET commands
namespace {

    std::string setCommand(const std::vector<std::string_view>& tokens, Store& store) {
        if (tokens.size() != 3) {
            return "-ERR wrong number of arguments for 'SET' command\r\n";
        }
        store.set(std::string(tokens[1]), std::string(tokens[2]));
        return "+OK\r\n";
    }

    std::string getCommand(const std::vector<std::string_view>& tokens, Store& store) {
        if (tokens.size() != 2) {
            return "-ERR wrong number of arguments for 'GET' command\r\n";
        }
        std::string value = store.get(std::string(tokens[1]));
        if (value.empty()) {
            return "$-1\r\n";
        }
        return "+" + value + "\r\n";
    }

    // Static registration
    struct CommandInitializer {
        CommandInitializer() {
            Command::registerCommand("SET", setCommand);
            Command::registerCommand("GET", getCommand);
        }
    } initializer;
}