#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "cache/lru_cache.h"

namespace {

const int kThreadCount = 8;

std::string makeKey(int id) {
    return "key" + std::to_string(id);
}

std::string makeValue(const std::string& key, int version) {
    return key + ":" + std::to_string(version);
}

bool valueBelongsToKey(const std::string& value, const std::string& key) {
    return value.rfind(key + ":", 0) == 0;
}

int countPresentKeys(const LRUCache& cache, int keySpace) {
    int count = 0;
    for (int i = 0; i < keySpace; ++i) {
        if (cache.contains(makeKey(i))) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST(ConcurrentCacheTest, DisjointPutsAreAllStored) {
    const int perThread = 1000;
    LRUCache cache(kThreadCount * perThread);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < perThread; ++i) {
                std::string key = makeKey(t * perThread + i);
                cache.put(key, makeValue(key, 0));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(cache.size(), static_cast<std::size_t>(kThreadCount * perThread));
    for (int i = 0; i < kThreadCount * perThread; ++i) {
        std::string key = makeKey(i);
        EXPECT_EQ(cache.get(key), makeValue(key, 0));
    }
}

TEST(ConcurrentCacheTest, EvictionUnderContentionRespectsCapacity) {
    const std::size_t capacity = 50;
    const int perThread = 2000;
    LRUCache cache(capacity);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < perThread; ++i) {
                std::string key = makeKey(t * perThread + i);
                cache.put(key, makeValue(key, 0));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(cache.size(), capacity);
    EXPECT_EQ(countPresentKeys(cache, kThreadCount * perThread), static_cast<int>(capacity));
}

TEST(ConcurrentCacheTest, MixedOperationsKeepStateValid) {
    const std::size_t capacity = 64;
    const int keySpace = 256;
    const int perThread = 20000;
    LRUCache cache(capacity);

    std::vector<std::thread> threads;
    std::vector<int> badValues(kThreadCount, 0);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&cache, &badValues, t]() {
            for (int i = 0; i < perThread; ++i) {
                std::string key = makeKey((i * 31 + t * 7) % keySpace);
                switch (i % 5) {
                    case 0:
                    case 1:
                        cache.put(key, makeValue(key, i));
                        break;
                    case 2: {
                        auto value = cache.get(key);
                        if (value && !valueBelongsToKey(*value, key)) {
                            ++badValues[t];
                        }
                        break;
                    }
                    case 3:
                        cache.contains(key);
                        break;
                    case 4:
                        if (i % 50 == 4) {
                            cache.remove(key);
                        } else {
                            cache.size();
                        }
                        break;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    for (int t = 0; t < kThreadCount; ++t) {
        EXPECT_EQ(badValues[t], 0) << "thread " << t;
    }
    EXPECT_LE(cache.size(), capacity);
    EXPECT_EQ(countPresentKeys(cache, keySpace), static_cast<int>(cache.size()));
    for (int i = 0; i < keySpace; ++i) {
        std::string key = makeKey(i);
        auto value = cache.get(key);
        if (value) {
            EXPECT_TRUE(valueBelongsToKey(*value, key)) << key << " -> " << *value;
        }
    }
}

TEST(ConcurrentCacheTest, ReadersSeeOnlyWrittenValuesForSameKey) {
    LRUCache cache(4);
    const std::string key = "shared";
    cache.put(key, makeValue(key, 0));

    const int writes = 20000;
    std::vector<std::thread> threads;
    threads.emplace_back([&cache, &key]() {
        for (int i = 1; i <= writes; ++i) {
            cache.put(key, makeValue(key, i));
        }
    });

    std::vector<int> badReads(kThreadCount - 1, 0);
    for (int t = 0; t < kThreadCount - 1; ++t) {
        threads.emplace_back([&cache, &key, &badReads, t]() {
            for (int i = 0; i < writes; ++i) {
                auto value = cache.get(key);
                if (!value || !valueBelongsToKey(*value, key)) {
                    ++badReads[t];
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    for (int t = 0; t < kThreadCount - 1; ++t) {
        EXPECT_EQ(badReads[t], 0) << "reader " << t;
    }
    EXPECT_EQ(cache.get(key), makeValue(key, writes));
}

TEST(ConcurrentCacheTest, ClearWhileWritingDoesNotCorruptState) {
    const std::size_t capacity = 32;
    const int keySpace = 128;
    LRUCache cache(capacity);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount - 1; ++t) {
        threads.emplace_back([&cache, t]() {
            for (int i = 0; i < 10000; ++i) {
                std::string key = makeKey((i + t) % keySpace);
                cache.put(key, makeValue(key, i));
            }
        });
    }
    threads.emplace_back([&cache]() {
        for (int i = 0; i < 200; ++i) {
            cache.clear();
        }
    });
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_LE(cache.size(), capacity);
    EXPECT_EQ(countPresentKeys(cache, keySpace), static_cast<int>(cache.size()));
}
