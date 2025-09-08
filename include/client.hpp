#pragma once
#include <string>
#include <vector>

#include "ring_buffer.hpp"

struct Client {
    RingBuffer buffer;
    std::string response;
    bool has_pending_write{false};

    bool in_transaction{false};  // 事务状态
    std::vector<std::vector<std::string>> transaction_queue;
};