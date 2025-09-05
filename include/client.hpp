#pragma once
#include <string>
#include <string_view>
#include <vector>

struct Client {
    std::string buffer;
    std::string response;
    bool has_pending_write{false};

    bool in_transaction{false};  // 事务状态
    std::vector<std::vector<std::string>> transaction_queue;
};