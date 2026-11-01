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

TEST(LRUCacheTest, AboveCapacityEvictsLeastRecentlyUsed) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");

    EXPECT_FALSE(cache.get("a").has_value());
    EXPECT_EQ(cache.get("b"), "2");
    EXPECT_EQ(cache.get("c"), "3");
}

TEST(LRUCacheTest, RecentlyAccessedItemIsNotEvicted) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.get("a");
    cache.put("c", "3");

    EXPECT_EQ(cache.get("a"), "1");
    EXPECT_FALSE(cache.get("b").has_value());
    EXPECT_EQ(cache.get("c"), "3");
}

TEST(LRUCacheTest, UpdatedItemBecomesMostRecentlyUsed) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("a", "10");
    cache.put("c", "3");

    EXPECT_EQ(cache.get("a"), "10");
    EXPECT_FALSE(cache.get("b").has_value());
    EXPECT_EQ(cache.get("c"), "3");
}

TEST(LRUCacheTest, CapacityOneKeepsOnlyLatest) {
    LRUCache cache(1);
    cache.put("a", "1");
    cache.put("b", "2");

    EXPECT_EQ(cache.size(), 1u);
    EXPECT_FALSE(cache.get("a").has_value());
    EXPECT_EQ(cache.get("b"), "2");

    cache.put("b", "20");
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.get("b"), "20");
}

TEST(LRUCacheTest, DuplicateKeyUpdateDoesNotEvict) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");
    cache.put("c", "30");
    cache.put("c", "300");

    EXPECT_EQ(cache.size(), 3u);
    EXPECT_EQ(cache.get("a"), "1");
    EXPECT_EQ(cache.get("b"), "2");
    EXPECT_EQ(cache.get("c"), "300");
}

TEST(LRUCacheTest, EvictionOrderFollowsUsage) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");
    cache.get("a");
    cache.get("b");

    cache.put("d", "4");
    EXPECT_FALSE(cache.get("c").has_value());

    cache.put("e", "5");
    EXPECT_FALSE(cache.get("a").has_value());

    EXPECT_EQ(cache.get("b"), "2");
    EXPECT_EQ(cache.get("d"), "4");
    EXPECT_EQ(cache.get("e"), "5");
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
