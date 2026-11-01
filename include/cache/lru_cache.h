#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class LRUCache {
public:
    explicit LRUCache(std::size_t capacity);

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    std::size_t size() const;
    std::size_t capacity() const;

private:
    std::size_t capacity_;
    std::unordered_map<std::string, std::string> entries_;
};
