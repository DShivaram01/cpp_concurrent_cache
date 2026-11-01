#include <gtest/gtest.h>

#include "cache/lru_cache.h"

TEST(LRUCacheTest, NewCacheIsEmpty) {
    LRUCache cache(3);
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.capacity(), 3u);
}

TEST(LRUCacheTest, GetMissingKeyReturnsEmpty) {
    LRUCache cache(3);
    EXPECT_FALSE(cache.get("missing").has_value());
}

TEST(LRUCacheTest, PutThenGetReturnsValue) {
    LRUCache cache(3);
    cache.put("a", "1");

    auto value = cache.get("a");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "1");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, PutExistingKeyOverwritesValue) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("a", "2");

    EXPECT_EQ(cache.get("a"), "2");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, SizeGrowsUpToCapacity) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");

    EXPECT_EQ(cache.size(), 3u);
    EXPECT_EQ(cache.get("a"), "1");
    EXPECT_EQ(cache.get("b"), "2");
    EXPECT_EQ(cache.get("c"), "3");
}

TEST(LRUCacheTest, SizeNeverExceedsCapacity) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");

    EXPECT_EQ(cache.size(), 2u);
}

TEST(LRUCacheTest, UpdateWhenFullStillWorks) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("a", "10");

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.get("a"), "10");
}

TEST(LRUCacheTest, ZeroCapacityStoresNothing) {
    LRUCache cache(0);
    cache.put("a", "1");

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.get("a").has_value());
}

TEST(LRUCacheTest, EmptyStringKeyAndValue) {
    LRUCache cache(2);
    cache.put("", "");

    auto value = cache.get("");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "");
}
