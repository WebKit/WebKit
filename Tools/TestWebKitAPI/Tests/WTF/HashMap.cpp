/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "MoveOnly.h"
#include <string>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

typedef WTF::HashMap<int, int> IntHashMap;

TEST(WTF_HashMap, HashTableIteratorComparison)
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

typedef HashMap<double, int64_t, DefaultHash<double>::Hash, TestDoubleHashTraits> DoubleHashMap;

static int bucketForKey(double key)
{
    return DefaultHash<double>::Hash::hash(key) & (TestDoubleHashTraits::minimumTableSize - 1);
}

TEST(WTF_HashMap, DoubleHashCollisions)
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

TEST(WTF_HashMap, MoveOnlyValues)
{
    HashMap<unsigned, MoveOnly> moveOnlyValues;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyValues.set(i + 1, std::move(moveOnly));
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

TEST(WTF_HashMap, MoveOnlyKeys)
{
    HashMap<MoveOnly, unsigned> moveOnlyKeys;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        moveOnlyKeys.set(std::move(moveOnly), i + 1);
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

TEST(WTF_HashMap, InitializerList)
{
    HashMap<unsigned, std::string> map = {
        { 1, "one" },
        { 2, "two" },
        { 3, "three" },
        { 4, "four" },
    };

    EXPECT_EQ(4, map.size());

    EXPECT_EQ("one", map.get(1));
    EXPECT_EQ("two", map.get(2));
    EXPECT_EQ("three", map.get(3));
    EXPECT_EQ("four", map.get(4));
    EXPECT_EQ(std::string(), map.get(5));
}

} // namespace TestWebKitAPI
