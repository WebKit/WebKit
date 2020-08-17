/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "MoveOnly.h"
#include "RefLogger.h"
#include <string>
#include <wtf/HashCountedSet.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

typedef WTF::HashCountedSet<int> IntHashCountedSet;

TEST(WTF_HashCountedSet, HashTableIteratorComparison)
{
    IntHashCountedSet hashCountedSet;
    hashCountedSet.add(1);
    ASSERT_TRUE(hashCountedSet.begin() != hashCountedSet.end());
    ASSERT_FALSE(hashCountedSet.begin() == hashCountedSet.end());

    IntHashCountedSet::const_iterator begin = hashCountedSet.begin();
    ASSERT_TRUE(begin == hashCountedSet.begin());
    ASSERT_TRUE(hashCountedSet.begin() == begin);
    ASSERT_TRUE(begin != hashCountedSet.end());
    ASSERT_TRUE(hashCountedSet.end() != begin);
    ASSERT_FALSE(begin != hashCountedSet.begin());
    ASSERT_FALSE(hashCountedSet.begin() != begin);
    ASSERT_FALSE(begin == hashCountedSet.end());
    ASSERT_FALSE(hashCountedSet.end() == begin);
}

struct TestDoubleHashTraits : HashTraits<double> {
    static const int minimumTableSize = 8;
};

typedef HashCountedSet<double, DefaultHash<double>, TestDoubleHashTraits> DoubleHashCountedSet;

static int bucketForKey(double key)
{
    return DefaultHash<double>::hash(key) & (TestDoubleHashTraits::minimumTableSize - 1);
}

TEST(WTF_HashCountedSet, DoubleHashCollisions)
{
    // The "clobber" key here is one that ends up stealing the bucket that the -0 key
    // originally wants to be in. This makes the 0 and -0 keys collide and the test then
    // fails unless the FloatHash::equals() implementation can distinguish them.
    const double clobberKey = 6;
    const double zeroKey = 0;
    const double negativeZeroKey = -zeroKey;

    DoubleHashCountedSet hashCountedSet;

    hashCountedSet.add(clobberKey);

    ASSERT_EQ(hashCountedSet.count(clobberKey), 1u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 0u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 0u);

    hashCountedSet.add(zeroKey);
    hashCountedSet.add(negativeZeroKey);

    ASSERT_EQ(bucketForKey(clobberKey), bucketForKey(negativeZeroKey));
    ASSERT_EQ(hashCountedSet.count(clobberKey), 1u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 1u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 1u);

    hashCountedSet.add(clobberKey);
    hashCountedSet.add(zeroKey);
    hashCountedSet.add(negativeZeroKey);

    ASSERT_EQ(hashCountedSet.count(clobberKey), 2u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 2u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 2u);

    hashCountedSet.add(clobberKey, 12);
    hashCountedSet.add(zeroKey, 15);
    hashCountedSet.add(negativeZeroKey, 17);

    ASSERT_EQ(hashCountedSet.count(clobberKey), 14u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 17u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 19u);
}

TEST(WTF_HashCountedSet, DoubleHashCollisionsInitialCount)
{
    // The "clobber" key here is one that ends up stealing the bucket that the -0 key
    // originally wants to be in. This makes the 0 and -0 keys collide and the test then
    // fails unless the FloatHash::equals() implementation can distinguish them.
    const double clobberKey = 6;
    const double zeroKey = 0;
    const double negativeZeroKey = -zeroKey;

    DoubleHashCountedSet hashCountedSet;

    hashCountedSet.add(clobberKey, 5);

    ASSERT_EQ(hashCountedSet.count(clobberKey), 5u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 0u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 0u);

    hashCountedSet.add(zeroKey, 22);
    hashCountedSet.add(negativeZeroKey, 0);

    ASSERT_EQ(bucketForKey(clobberKey), bucketForKey(negativeZeroKey));
    ASSERT_EQ(hashCountedSet.count(clobberKey), 5u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 22u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 0u);

    hashCountedSet.add(clobberKey);
    hashCountedSet.add(zeroKey);
    hashCountedSet.add(negativeZeroKey);

    ASSERT_EQ(hashCountedSet.count(clobberKey), 6u);
    ASSERT_EQ(hashCountedSet.count(zeroKey), 23u);
    ASSERT_EQ(hashCountedSet.count(negativeZeroKey), 1u);
}


TEST(WTF_HashCountedSet, MoveOnlyKeys)
{
    HashCountedSet<MoveOnly> moveOnlyKeys;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyKeys.add(WTFMove(moveOnly));
    }

    for (size_t i = 0; i < 100; ++i) {
        auto it = moveOnlyKeys.find(MoveOnly(i + 1));
        ASSERT_FALSE(it == moveOnlyKeys.end());
        ASSERT_EQ(it->value, 1u);
    }

    for (size_t i = 0; i < 100; ++i)
        ASSERT_FALSE(moveOnlyKeys.add(MoveOnly(i + 1)).isNewEntry);

    for (size_t i = 0; i < 100; ++i)
        ASSERT_FALSE(moveOnlyKeys.remove(MoveOnly(i + 1)));

    for (size_t i = 0; i < 100; ++i)
        ASSERT_TRUE(moveOnlyKeys.remove(MoveOnly(i + 1)));

    ASSERT_TRUE(moveOnlyKeys.isEmpty());
}

TEST(WTF_HashCountedSet, MoveOnlyKeysInitialCount)
{
    HashCountedSet<MoveOnly> moveOnlyKeys;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyKeys.add(WTFMove(moveOnly), i + 1);
    }

    for (size_t i = 0; i < 100; ++i) {
        auto it = moveOnlyKeys.find(MoveOnly(i + 1));
        ASSERT_FALSE(it == moveOnlyKeys.end());
        ASSERT_EQ(it->value, i + 1);
    }

    for (size_t i = 0; i < 100; ++i)
        ASSERT_FALSE(moveOnlyKeys.add(MoveOnly(i + 1)).isNewEntry);

    for (size_t i = 0; i < 100; ++i)
        ASSERT_EQ(moveOnlyKeys.count(MoveOnly(i + 1)), i + 2);

    for (size_t i = 0; i < 100; ++i)
        ASSERT_FALSE(moveOnlyKeys.remove(MoveOnly(i + 1)));

    for (size_t i = 0; i < 100; ++i)
        ASSERT_EQ(moveOnlyKeys.count(MoveOnly(i + 1)), i + 1);
}

TEST(WTF_HashCountedSet, InitializerList)
{
    HashCountedSet<String> hashCountedSet = {
        "one",
        "two",
        "three",
        "four",
        "four",
        "four",
        "four",
    };

    EXPECT_EQ(4u, hashCountedSet.size());

    EXPECT_EQ(hashCountedSet.count("one"), 1u);
    EXPECT_EQ(hashCountedSet.count("two"), 1u);
    EXPECT_EQ(hashCountedSet.count("three"), 1u);
    EXPECT_EQ(hashCountedSet.count("four"), 4u);
}

TEST(WTF_HashCountedSet, InitializerListInitialCount)
{
    HashCountedSet<String> hashCountedSet = {
        { String("one"), 1u },
        { String("two"), 2u },
        { String("three"), 3u },
        { String("four"), 4u },
    };

    EXPECT_EQ(4u, hashCountedSet.size());

    EXPECT_EQ(hashCountedSet.count("one"), 1u);
    EXPECT_EQ(hashCountedSet.count("two"), 2u);
    EXPECT_EQ(hashCountedSet.count("three"), 3u);
    EXPECT_EQ(hashCountedSet.count("four"), 4u);
}

TEST(WTF_HashCountedSet, UniquePtrKey)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashCountedSet<std::unique_ptr<ConstructorDestructorCounter>> hashCountedSet;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    hashCountedSet.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    hashCountedSet.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_HashCountedSet, UniquePtrKeyInitialCount)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashCountedSet<std::unique_ptr<ConstructorDestructorCounter>> hashCountedSet;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    hashCountedSet.add(WTFMove(uniquePtr), 12);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    hashCountedSet.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_HashCountedSet, UniquePtrKey_CustomDeleter)
{
    ConstructorDestructorCounter::TestingScope constructorDestructorCounterScope;
    DeleterCounter<ConstructorDestructorCounter>::TestingScope deleterCounterScope;

    HashCountedSet<std::unique_ptr<ConstructorDestructorCounter, DeleterCounter<ConstructorDestructorCounter>>> hashCountedSet;

    std::unique_ptr<ConstructorDestructorCounter, DeleterCounter<ConstructorDestructorCounter>> uniquePtr(new ConstructorDestructorCounter(), DeleterCounter<ConstructorDestructorCounter>());
    hashCountedSet.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    EXPECT_EQ(0u, DeleterCounter<ConstructorDestructorCounter>::deleterCount());

    hashCountedSet.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);

    EXPECT_EQ(1u, DeleterCounter<ConstructorDestructorCounter>::deleterCount());
}

TEST(WTF_HashCountedSet, UniquePtrKey_FindUsingRawPointer)
{
    HashCountedSet<std::unique_ptr<int>> hashCountedSet;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    hashCountedSet.add(WTFMove(uniquePtr));

    auto it = hashCountedSet.find(ptr);
    ASSERT_TRUE(it != hashCountedSet.end());
    EXPECT_EQ(ptr, it->key.get());
    EXPECT_EQ(1u, it->value);
}

TEST(WTF_HashCountedSet, UniquePtrKey_ContainsUsingRawPointer)
{
    HashCountedSet<std::unique_ptr<int>> hashCountedSet;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    hashCountedSet.add(WTFMove(uniquePtr));

    EXPECT_EQ(true, hashCountedSet.contains(ptr));
}

TEST(WTF_HashCountedSet, UniquePtrKey_GetUsingRawPointer)
{
    HashCountedSet<std::unique_ptr<int>> hashCountedSet;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    hashCountedSet.add(WTFMove(uniquePtr));

    int value = hashCountedSet.count(ptr);
    EXPECT_EQ(1, value);
}

TEST(WTF_HashCountedSet, UniquePtrKey_RemoveUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashCountedSet<std::unique_ptr<ConstructorDestructorCounter>> hashCountedSet;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    hashCountedSet.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    bool result = hashCountedSet.remove(ptr);
    EXPECT_EQ(true, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_HashCountedSet, RefPtrKey_Add)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        RefPtr<RefLogger> ptr(&a);
        hashCountedSet.add(ptr);

        ASSERT_STREQ("ref(a) ref(a) ", takeLogStr().c_str());
        EXPECT_EQ(1U, hashCountedSet.count(ptr));
        EXPECT_EQ(1U, hashCountedSet.count(ptr.get()));
    }

    EXPECT_STREQ("deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddUsingRelease)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        RefPtr<RefLogger> ptr(&a);
        hashCountedSet.add(WTFMove(ptr));
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddUsingMove)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        RefPtr<RefLogger> ptr(&a);
        hashCountedSet.add(WTFMove(ptr));
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddUsingRaw)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        RefPtr<RefLogger> ptr(&a);
        hashCountedSet.add(ptr.get());

        EXPECT_STREQ("ref(a) ref(a) ", takeLogStr().c_str());
        EXPECT_EQ(1U, hashCountedSet.count(ptr));
        EXPECT_EQ(1U, hashCountedSet.count(ptr.get()));
    }

    EXPECT_STREQ("deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        {
            RefPtr<RefLogger> ptr(&a);
            hashCountedSet.add(ptr);
        }

        EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = hashCountedSet.add(ptr2);
            EXPECT_FALSE(addResult.isNewEntry);
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddUsingReleaseKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        {
            RefPtr<RefLogger> ptr(&a);
            hashCountedSet.add(ptr);
        }

        EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = hashCountedSet.add(WTFMove(ptr2));
            EXPECT_FALSE(addResult.isNewEntry);
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, RefPtrKey_AddUsingMoveKeyAlreadyPresent)
{
    {
        DerivedRefLogger a("a");

        HashCountedSet<RefPtr<RefLogger>> hashCountedSet;

        {
            RefPtr<RefLogger> ptr(&a);
            hashCountedSet.add(ptr);
        }

        EXPECT_STREQ("ref(a) ref(a) deref(a) ", takeLogStr().c_str());

        {
            RefPtr<RefLogger> ptr2(&a);
            auto addResult = hashCountedSet.add(WTFMove(ptr2));
            EXPECT_FALSE(addResult.isNewEntry);
        }

        EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
    }

    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashCountedSet, Values)
{
    HashCountedSet<int> set { 1, 1, 2, 3, 3 };
    
    auto vector = copyToVector(set.values());
    EXPECT_EQ(3U, vector.size());

    std::sort(vector.begin(), vector.end());
    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
}

} // namespace TestWebKitAPI
