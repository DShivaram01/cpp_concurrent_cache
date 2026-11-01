#include "cache/lru_cache.h"

LRUCache::LRUCache(std::size_t capacity) : capacity_(capacity) {}

void LRUCache::put(const std::string& key, const std::string& value) {
    if (capacity_ == 0) {
        return;
    }

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        it->second = value;
        return;
    }

    // Eviction order is not tracked yet, so new keys are rejected when full
    if (entries_.size() >= capacity_) {
        return;
    }

    entries_.emplace(key, value);
}

std::optional<std::string> LRUCache::get(const std::string& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t LRUCache::size() const {
    return entries_.size();
}

std::size_t LRUCache::capacity() const {
    return capacity_;
}
