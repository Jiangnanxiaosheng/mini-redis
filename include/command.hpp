#pragma once
#include <string>

#include "store.hpp"

class Command {
public:
    static std::string process(const std::string& cmd, Store& store);
};