#pragma once
#include <functional>
#include <string>
#include <string_view>

#include "store.hpp"

class Command {
public:
    using Handler = std::function<std::string(std::vector<std::string_view>, Store&)>;

    static std::string process(const std::string& cmd, Store& store);

    static void registerCommand(std::string_view name, Handler handler);

private:
    static std::unordered_map<std::string_view, Handler> handlers_;
};
