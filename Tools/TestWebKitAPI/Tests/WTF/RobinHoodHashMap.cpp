/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "Counters.h"
#include "DeletedAddressOfOperator.h"
#include "MoveOnly.h"
#include "RefLogger.h"
#include "Test.h"
#include <string>
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

template<typename T>
struct RobinHoodHash : public DefaultHash<T> {
    static constexpr bool hasHashInValue = true;
};

typedef WTF::MemoryCompactLookupOnlyRobinHoodHashMap<int, int, RobinHoodHash<int>> IntHashMap;

TEST(WTF_RobinHoodHashMap, HashTableIteratorComparison)
{
    IntHashMap map;
    map.add(1, 2);
    ASSERT_TRUE(map.begin() != map.end());
    ASSERT_FALSE(map.begin() == map.end());

    IntHashMap::const_iterator begin = map.begin();
    ASSERT_TRUE(begin == map.begin());
    ASSERT_TRUE(map.begin() == begin);
    ASSERT_TRUE(begin != map.end());
    ASSERT_TRUE(map.end() != begin);
    ASSERT_FALSE(begin != map.begin());
    ASSERT_FALSE(map.begin() != begin);
    ASSERT_FALSE(begin == map.end());
    ASSERT_FALSE(map.end() == begin);
}

struct TestDoubleHashTraits : HashTraits<double> {
    static const int minimumTableSize = 8;
};

typedef MemoryCompactLookupOnlyRobinHoodHashMap<double, int64_t, RobinHoodHash<double>, TestDoubleHashTraits> DoubleHashMap;

static int bucketForKey(double key)
{
    return DefaultHash<double>::hash(key) & (TestDoubleHashTraits::minimumTableSize - 1);
}

template<typename T> struct BigTableHashTraits : public HashTraits<T> {
    static const int minimumTableSize = WTF::HashTableCapacityForSize<4096>::value;
};

template<typename T> struct ZeroHash : public IntHash<T> {
    static unsigned hash(const T&) { return 0; }
    static constexpr bool hasHashInValue = true;
};

TEST(WTF_RobinHoodHashMap, DoubleHashCollisions)
{
    // The "clobber" key here is one that ends up stealing the bucket that the -0 key
    // originally wants to be in. This makes the 0 and -0 keys collide and the test then
    // fails unless the FloatHash::equals() implementation can distinguish them.
    const double clobberKey = 6;
    const double zeroKey = 0;
    const double negativeZeroKey = -zeroKey;

    DoubleHashMap map;
    map.add(clobberKey, 1);
    map.add(zeroKey, 2);
    map.add(negativeZeroKey, 3);

    ASSERT_EQ(bucketForKey(clobberKey), bucketForKey(negativeZeroKey));
    ASSERT_EQ(map.get(clobberKey), 1);
    ASSERT_EQ(map.get(zeroKey), 2);
    ASSERT_EQ(map.get(negativeZeroKey), 3);
}

TEST(WTF_RobinHoodHashMap, MoveOnlyValues)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, MoveOnly, RobinHoodHash<unsigned>> moveOnlyValues;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyValues.set(i + 1, WTFMove(moveOnly));
    }

    for (size_t i = 0; i < 100; ++i) {
        auto it = moveOnlyValues.find(i + 1);
        ASSERT_FALSE(it == moveOnlyValues.end());
    }

    for (size_t i = 0; i < 50; ++i)
        ASSERT_EQ(moveOnlyValues.take(i + 1).value(), i + 1);

    for (size_t i = 50; i < 100; ++i)
        ASSERT_TRUE(moveOnlyValues.remove(i + 1));

    ASSERT_TRUE(moveOnlyValues.isEmpty());
}

TEST(WTF_RobinHoodHashMap, MoveOnlyKeys)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<MoveOnly, unsigned> moveOnlyKeys;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyKeys.set(WTFMove(moveOnly), i + 1);
    }

    for (size_t i = 0; i < 100; ++i) {
        auto it = moveOnlyKeys.find(MoveOnly(i + 1));
        ASSERT_FALSE(it == moveOnlyKeys.end());
    }

    for (size_t i = 0; i < 100; ++i)
        ASSERT_FALSE(moveOnlyKeys.add(MoveOnly(i + 1), i + 1).isNewEntry);

    for (size_t i = 0; i < 100; ++i)
        ASSERT_TRUE(moveOnlyKeys.remove(MoveOnly(i + 1)));

    ASSERT_TRUE(moveOnlyKeys.isEmpty());
}

TEST(WTF_RobinHoodHashMap, InitializerList)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, std::string, RobinHoodHash<unsigned>> map = {
        { 1, "one" },
        { 2, "two" },
        { 3, "three" },
        { 4, "four" },
    };

    EXPECT_EQ(4u, map.size());

    EXPECT_EQ("one", map.get(1));
    EXPECT_EQ("two", map.get(2));
    EXPECT_EQ("three", map.get(3));
    EXPECT_EQ("four", map.get(4));
    EXPECT_EQ(std::string(), map.get(5));
}

TEST(WTF_RobinHoodHashMap, EfficientGetter)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, CopyMoveCounter, RobinHoodHash<unsigned>> map;
    map.set(1, CopyMoveCounter());

    {
        CopyMoveCounter::TestingScope scope;
        map.get(1);
        EXPECT_EQ(0U, CopyMoveCounter::constructionCount);
        EXPECT_EQ(1U, CopyMoveCounter::copyCount);
        EXPECT_EQ(0U, CopyMoveCounter::moveCount);
    }

    {
        CopyMoveCounter::TestingScope scope;
        map.get(2);
        EXPECT_EQ(1U, CopyMoveCounter::constructionCount);
        EXPECT_EQ(0U, CopyMoveCounter::copyCount);
        EXPECT_EQ(1U, CopyMoveCounter::moveCount);
    }
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<ConstructorDestructorCounter>, int, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> map;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    map.add(WTFMove(uniquePtr), 2);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    map.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_CustomDeleter)
{
    ConstructorDestructorCounter::TestingScope constructorDestructorCounterScope;
    DeleterCounter<ConstructorDestructorCounter>::TestingScope deleterCounterScope;

    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<ConstructorDestructorCounter, DeleterCounter<ConstructorDestructorCounter>>, int, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter, DeleterCounter<ConstructorDestructorCounter>>>> map;

    std::unique_ptr<ConstructorDestructorCounter, DeleterCounter<ConstructorDestructorCounter>> uniquePtr(new ConstructorDestructorCounter(), DeleterCounter<ConstructorDestructorCounter>());
    map.add(WTFMove(uniquePtr), 2);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    EXPECT_EQ(0u, DeleterCounter<ConstructorDestructorCounter>::deleterCount());

    map.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);

    EXPECT_EQ(1u, DeleterCounter<ConstructorDestructorCounter>::deleterCount());
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_FindUsingRawPointer)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<int>, int, RobinHoodHash<std::unique_ptr<int>>> map;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    map.add(WTFMove(uniquePtr), 2);

    auto it = map.find(ptr);
    ASSERT_TRUE(it != map.end());
    EXPECT_EQ(ptr, it->key.get());
    EXPECT_EQ(2, it->value);
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_ContainsUsingRawPointer)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<int>, int, RobinHoodHash<std::unique_ptr<int>>> map;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    map.add(WTFMove(uniquePtr), 2);

    EXPECT_EQ(true, map.contains(ptr));
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_GetUsingRawPointer)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<int>, int, RobinHoodHash<std::unique_ptr<int>>> map;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    map.add(WTFMove(uniquePtr), 2);

    int value = map.get(ptr);
    EXPECT_EQ(2, value);
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_RemoveUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<ConstructorDestructorCounter>, int, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> map;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    map.add(WTFMove(uniquePtr), 2);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    bool result = map.remove(ptr);
    EXPECT_EQ(true, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashMap, UniquePtrKey_TakeUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashMap<std::unique_ptr<ConstructorDestructorCounter>, int, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> map;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    map.add(WTFMove(uniquePtr), 2);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    int result = map.take(ptr);
    EXPECT_EQ(2, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashMap, UniqueRefValue)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<int, UniqueRef<int>, RobinHoodHash<int>> map;
    UniqueRef<int> five = makeUniqueRefWithoutFastMallocCheck<int>(5);
    map.add(5, WTFMove(five));
    EXPECT_TRUE(map.contains(5));
    int* shouldBeFive = map.get(5);
    EXPECT_EQ(*shouldBeFive, 5);
    std::unique_ptr<int> takenFive = map.take(5);
    EXPECT_EQ(*takenFive, 5);
    map.ensure(6, [] {
        return makeUniqueRefWithoutFastMallocCheck<int>(6);
    });
    EXPECT_FALSE(map.contains(5));
    EXPECT_TRUE(map.contains(6));
    for (UniqueRef<int>& a : map.values())
        EXPECT_EQ(a.get(), 6);
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_Add)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.add(ptr, 0);
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddUsingRelease)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.add(WTFMove(ptr), 0);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddUsingMove)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.add(WTFMove(ptr), 0);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddUsingRaw)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.add(ptr.get(), 0);
    }

    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddKeyAlreadyPresent)
{
    DerivedRefLogger a("a");

    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

    {
        RefPtr<RefLogger> ptr(&a);
        map.add(ptr, 0);
    }

    EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr2(&a);
        auto addResult = map.add(ptr2, 0);
        EXPECT_FALSE(addResult.isNewEntry);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    map.clear();

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddUsingReleaseKeyAlreadyPresent)
{
    DerivedRefLogger a("a");

    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

    {
        RefPtr<RefLogger> ptr(&a);
        map.add(ptr, 0);
    }

    EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr2(&a);
        auto addResult = map.add(WTFMove(ptr2), 0);
        EXPECT_FALSE(addResult.isNewEntry);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    map.clear();

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_AddUsingMoveKeyAlreadyPresent)
{
    DerivedRefLogger a("a");

    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

    {
        RefPtr<RefLogger> ptr(&a);
        map.add(ptr, 0);
    }

    EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr2(&a);
        auto addResult = map.add(WTFMove(ptr2), 0);
        EXPECT_FALSE(addResult.isNewEntry);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    map.clear();

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_Set)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(ptr, 0);
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_SetUsingRelease)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(WTFMove(ptr), 0);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}


TEST(WTF_RobinHoodHashMap, RefPtrKey_SetUsingMove)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(WTFMove(ptr), 0);
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_SetUsingRaw)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(ptr.get(), 0);
    }

    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_SetKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(ptr, 0);

        EXPECT_STREQ("ref(a) ref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = map.set(ptr2, 1);
            EXPECT_FALSE(addResult.isNewEntry);
            EXPECT_EQ(1, map.get(ptr.get()));
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_SetUsingReleaseKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(ptr, 0);

        EXPECT_STREQ("ref(a) ref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = map.set(WTFMove(ptr2), 1);
            EXPECT_FALSE(addResult.isNewEntry);
            EXPECT_EQ(1, map.get(ptr.get()));
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, RefPtrKey_SetUsingMoveKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<RefLogger>, int, RobinHoodHash<RefPtr<RefLogger>>> map;

        RefPtr<RefLogger> ptr(&a);
        map.set(ptr, 0);

        EXPECT_STREQ("ref(a) ref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = map.set(WTFMove(ptr2), 1);
            EXPECT_FALSE(addResult.isNewEntry);
            EXPECT_EQ(1, map.get(ptr.get()));
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, Ensure)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, unsigned, RobinHoodHash<unsigned>> map;
    {
        auto addResult = map.ensure(1, [] { return 1; });
        EXPECT_EQ(1u, addResult.iterator->value);
        EXPECT_EQ(1u, addResult.iterator->key);
        EXPECT_TRUE(addResult.isNewEntry);
        auto addResult2 = map.ensure(1, [] { return 2; });
        EXPECT_EQ(1u, addResult2.iterator->value);
        EXPECT_EQ(1u, addResult2.iterator->key);
        EXPECT_FALSE(addResult2.isNewEntry);
    }
}

TEST(WTF_RobinHoodHashMap, Ensure_MoveOnlyValues)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, MoveOnly, RobinHoodHash<unsigned>> moveOnlyValues;
    {
        auto addResult = moveOnlyValues.ensure(1, [] { return MoveOnly(1); });
        EXPECT_EQ(1u, addResult.iterator->value.value());
        EXPECT_EQ(1u, addResult.iterator->key);
        EXPECT_TRUE(addResult.isNewEntry);
        auto addResult2 = moveOnlyValues.ensure(1, [] { return MoveOnly(2); });
        EXPECT_EQ(1u, addResult2.iterator->value.value());
        EXPECT_EQ(1u, addResult2.iterator->key);
        EXPECT_FALSE(addResult2.isNewEntry);
    }
}

TEST(WTF_RobinHoodHashMap, Ensure_UniquePointer)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, std::unique_ptr<unsigned>, RobinHoodHash<unsigned>> map;
    {
        auto addResult = map.ensure(1, [] { return makeUniqueWithoutFastMallocCheck<unsigned>(1); });
        EXPECT_EQ(1u, *map.get(1));
        EXPECT_EQ(1u, *addResult.iterator->value.get());
        EXPECT_EQ(1u, addResult.iterator->key);
        EXPECT_TRUE(addResult.isNewEntry);
        auto addResult2 = map.ensure(1, [] { return makeUniqueWithoutFastMallocCheck<unsigned>(2); });
        EXPECT_EQ(1u, *map.get(1));
        EXPECT_EQ(1u, *addResult2.iterator->value.get());
        EXPECT_EQ(1u, addResult2.iterator->key);
        EXPECT_FALSE(addResult2.isNewEntry);
    }
}

TEST(WTF_RobinHoodHashMap, Ensure_RefPtr)
{
    DerivedRefLogger a("a");

    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, RefPtr<RefLogger>, RobinHoodHash<unsigned>> map;

    map.ensure(1, [&] { return RefPtr<RefLogger>(&a); });
    EXPECT_STREQ("ref(a) ", takeLogStr().c_str());

    map.ensure(1, [&] { return RefPtr<RefLogger>(&a); });
    EXPECT_STREQ("", takeLogStr().c_str());

    map.clear();

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

class ObjectWithRefLogger {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ObjectWithRefLogger(Ref<RefLogger>&& logger)
        : m_logger(WTFMove(logger))
    {
    }

    Ref<RefLogger> m_logger;
};


static void testMovingUsingEnsure(Ref<RefLogger>&& logger)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, std::unique_ptr<ObjectWithRefLogger>, RobinHoodHash<unsigned>> map;

    map.ensure(1, [&] { return makeUnique<ObjectWithRefLogger>(WTFMove(logger)); });
}

static void testMovingUsingAdd(Ref<RefLogger>&& logger)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, std::unique_ptr<ObjectWithRefLogger>, RobinHoodHash<unsigned>> map;

    auto& slot = map.add(1, nullptr).iterator->value;
    slot = makeUnique<ObjectWithRefLogger>(WTFMove(logger));
}

TEST(WTF_RobinHoodHashMap, Ensure_LambdasCapturingByReference)
{
    {
        DerivedRefLogger a("a");
        Ref<RefLogger> ref(a);
        testMovingUsingEnsure(WTFMove(ref));

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    {
        DerivedRefLogger a("a");
        Ref<RefLogger> ref(a);
        testMovingUsingAdd(WTFMove(ref));

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }
}


TEST(WTF_RobinHoodHashMap, ValueIsDestructedOnRemove)
{
    struct DestructorObserver {
        DestructorObserver() = default;

        DestructorObserver(bool* destructed)
            : destructed(destructed)
        {
        }

        ~DestructorObserver()
        {
            if (destructed)
                *destructed = true;
        }

        DestructorObserver(DestructorObserver&& other)
            : destructed(other.destructed)
        {
            other.destructed = nullptr;
        }

        DestructorObserver& operator=(DestructorObserver&& other)
        {
            destructed = other.destructed;
            other.destructed = nullptr;
            return *this;
        }

        bool* destructed { nullptr };
    };

    MemoryCompactLookupOnlyRobinHoodHashMap<int, DestructorObserver, RobinHoodHash<int>> map;

    bool destructed = false;
    map.add(5, DestructorObserver { &destructed });

    EXPECT_FALSE(destructed);

    bool removeResult = map.remove(5);

    EXPECT_TRUE(removeResult);
    EXPECT_TRUE(destructed);
}

struct DerefObserver {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    NEVER_INLINE void ref()
    {
        ++count;
    }
    NEVER_INLINE void deref()
    {
        --count;
        observedBucket = bucketAddress->get();
    }
    unsigned count { 1 };
    const RefPtr<DerefObserver>* bucketAddress { nullptr };
    const DerefObserver* observedBucket { nullptr };
};

TEST(WTF_RobinHoodHashMap, RefPtrNotZeroedBeforeDeref)
{
    auto observer = makeUniqueWithoutRefCountedCheck<DerefObserver>();

    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<DerefObserver>, int, RobinHoodHash<RefPtr<DerefObserver>>> map;
    map.add(adoptRef(observer.get()), 5);

    auto iterator = map.find(observer.get());
    EXPECT_TRUE(iterator != map.end());

    observer->bucketAddress = &iterator->key;

    EXPECT_TRUE(observer->observedBucket == nullptr);
    EXPECT_TRUE(map.remove(observer.get()));

    // It if fine to either leave the old value intact at deletion or already set it to the deleted
    // value.
    // A zero would be a incorrect outcome as it would mean we nulled the bucket before an opaque
    // call.
    EXPECT_TRUE(observer->observedBucket == observer.get() || observer->observedBucket == RefPtr<DerefObserver>::PtrTraits::hashTableDeletedValue());
    EXPECT_EQ(observer->count, 0u);
}

TEST(WTF_RobinHoodHashMap, Ref_Key)
{
    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.add(WTFMove(ref), 1);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.set(WTFMove(ref), 1);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> refA(a);
        map.add(WTFMove(refA), 1);

        Ref<RefLogger> refA2(a);
        map.set(WTFMove(refA2), 1);
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.ensure(WTFMove(ref), []() {
            return 1;
        });
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.add(WTFMove(ref), 1);

        auto it = map.find(&a);
        ASSERT_TRUE(it != map.end());

        ASSERT_EQ(it->key.ptr(), &a);
        ASSERT_EQ(it->value, 1);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.add(WTFMove(ref), 1);

        map.remove(&a);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;

        Ref<RefLogger> ref(a);
        map.add(WTFMove(ref), 1);

        int i = map.take(&a);
        ASSERT_EQ(i, 1);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashMap<Ref<RefLogger>, int, RobinHoodHash<Ref<RefLogger>>> map;
        for (int i = 0; i < 64; ++i) {
            // FIXME: All of these RefLogger objects leak. No big deal for a test I guess.
            Ref<RefLogger> ref = adoptRef(*new RefLogger("a"));
            auto* pointer = ref.ptr();
            map.add(WTFMove(ref), i + 1);
            ASSERT_TRUE(map.contains(pointer));
        }
    }

    ASSERT_STREQ("deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, Ref_Value)
{
    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> ref(a);
        map.add(1, WTFMove(ref));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> ref(a);
        map.set(1, WTFMove(ref));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");
        RefLogger b("b");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> refA(a);
        map.add(1, WTFMove(refA));

        Ref<RefLogger> refB(b);
        map.set(1, WTFMove(refB));
    }

    ASSERT_STREQ("ref(a) ref(b) deref(a) deref(b) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> ref(a);
        map.add(1, WTFMove(ref));

        auto aGet = map.get(1);
        ASSERT_EQ(aGet, &a);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        auto emptyGet = map.get(1);
        ASSERT_TRUE(emptyGet == nullptr);
    }

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> ref(a);
        map.add(1, WTFMove(ref));

        auto aOut = map.take(1);
        ASSERT_TRUE(static_cast<bool>(aOut));
        ASSERT_EQ(&a, aOut.get());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        auto emptyTake = map.take(1);
        ASSERT_FALSE(static_cast<bool>(emptyTake));
    }

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        Ref<RefLogger> ref(a);
        map.add(1, WTFMove(ref));
        map.remove(1);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;

        map.ensure(1, [&] {
            Ref<RefLogger> ref(a);
            return ref;
        });
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashMap<int, Ref<RefLogger>, RobinHoodHash<int>> map;
        for (int i = 0; i < 64; ++i) {
            // FIXME: All of these RefLogger objects leak. No big deal for a test I guess.
            Ref<RefLogger> ref = adoptRef(*new RefLogger("a"));
            map.add(i + 1, WTFMove(ref));
            ASSERT_TRUE(map.contains(i + 1));
        }
    }

    ASSERT_STREQ("deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashMap, DeletedAddressOfOperator)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<int, DeletedAddressOfOperator, RobinHoodHash<int>> map1;
    for (auto& value : map1.values())
        (void)value;
}

TEST(WTF_RobinHoodHashMap, RefMappedToNonZeroEmptyValue)
{
    class Value {
    public:
        Value() = default;
        Value(Value&&) = default;
        Value(const Value&) = default;
        Value& operator=(Value&&) = default;

        Value(int32_t f)
            : m_field(f)
        { }

        int32_t field() { return m_field; }

    private:
        int32_t m_field { 0xbadbeef };
    };

    class Key : public RefCounted<Key> {
        Key() = default;
    public:
        static Ref<Key> create() { return adoptRef(*new Key); }
    };

    static_assert(!WTF::HashTraits<Value>::emptyValueIsZero);

    MemoryCompactLookupOnlyRobinHoodHashMap<Ref<Key>, Value, RobinHoodHash<Ref<Key>>> map;
    Vector<std::pair<Ref<Key>, int32_t>> vectorMap;

    for (int32_t i = 0; i < 160; ++i) {
        Ref<Key> key = Key::create();
        map.add(Ref<Key>(key.get()), Value { i });
        vectorMap.append({ WTFMove(key), i });
    }

    for (auto& pair : vectorMap)
        ASSERT_EQ(pair.second, map.get(pair.first).field());

    for (auto& pair : vectorMap)
        ASSERT_TRUE(map.remove(pair.first));
}

TEST(WTF_RobinHoodHashMap, Random_Empty)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, unsigned, RobinHoodHash<unsigned>> map;

    auto result = map.random();
    ASSERT_EQ(result, map.end());
}

TEST(WTF_RobinHoodHashMap, Random_WrapAround)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, unsigned, ZeroHash<unsigned>, BigTableHashTraits<unsigned>> map;
    map.add(1, 1);

    auto result = map.random();
    ASSERT_EQ(result, map.begin());
}

TEST(WTF_RobinHoodHashMap, Random_IsEvenlyDistributed)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, unsigned, RobinHoodHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> map;
    map.add(0, 0);
    map.add(1, 1);

    unsigned zeros = 0;
    unsigned ones = 0;

    for (unsigned i = 0; i < 1000; ++i) {
        auto it = map.random();
        if (!it->value)
            ++zeros;
        else {
            ASSERT_EQ(it->value, 1u);
            ++ones;
        }
    }

    ASSERT_EQ(zeros + ones, 1000u);
    ASSERT_LE(zeros, 600u);
    ASSERT_LE(ones, 600u);
}

TEST(WTF_RobinHoodHashMap, ReserveInitialCapacity)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<String, String> map;
    EXPECT_EQ(0u, map.size());
    EXPECT_EQ(0u, map.capacity());

    map.reserveInitialCapacity(9999);
    EXPECT_EQ(0u, map.size());
    EXPECT_EQ(16384u, map.capacity());

    for (int i = 0; i < 9999; ++i)
        map.add(makeString("foo"_s, i), makeString("bar"_s, i));
    EXPECT_EQ(9999u, map.size());
    EXPECT_EQ(16384u, map.capacity());
    EXPECT_TRUE(map.contains("foo3"_str));
    EXPECT_STREQ("bar3", map.get("foo3"_str).utf8().data());

    for (int i = 0; i < 9999; ++i)
        map.add(makeString("excess"_s, i), makeString("baz"_s, i));
    EXPECT_EQ(9999u + 9999u, map.size());
    EXPECT_EQ(32768u, map.capacity());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(map.remove(makeString("foo"_s, i)));
    EXPECT_EQ(9999u, map.size());
    EXPECT_EQ(32768u, map.capacity());
    EXPECT_STREQ("baz3", map.get("excess3"_str).utf8().data());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(map.remove(makeString("excess"_s, i)));
    EXPECT_EQ(0u, map.size());
    EXPECT_EQ(8u, map.capacity());

    MemoryCompactLookupOnlyRobinHoodHashMap<String, String> map2;
    map2.reserveInitialCapacity(9999);
    EXPECT_FALSE(map2.remove("foo1"_s));

    for (int i = 0; i < 2000; ++i)
        map2.add(makeString("foo"_s, i), makeString("bar"_s, i));
    EXPECT_EQ(2000u, map2.size());
    EXPECT_EQ(16384u, map2.capacity());

    for (int i = 0; i < 2000; ++i)
        EXPECT_TRUE(map2.remove(makeString("foo"_s, i)));
    EXPECT_EQ(0u, map2.size());
    EXPECT_EQ(8u, map2.capacity());
}

TEST(WTF_RobinHoodHashMap, Random_IsEvenlyDistributedAfterRemove)
{
    for (size_t tableSize = 2; tableSize <= 2 * 6; ++tableSize) { // Our hash tables shrink at a load factor of 1 / 6.
        MemoryCompactLookupOnlyRobinHoodHashMap<unsigned, unsigned, RobinHoodHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> map;
        for (size_t i = 0; i < tableSize; ++i)
            map.add(i, i);
        for (size_t i = 2; i < tableSize; ++i)
            map.remove(i);

        unsigned zeros = 0;
        unsigned ones = 0;

        for (unsigned i = 0; i < 1000; ++i) {
            auto it = map.random();
            if (!it->value)
                ++zeros;
            else {
                ASSERT_EQ(it->value, 1u);
                ++ones;
            }
        }

        ASSERT_EQ(zeros + ones, 1000u);
        ASSERT_LE(zeros, 600u);
        ASSERT_LE(ones, 600u);
    }
}

class TestObjectWithCustomDestructor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TestObjectWithCustomDestructor(Function<void()>&& runInDestructor)
        : m_runInDestructor(WTFMove(runInDestructor))
    { }

    ~TestObjectWithCustomDestructor()
    {
        m_runInDestructor();
    }

private:
    Function<void()> m_runInDestructor;
};

TEST(WTF_RobinHoodHashMap, Clear_Reenter)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<uint64_t, std::unique_ptr<TestObjectWithCustomDestructor>, RobinHoodHash<uint64_t>> map;
    for (unsigned i = 1; i <= 10; ++i) {
        map.add(i, makeUnique<TestObjectWithCustomDestructor>([&map] {
            map.clear();
            EXPECT_EQ(0U, map.size());
            EXPECT_TRUE(map.isEmpty());
        }));
    }
    EXPECT_EQ(10U, map.size());
    map.clear();
    EXPECT_EQ(0U, map.size());
    EXPECT_TRUE(map.isEmpty());
}

TEST(WTF_RobinHoodHashMap, AddAndIdentity)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<StringImpl>, unsigned> map;
    for (unsigned index = 0; index < 1e3; ++index) {
        auto string = String::number(index);
        auto result = map.set(string.impl(), index);
        EXPECT_TRUE(result.isNewEntry);
        EXPECT_EQ(result.iterator->key, string.impl());
    }
    for (unsigned index = 0; index < 1e3; ++index) {
        auto string = String::number(index);
        auto result = map.add(string.impl(), index + 1);
        EXPECT_FALSE(result.isNewEntry);
        EXPECT_EQ(result.iterator->value, index);
        EXPECT_NE(result.iterator->key, string.impl());
    }
}

TEST(WTF_RobinHoodHashMap, LargeRemoval)
{
    MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<StringImpl>, unsigned> map;
    for (unsigned index = 0; index < 1e4; ++index) {
        auto string = String::number(index);
        map.set(string.impl(), index);
    }
    EXPECT_EQ(map.size(), 1e4);
    for (unsigned index = 0; index < 1e4; ++index) {
        if (index & 0x1) {
            auto string = String::number(index);
            bool removed = map.remove(string.impl());
            EXPECT_TRUE(removed);
        }
    }
    EXPECT_EQ(map.size(), 1e4 / 2);
    for (unsigned index = 0; index < 1e4; ++index) {
        auto string = String::number(index);
        if (index & 0x1)
            EXPECT_FALSE(map.contains(string.impl()));
        else {
            unsigned value = map.get(string.impl());
            EXPECT_EQ(value, index);
        }
    }

    for (unsigned index = 0; index < 1e4; ++index) {
        if (!(index & 0x1)) {
            auto string = String::number(index);
            bool removed = map.remove(string.impl());
            EXPECT_TRUE(removed);
        }
    }
    EXPECT_EQ(map.size(), 0u);
    EXPECT_EQ(map.capacity(), 8u);
}

} // namespace TestWebKitAPI
