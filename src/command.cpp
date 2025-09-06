#include "command.hpp"

#include <sstream>

/*
void Command::registerCommand(std::string_view name, Handler handler) { handlers_[name] = handler; }
*/

bool Command::parseResp(std::string_view buffer, size_t& consumed,
                        std::vector<std::string_view>& result) {
    consumed = 0;
    if (buffer.empty() || buffer[0] != '*') {
        return false;
    }

    size_t pos = buffer.find("\r\n");
    if (pos == std::string_view::npos) {
        return false;
    }
    std::string_view len_str = buffer.substr(1, pos - 1);
    int len;
    try {
        len = std::stoi(std::string(len_str));
    } catch (...) {
        return false;
    }
    if (len < 0) {
        return false;
    }
    consumed = pos + 2;

    result.clear();
    for (int i = 0; i < len; ++i) {
        if (consumed >= buffer.size() || buffer[consumed] != '$') {
            return false;
        }
        pos = buffer.find("\r\n", consumed);
        if (pos == std::string_view::npos) {
            return false;
        }
        std::string_view str_len = buffer.substr(consumed + 1, pos - consumed - 1);
        int str_size;
        try {
            str_size = std::stoi(std::string(str_len));
        } catch (...) {
            return false;
        }
        consumed = pos + 2;
        if (str_size < 0 || consumed + str_size + 2 > buffer.size()) {
            return false;
        }
        result.push_back(buffer.substr(consumed, str_size));
        consumed += str_size + 2;
    }
    return true;
}

std::string Command::process(std::string_view buffer, size_t& consumed, Store& store,
                             Client& client) {
    std::vector<std::string_view> tokens;
    if (!parseResp(buffer, consumed, tokens)) {
        return "-ERR invalid RESP protocol\r\n";
    }
    if (tokens.empty()) {
        return "-ERR empty command\r\n";
    }

    if (tokens[0] == "MULTI") {
        if (client.in_transaction) {
            return "-ERR MULTI calls can not be nested\r\n";
        }
        client.in_transaction = true;
        client.transaction_queue.clear();
        return "+OK\r\n";
    }

    if (tokens[0] == "EXEC") {
        if (!client.in_transaction) {
            return "-ERR EXEC without MULTI\r\n";
        }
        client.in_transaction = false;
        if (client.transaction_queue.empty()) {
            return "*0\r\n";
        }
        std::string response = "*" + std::to_string(client.transaction_queue.size()) + "\r\n";
        for (const auto& cmd : client.transaction_queue) {
            // auto it = handlers_.find(cmd[0]);
            auto command = CommandFactory::getInstance().createCommand(cmd[0]);
            if (!command /* it == handlers_.end()*/) {
                response += "-ERR unknown command '" + cmd[0] + "'\r\n";
                continue;
            }
            if (cmd[0] == "MULTI" || cmd[0] == "EXEC" || cmd[0] == "DISCARD") {
                response += "-ERR command '" + cmd[0] + "' not allowed in transaction\r\n";
                continue;
            }
            std::vector<std::string_view> cmd_view;
            cmd_view.reserve(cmd.size());
            for (const auto& s : cmd) {
                cmd_view.push_back(s);
            }
            // response += it->second(cmd_view, store, client);
            response += command->execute(cmd_view, store, client);
        }
        client.transaction_queue.clear();
        return response;
    }

    if (tokens[0] == "DISCARD") {
        if (!client.in_transaction) {
            return "-ERR DISCARD without MULTI\r\n";
        }
        client.in_transaction = false;
        client.transaction_queue.clear();
        return "+OK\r\n";
    }

    if (client.in_transaction) {
        // auto it = handlers_.find(tokens[0]);
        auto command = CommandFactory::getInstance().createCommand(tokens[0]);
        if (!command /* it == handlers_.end()*/) {
            std::vector<std::string> owned_tokens;
            owned_tokens.reserve(tokens.size());
            for (const auto& t : tokens) {
                owned_tokens.emplace_back(t);
            }
            client.transaction_queue.push_back(std::move(owned_tokens));
            return "+QUEUED\r\n";
        }
        if (tokens[0] == "MULTI" || tokens[0] == "EXEC" || tokens[0] == "DISCARD") {
            return "-ERR command '" + std::string(tokens[0]) + "' not allowed in transaction\r\n";
        }
        if ((tokens[0] == "SET" && tokens.size() != 3) ||
            (tokens[0] == "GET" && tokens.size() != 2) ||
            (tokens[0] == "EXPIRE" && tokens.size() != 3)) {
            return "-ERR wrong number of arguments for '" + std::string(tokens[0]) +
                   "' command\r\n";
        }
        if (tokens[0] == "EXPIRE") {
            try {
                std::stoi(std::string(tokens[2]));
            } catch (...) {
                return "-ERR value is not an integer or out of range\r\n";
            }
        }
        std::vector<std::string> owned_tokens;
        owned_tokens.reserve(tokens.size());
        for (const auto& t : tokens) {
            owned_tokens.emplace_back(t);
        }
        client.transaction_queue.push_back(std::move(owned_tokens));
        return "+QUEUED\r\n";
    }

    // auto it = handlers_.find(tokens[0]);
    auto command = CommandFactory::getInstance().createCommand(tokens[0]);
    if (!command /* it == handlers_.end()*/) {
        return "-ERR unknown command '" + std::string(tokens[0]) + "'\r\n";
    }

    // return it->second(tokens, store, client);
    return command->execute(tokens, store, client);
}

void Command::registerCommand(std::string_view name,
                              std::function<std::unique_ptr<Command>()> creator) {
    CommandFactory::getInstance().registerCommand(name, std::move(creator));
}

CommandFactory& CommandFactory::getInstance() {
    static CommandFactory instance;
    return instance;
}

void CommandFactory::registerCommand(std::string_view name,
                                     std::function<std::unique_ptr<Command>()> creator) {
    creators_[name] = std::move(creator);
}

std::unique_ptr<Command> CommandFactory::createCommand(std::string_view name) const {
    auto it = creators_.find(name);
    if (it == creators_.end()) {
        return nullptr;
    }
    return it->second();
}

namespace {
    class SetCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store& store,
                            Client&) override {
            if (tokens.size() != 3) {
                return "-ERR wrong number of arguments for 'SET' command\r\n";
            }
            store.set(std::string(tokens[1]), std::string(tokens[2]));
            return "+OK\r\n";
        }
    };

    class GetCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store& store,
                            Client&) override {
            if (tokens.size() != 2) {
                return "-ERR wrong number of arguments for 'GET' command\r\n";
            }
            std::string value = store.get(std::string(tokens[1]));
            if (value.empty()) {
                return "$-1\r\n";
            }
            return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
        }
    };

    class ExpireCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store& store,
                            Client&) override {
            if (tokens.size() != 3) {
                return "-ERR wrong number of arguments for 'EXPIRE' command\r\n";
            }
            int seconds;
            try {
                seconds = std::stoi(std::string(tokens[2]));
            } catch (...) {
                return "-ERR value is not an integer or out of range\r\n";
            }
            bool success = store.setExpire(std::string(tokens[1]), seconds);
            return success ? ":1\r\n" : ":0\r\n";
        }
    };

    class MultiCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store&, Client&) override {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'MULTI' command\r\n";
            }
            return "+OK\r\n";
        }
    };

    class ExecCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store&, Client&) override {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'EXEC' command\r\n";
            }
            return "+OK\r\n";
        }
    };

    class DiscardCommand : public Command {
    public:
        std::string execute(const std::vector<std::string_view>& tokens, Store&, Client&) override {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'DISCARD' command\r\n";
            }
            return "+OK\r\n";
        }
    };

    struct CommandInitializer {
        CommandInitializer() {
            Command::registerCommand("SET", []() { return std::make_unique<SetCommand>(); });
            Command::registerCommand("GET", []() { return std::make_unique<GetCommand>(); });
            Command::registerCommand("EXPIRE", []() { return std::make_unique<ExpireCommand>(); });
            Command::registerCommand("MULTI", []() { return std::make_unique<MultiCommand>(); });
            Command::registerCommand("EXEC", []() { return std::make_unique<ExecCommand>(); });
            Command::registerCommand("DISCARD",
                                     []() { return std::make_unique<DiscardCommand>(); });
        }
    } initializer;

#if 0
        std::string setCommand(const std::vector<std::string_view>& tokens, Store& store, Client&) {
            if (tokens.size() != 3) {
                return "-ERR wrong number of arguments for 'SET' command\r\n";
            }
            store.set(std::string(tokens[1]), std::string(tokens[2]));
            return "+OK\r\n";
        }

        std::string getCommand(const std::vector<std::string_view>& tokens, Store& store, Client&) {
            if (tokens.size() != 2) {
                return "-ERR wrong number of arguments for 'GET' command\r\n";
            }
            std::string value = store.get(std::string(tokens[1]));
            if (value.empty()) {
                return "$-1\r\n";
            }
            return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
        }

        std::string expireCommand(const std::vector<std::string_view>& tokens, Store& store,
            Client&) { 
            if (tokens.size() != 3) {
                return "-ERR wrong number of arguments for 'EXPIRE' command\r\n";
            }
            int seconds;
            try {
                seconds = std::stoi(std::string(tokens[2]));
            } catch (...) {
                return "-ERR value is not an integer or out of range\r\n";
            }
            bool success = store.setExpire(std::string(tokens[1]), seconds);
            return success ? ":1\r\n" : ":0\r\n";
        }

        std::string multiCommand(const std::vector<std::string_view>& tokens, Store&, Client&) {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'MULTI' command\r\n";
            }
            return "+OK\r\n";
        }

        std::string execCommand(const std::vector<std::string_view>& tokens, Store&, Client&) {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'EXEC' command\r\n";
            }
            return "+OK\r\n";
        }

        std::string discardCommand(const std::vector<std::string_view>& tokens, Store&, Client&) {
            if (tokens.size() != 1) {
                return "-ERR wrong number of arguments for 'DISCARD' command\r\n";
            }
            return "+OK\r\n";
        }

        struct CommandInitializer {
            CommandInitializer() {
                Command::registerCommand("SET", setCommand);
                Command::registerCommand("GET", getCommand);
                Command::registerCommand("EXPIRE", expireCommand);
                Command::registerCommand("MULTI", multiCommand);
                Command::registerCommand("EXEC", execCommand);
                Command::registerCommand("DISCARD", discardCommand);
            }
        } initializer;

#endif
}