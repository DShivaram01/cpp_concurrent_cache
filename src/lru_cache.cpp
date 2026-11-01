#include "cache/lru_cache.h"

LRUCache::LRUCache(std::size_t capacity) : capacity_(capacity) {}

void LRUCache::put(const std::string& key, const std::string& value) {
    if (capacity_ == 0) {
        return;
    }

    auto it = index_.find(key);
    if (it != index_.end()) {
        it->second->second = value;
        // Move the item to the front because it was just used
        usageOrder_.splice(usageOrder_.begin(), usageOrder_, it->second);
        return;
    }

    if (usageOrder_.size() >= capacity_) {
        evictLeastRecentlyUsed();
    }

    usageOrder_.emplace_front(key, value);
    index_[key] = usageOrder_.begin();
}

std::optional<std::string> LRUCache::get(const std::string& key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
        return std::nullopt;
    }

    usageOrder_.splice(usageOrder_.begin(), usageOrder_, it->second);
    return it->second->second;
}

bool LRUCache::remove(const std::string& key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
        return false;
    }

    usageOrder_.erase(it->second);
    index_.erase(it);
    return true;
}

bool LRUCache::contains(const std::string& key) const {
    return index_.find(key) != index_.end();
}

std::size_t LRUCache::size() const {
    return usageOrder_.size();
}

void LRUCache::clear() {
    usageOrder_.clear();
    index_.clear();
}

std::size_t LRUCache::capacity() const {
    return capacity_;
}

void LRUCache::evictLeastRecentlyUsed() {
    const Entry& oldest = usageOrder_.back();
    index_.erase(oldest.first);
    usageOrder_.pop_back();
}
