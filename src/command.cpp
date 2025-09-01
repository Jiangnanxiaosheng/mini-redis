#include "command.hpp"

#include <sstream>
#include <vector>

// Initialize static member
std::unordered_map<std::string_view, Command::Handler> Command::handlers_;

void Command::registerCommand(std::string_view name, Handler handler) { handlers_[name] = handler; }

bool Command::parseResp(std::string_view buffer, size_t& consumed,
                        std::vector<std::string_view>& result) {
    consumed = 0;  // 初始化消耗字节数为0

    // 检查缓冲区是否为空或不是数组格式（RESP数组以'*'开头）
    if (buffer.empty() || buffer[0] != '*') {
        return false;  // 不是有效的RESP数组格式
    }

    // 解析数组长度（查找第一个\r\n）
    size_t pos = buffer.find("\r\n");
    if (pos == std::string_view::npos) {
        return false;  // 数据不完整，没有找到行结束符
    }

    // 提取长度字符串（跳过开头的'*'）
    std::string_view len_str = buffer.substr(1, pos - 1);
    int len;
    try {
        // 将字符串转换为整数（数组元素个数）
        len = std::stoi(std::string(len_str));
    } catch (...) {
        return false;  // 长度格式无效
    }

    // 检查数组长度有效性
    if (len < 0) {
        return false;  // 长度不能为负数（NULL数组用*-1\r\n表示，但这里不支持）
    }

    // 更新已消耗的字节数（数组长度行 + \r\n）
    consumed = pos + 2;

    // 解析数组中的每个元素（应为批量字符串格式）
    result.clear();  // 清空结果容器
    for (int i = 0; i < len; ++i) {
        // 检查是否还有数据且当前元素是否为批量字符串（以'$'开头）
        if (consumed >= buffer.size() || buffer[consumed] != '$') {
            return false;  // 数据不足或不是批量字符串格式
        }

        // 查找当前批量字符串的长度行结束符
        pos = buffer.find("\r\n", consumed);
        if (pos == std::string_view::npos) {
            return false;  // 数据不完整
        }

        // 提取字符串长度（跳过开头的'$'）
        std::string_view str_len = buffer.substr(consumed + 1, pos - consumed - 1);
        int str_size;
        try {
            // 将字符串长度转换为整数
            str_size = std::stoi(std::string(str_len));
        } catch (...) {
            return false;  // 字符串长度格式无效
        }

        // 更新消耗字节数（字符串长度行 + \r\n）
        consumed = pos + 2;

        // 检查字符串数据是否完整
        if (str_size < 0 || consumed + str_size + 2 > buffer.size()) {
            return false;  // 字符串长度无效或数据不完整
        }

        // 提取实际的字符串数据（跳过长度说明，直接获取内容）
        result.push_back(buffer.substr(consumed, str_size));

        // 更新消耗字节数（字符串内容 + 结尾的\r\n）
        consumed += str_size + 2;
    }

    return true;  // 解析成功
}

std::string Command::process(std::string_view buffer, size_t& consumed, Store& store) {
    std::vector<std::string_view> tokens;
    if (!parseResp(buffer, consumed, tokens)) {
        return "-ERR invalid RESP protocol\r\n";
    }

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
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }

    // Static registration
    struct CommandInitializer {
        CommandInitializer() {
            Command::registerCommand("SET", setCommand);
            Command::registerCommand("GET", getCommand);
        }
    } initializer;
}