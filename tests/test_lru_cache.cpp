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

TEST(LRUCacheTest, EmptyCacheOperations) {
    LRUCache cache(3);
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_FALSE(cache.remove("a"));
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
}

TEST(LRUCacheTest, ContainsReportsPresence) {
    LRUCache cache(2);
    cache.put("a", "1");

    EXPECT_TRUE(cache.contains("a"));
    EXPECT_FALSE(cache.contains("b"));
}

TEST(LRUCacheTest, ContainsDoesNotChangeUsageOrder) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    EXPECT_TRUE(cache.contains("a"));
    cache.put("c", "3");

    EXPECT_FALSE(cache.contains("a"));
    EXPECT_TRUE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
}

TEST(LRUCacheTest, RemoveExistingKey) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");

    EXPECT_TRUE(cache.remove("a"));
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_FALSE(cache.get("a").has_value());
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, RemoveMissingKeyReturnsFalse) {
    LRUCache cache(3);
    cache.put("a", "1");

    EXPECT_FALSE(cache.remove("b"));
    EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, RemoveTwiceReturnsFalseSecondTime) {
    LRUCache cache(3);
    cache.put("a", "1");

    EXPECT_TRUE(cache.remove("a"));
    EXPECT_FALSE(cache.remove("a"));
}

TEST(LRUCacheTest, RemoveFreesSpaceWithoutEviction) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.remove("a");
    cache.put("c", "3");

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_TRUE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
}

TEST(LRUCacheTest, RemoveLeastRecentlyUsedThenEvictNext) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");
    cache.remove("a");
    cache.put("d", "4");
    cache.put("e", "5");

    EXPECT_FALSE(cache.contains("b"));
    EXPECT_TRUE(cache.contains("c"));
    EXPECT_TRUE(cache.contains("d"));
    EXPECT_TRUE(cache.contains("e"));
}

TEST(LRUCacheTest, ClearEmptiesCache) {
    LRUCache cache(3);
    cache.put("a", "1");
    cache.put("b", "2");
    cache.clear();

    EXPECT_EQ(cache.size(), 0u);
    EXPECT_FALSE(cache.contains("a"));
    EXPECT_FALSE(cache.get("b").has_value());
}

TEST(LRUCacheTest, CacheIsUsableAfterClear) {
    LRUCache cache(2);
    cache.put("a", "1");
    cache.clear();
    cache.put("b", "2");
    cache.put("c", "3");
    cache.put("d", "4");

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_FALSE(cache.contains("b"));
    EXPECT_EQ(cache.get("d"), "4");
}

TEST(LRUCacheTest, RepeatedAccessKeepsItemAlive) {
    LRUCache cache(3);
    cache.put("hot", "x");
    for (int i = 0; i < 100; ++i) {
        cache.put("key" + std::to_string(i), std::to_string(i));
        EXPECT_EQ(cache.get("hot"), "x");
    }

    EXPECT_EQ(cache.size(), 3u);
    EXPECT_TRUE(cache.contains("key99"));
    EXPECT_TRUE(cache.contains("key98"));
    EXPECT_FALSE(cache.contains("key97"));
}

TEST(LRUCacheTest, ManyInsertsKeepOnlyNewestEntries) {
    const int capacity = 100;
    const int inserts = 10000;
    LRUCache cache(capacity);

    for (int i = 0; i < inserts; ++i) {
        cache.put("key" + std::to_string(i), std::to_string(i));
    }

    EXPECT_EQ(cache.size(), static_cast<std::size_t>(capacity));
    for (int i = 0; i < inserts; ++i) {
        bool shouldExist = i >= inserts - capacity;
        EXPECT_EQ(cache.contains("key" + std::to_string(i)), shouldExist) << "key" << i;
    }
}

TEST(LRUCacheTest, RemoveOnZeroCapacityCache) {
    LRUCache cache(0);
    cache.put("a", "1");

    EXPECT_FALSE(cache.contains("a"));
    EXPECT_FALSE(cache.remove("a"));
}
