#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

class LRUCache {
public:
    explicit LRUCache(std::size_t capacity);

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);
    bool contains(const std::string& key) const;
    std::size_t size() const;
    void clear();
    std::size_t capacity() const;

private:
    using Entry = std::pair<std::string, std::string>;
    using EntryList = std::list<Entry>;

    void evictLeastRecentlyUsed();

    std::size_t capacity_;
    EntryList usageOrder_;
    std::unordered_map<std::string, EntryList::iterator> index_;
};
