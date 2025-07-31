#include "store.hpp"

void Store::set(const std::string& key, const std::string& value) { data_[key] = value; }

std::string Store::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return "";  // Return empty string for missing keys
}