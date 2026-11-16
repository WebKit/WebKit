/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/TinyLRUCache.h>

#include "Helpers/Test.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

struct CountingPolicy : public WTF::TinyLRUCachePolicy<int, int> {
    static unsigned createCount;
    static int createValueForKey(const int& key)
    {
        ++createCount;
        return key * 10;
    }
};
unsigned CountingPolicy::createCount = 0;

TEST(WTF_TinyLRUCache, GetCachesOnHit)
{
    CountingPolicy::createCount = 0;
    WTF::TinyLRUCache<int, int, 4, CountingPolicy> cache;
    EXPECT_EQ(0u, cache.size());

    EXPECT_EQ(10, cache.get(1));
    EXPECT_EQ(1u, CountingPolicy::createCount);
    EXPECT_EQ(1u, cache.size());

    EXPECT_EQ(10, cache.get(1));
    EXPECT_EQ(1u, CountingPolicy::createCount);
    EXPECT_EQ(1u, cache.size());

    EXPECT_EQ(20, cache.get(2));
    EXPECT_EQ(2u, CountingPolicy::createCount);
    EXPECT_EQ(2u, cache.size());
}

TEST(WTF_TinyLRUCache, GetEvictsLRU)
{
    CountingPolicy::createCount = 0;
    WTF::TinyLRUCache<int, int, 2, CountingPolicy> cache;

    cache.get(1);
    cache.get(2);
    EXPECT_EQ(2u, cache.size());

    cache.get(3); // evicts 1
    EXPECT_EQ(2u, cache.size());

    cache.get(2);
    EXPECT_EQ(3u, CountingPolicy::createCount);

    cache.get(1); // re-computed because evicted
    EXPECT_EQ(4u, CountingPolicy::createCount);
}

struct NullKeyPolicy : public WTF::TinyLRUCachePolicy<int, int> {
    static unsigned createCount;
    static unsigned createNullCount;
    static bool isKeyNull(const int& key) { return !key; }
    static int createValueForNullKey()
    {
        ++createNullCount;
        return -1;
    }
    static int createValueForKey(const int& key)
    {
        ++createCount;
        return key * 10;
    }
};
unsigned NullKeyPolicy::createCount = 0;
unsigned NullKeyPolicy::createNullCount = 0;

TEST(WTF_TinyLRUCache, GetWithNullKeyReturnsNullValueAndDoesNotStore)
{
    NullKeyPolicy::createCount = 0;
    NullKeyPolicy::createNullCount = 0;
    WTF::TinyLRUCache<int, int, 4, NullKeyPolicy> cache;

    EXPECT_EQ(-1, cache.get(0));
    EXPECT_EQ(0u, cache.size());
    EXPECT_EQ(0u, NullKeyPolicy::createCount);

    // Repeated null lookups must not grow the cache or re-create the null value.
    EXPECT_EQ(-1, cache.get(0));
    EXPECT_EQ(0u, cache.size());
    EXPECT_EQ(1u, NullKeyPolicy::createNullCount);

    // Non-null keys still work and are unaffected by null-key lookups.
    EXPECT_EQ(50, cache.get(5));
    EXPECT_EQ(1u, cache.size());
    EXPECT_EQ(1u, NullKeyPolicy::createCount);

    // findIfCached must miss for null keys regardless of prior get() calls.
    EXPECT_FALSE(cache.findIfCached(0));
}

TEST(WTF_TinyLRUCache, FindIfCachedReturnsNullOnMiss)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    EXPECT_FALSE(cache.findIfCached(42));
}

TEST(WTF_TinyLRUCache, FindIfCachedHitsAfterInsert)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    auto hit = cache.findIfCached(1);
    ASSERT_TRUE(hit);
    EXPECT_EQ(100, *hit);
    EXPECT_EQ(1u, cache.size());
}

TEST(WTF_TinyLRUCache, InsertEvictsLRU)
{
    WTF::TinyLRUCache<int, int, 2> cache;
    cache.insert(1, 100);
    cache.insert(2, 200);
    EXPECT_EQ(2u, cache.size());

    cache.insert(3, 300); // evicts 1
    EXPECT_EQ(2u, cache.size());
    EXPECT_FALSE(cache.findIfCached(1));
    auto two = cache.findIfCached(2);
    ASSERT_TRUE(two);
    EXPECT_EQ(200, *two);
    auto three = cache.findIfCached(3);
    ASSERT_TRUE(three);
    EXPECT_EQ(300, *three);
}

TEST(WTF_TinyLRUCache, FindIfCachedPromotesToMRU)
{
    WTF::TinyLRUCache<int, int, 3> cache;
    cache.insert(1, 100);
    cache.insert(2, 200);
    cache.insert(3, 300);

    // Promote 1 to MRU; subsequent insert should evict 2 (now LRU), not 1.
    EXPECT_TRUE(cache.findIfCached(1));

    cache.insert(4, 400);
    EXPECT_TRUE(cache.findIfCached(1));
    EXPECT_FALSE(cache.findIfCached(2));
    EXPECT_TRUE(cache.findIfCached(3));
    EXPECT_TRUE(cache.findIfCached(4));
}

TEST(WTF_TinyLRUCache, ClearReleasesEntriesAndResetsSize)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    cache.insert(2, 200);
    EXPECT_EQ(2u, cache.size());
    cache.clear();
    EXPECT_EQ(0u, cache.size());
    EXPECT_FALSE(cache.findIfCached(1));
    EXPECT_FALSE(cache.findIfCached(2));
}

class TrackedRefCounted : public RefCounted<TrackedRefCounted> {
public:
    static unsigned aliveCount;
    static Ref<TrackedRefCounted> create(int value) { return adoptRef(*new TrackedRefCounted(value)); }
    int value() const { return m_value; }
    ~TrackedRefCounted() { --aliveCount; }
private:
    TrackedRefCounted(int value) : m_value(value) { ++aliveCount; }
    int m_value;
};
unsigned TrackedRefCounted::aliveCount = 0;

TEST(WTF_TinyLRUCache, ClearReleasesRefsImmediately)
{
    TrackedRefCounted::aliveCount = 0;
    {
        WTF::TinyLRUCache<int, RefPtr<TrackedRefCounted>, 4> cache;
        cache.insert(1, TrackedRefCounted::create(100).ptr());
        cache.insert(2, TrackedRefCounted::create(200).ptr());
        EXPECT_EQ(2u, TrackedRefCounted::aliveCount);

        cache.clear();
        EXPECT_EQ(0u, TrackedRefCounted::aliveCount);
    }
}

TEST(WTF_TinyLRUCache, EvictionViaInsertReleasesRef)
{
    TrackedRefCounted::aliveCount = 0;
    {
        WTF::TinyLRUCache<int, RefPtr<TrackedRefCounted>, 2> cache;
        cache.insert(1, TrackedRefCounted::create(100).ptr());
        cache.insert(2, TrackedRefCounted::create(200).ptr());
        EXPECT_EQ(2u, TrackedRefCounted::aliveCount);

        // Inserting a third entry into a capacity-2 cache must drop the LRU's ref.
        cache.insert(3, TrackedRefCounted::create(300).ptr());
        EXPECT_EQ(2u, TrackedRefCounted::aliveCount);
    }
    EXPECT_EQ(0u, TrackedRefCounted::aliveCount);
}

TEST(WTF_TinyLRUCache, FindIfCachedDoesNotConsumeEntry)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    {
        auto hit = cache.findIfCached(1);
        ASSERT_TRUE(hit);
        EXPECT_EQ(100, *hit);
    }
    {
        auto hit = cache.findIfCached(1);
        ASSERT_TRUE(hit);
        EXPECT_EQ(100, *hit);
    }
    EXPECT_EQ(1u, cache.size());
}

TEST(WTF_TinyLRUCache, FindIfCachedAfterClearMisses)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    cache.clear();
    EXPECT_FALSE(cache.findIfCached(1));
    cache.insert(1, 999); // re-insert with different value
    auto hit = cache.findIfCached(1);
    ASSERT_TRUE(hit);
    EXPECT_EQ(999, *hit);
}

TEST(WTF_TinyLRUCache, FindOrComputeWorkflow)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    int externalContext = 7;

    auto findOrInsert = [&](int key) -> int {
        if (auto hit = cache.findIfCached(key))
            return *hit;
        int value = key * externalContext;
        cache.insert(key, WTF::move(value));
        auto hit = cache.findIfCached(key);
        return hit ? *hit : -1;
    };

    EXPECT_EQ(7, findOrInsert(1));
    EXPECT_EQ(14, findOrInsert(2));
    EXPECT_EQ(7, findOrInsert(1));
    EXPECT_EQ(14, findOrInsert(2));
    EXPECT_EQ(2u, cache.size());
}

TEST(WTF_TinyLRUCache, FindResultSafeAfterCacheDestroyed)
{
    WTF::TinyLRUCache<int, int, 4>* cache = new WTF::TinyLRUCache<int, int, 4>();
    cache->insert(1, 100);
    auto result = cache->findIfCached(1);
    ASSERT_TRUE(result);
    delete cache;
    EXPECT_FALSE(result);
}

TEST(WTF_TinyLRUCache, InsertInvalidatesFindResult)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    auto result = cache.findIfCached(1);
    ASSERT_TRUE(result);
    cache.insert(2, 200);
    EXPECT_FALSE(result);
}

TEST(WTF_TinyLRUCache, ClearInvalidatesFindResult)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    auto result = cache.findIfCached(1);
    ASSERT_TRUE(result);
    cache.clear();
    EXPECT_FALSE(result);
}

TEST(WTF_TinyLRUCache, SubsequentFindInvalidatesPriorFindResult)
{
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    cache.insert(2, 200);
    auto result = cache.findIfCached(2);
    ASSERT_TRUE(result);
    cache.findIfCached(1);
    EXPECT_FALSE(result);
}

TEST(WTF_TinyLRUCacheDeathTest, UseAfterInsertDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    auto result = cache.findIfCached(1);
    ASSERT_TRUE(result);
    cache.insert(2, 200);
    ASSERT_DEATH_IF_SUPPORTED(*result, "");
}

TEST(WTF_TinyLRUCacheDeathTest, UseAfterClearDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    auto result = cache.findIfCached(1);
    ASSERT_TRUE(result);
    cache.clear();
    ASSERT_DEATH_IF_SUPPORTED(*result, "");
}

TEST(WTF_TinyLRUCacheDeathTest, UseAfterSubsequentFindDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    WTF::TinyLRUCache<int, int, 4> cache;
    cache.insert(1, 100);
    cache.insert(2, 200);
    auto result = cache.findIfCached(2);
    ASSERT_TRUE(result);
    cache.findIfCached(1);
    ASSERT_DEATH_IF_SUPPORTED(*result, "");
}

} // namespace TestWebKitAPI
