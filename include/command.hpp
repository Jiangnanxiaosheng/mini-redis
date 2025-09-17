#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "client.hpp"
#include "store.hpp"

class Server;

class Command {
public:
    virtual ~Command() = default;
    virtual std::string execute(const std::vector<std::string_view>& tokens, Store& store,
                                Client& client) = 0;

    // using Handler = std::function<std::string(const std::vector<std::string_view>&, Store&,
    // Client&)>;

    static std::string process(std::string_view buffer, size_t& consumed, Store& store,
                               Client& client);

    static void registerCommand(std::string_view name,
                                std::function<std::unique_ptr<Command>()> creator);

    /**
     * 解析 Redis 序列化协议 (RESP) 格式的数组
     *
     * @param buffer 输入缓冲区，包含 RESP 格式数据
     * @param consumed 输出参数，表示成功解析消耗的字节数
     * @param result 输出参数，存储解析出的字符串元素
     * @return bool 解析成功返回 true，失败返回 false
     */
    static bool parseResp(std::string_view buffer, size_t& consumed,
                          std::vector<std::string_view>& result);
};

class CommandFactory {
public:
    static CommandFactory& getInstance();
    void registerCommand(std::string_view name, std::function<std::unique_ptr<Command>()> creator);
    std::unique_ptr<Command> createCommand(std::string_view name) const;

private:
    CommandFactory() = default;

    std::unordered_map<std::string_view, std::function<std::unique_ptr<Command>()>> creators_;
};

// 新增：向量命令
class VectorCommand : public Command {
public:
    std::string execute(const std::vector<std::string_view>& tokens, Store& store,
                        Client& client) override;
};