/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/SmallMap.h>
#include <wtf/SmallSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

template<typename T>
void testSmallSetOfUnsigned(unsigned n)
{
    SmallSet<T, IntHash<T>> set;

    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(set.size(), 0u);

    for (unsigned i = 0; i < n; ++i) {
        auto result = set.add(i);
        EXPECT_TRUE(result.isNewEntry);
    }

    EXPECT_FALSE(set.isEmpty());
    EXPECT_EQ(set.size(), n);

    for (unsigned i = 0; i < 2 * n; i += 2) {
        auto result = set.add(i);
        if (i < n)
            EXPECT_FALSE(result.isNewEntry);
        else
            EXPECT_TRUE(result.isNewEntry);
    }

    EXPECT_EQ(set.size(), n + n / 2);

    for (unsigned i = 0; i < 2 * n; ++i) {
        if (i < n || !(i % 2))
            EXPECT_TRUE(set.contains(i));
        else
            EXPECT_FALSE(set.contains(i));
    }

    unsigned count = 0;
    HashSet<T, IntHash<T>, WTF::UnsignedWithZeroKeyHashTraits<T>> referenceSet;
    for (unsigned i : set) {
        referenceSet.add(i);
        ++count;
    }
    EXPECT_EQ(count, n + n / 2);
    EXPECT_EQ(count, referenceSet.size());
}

void testSmallSetOfPointers()
{
    SmallSet<int*> set;

    constexpr unsigned arraySize = 200;
    int array[arraySize];
    for (unsigned i = 0; i < arraySize; ++i)
        array[i] = i;

    for (unsigned i = 0; i < arraySize; ++i) {
        int* ptr = &(array[i]);
        EXPECT_FALSE(set.contains(ptr));
        auto result = set.add(ptr);
        EXPECT_TRUE(result.isNewEntry);
        EXPECT_TRUE(set.contains(ptr));
        result = set.add(ptr);
        EXPECT_FALSE(result.isNewEntry);
    }

    for (int* ptr : set)
        ++(*ptr);

    for (unsigned i = 0; i < arraySize; ++i)
        EXPECT_EQ(array[i], static_cast<int>(i+1));
}

template<typename T>
void testVectorsOfSmallSetsOfUnsigned()
{
    Vector<SmallSet<T, IntHash<T>>, 2> vector;
    vector.append({ });
    vector.append({ });

    vector[0].add(100);
    vector[0].add(200);
    vector[0].add(300);

    for (unsigned i = 0; i < 200; ++i)
        vector[1].add(i);

    // Now we cause the vector to copy its contents to a larger buffer.
    // This should corrupt neither the small set that uses inline storage (vector[0]), nor the one that uses an out-of-line buffer (vector[1]).
    vector.resize(300);

    EXPECT_EQ(vector[0].size(), 3u);
    EXPECT_TRUE(vector[0].contains(100));
    EXPECT_TRUE(vector[0].contains(200));
    EXPECT_TRUE(vector[0].contains(300));
    EXPECT_FALSE(vector[0].contains(42));

    EXPECT_EQ(vector[1].size(), 200u);
    for (unsigned i = 0; i < 200; ++i)
        EXPECT_TRUE(vector[1].contains(i));
    EXPECT_FALSE(vector[1].contains(300));
}

TEST(WTF_SmallSet, ThreeUint8) { testSmallSetOfUnsigned<uint8_t>(3); }
TEST(WTF_SmallSet, FourUint8) { testSmallSetOfUnsigned<uint8_t>(4); }
TEST(WTF_SmallSet, HundredUint8) { testSmallSetOfUnsigned<uint8_t>(100); }
TEST(WTF_SmallSet, ThreeUint16) { testSmallSetOfUnsigned<uint16_t>(3); }
TEST(WTF_SmallSet, FourUint16) { testSmallSetOfUnsigned<uint16_t>(4); }
TEST(WTF_SmallSet, HundredUint16) { testSmallSetOfUnsigned<uint16_t>(100); }
TEST(WTF_SmallSet, ThreeUint32) { testSmallSetOfUnsigned<uint32_t>(3); }
TEST(WTF_SmallSet, FourUint32) { testSmallSetOfUnsigned<uint32_t>(4); }
TEST(WTF_SmallSet, HundredUint32) { testSmallSetOfUnsigned<uint32_t>(100); }
TEST(WTF_SmallSet, ThreeUint64) { testSmallSetOfUnsigned<uint64_t>(3); }
TEST(WTF_SmallSet, FourUint64) { testSmallSetOfUnsigned<uint64_t>(4); }
TEST(WTF_SmallSet, HundredUint64) { testSmallSetOfUnsigned<uint64_t>(100); }

TEST(WTF_SmallSet, Pointers) { testSmallSetOfPointers(); }

TEST(WTF_SmallSet, VectorUint16) { testVectorsOfSmallSetsOfUnsigned<uint16_t>(); }
TEST(WTF_SmallSet, VectorUint32) { testVectorsOfSmallSetsOfUnsigned<uint32_t>(); }
TEST(WTF_SmallSet, VectorUint64) { testVectorsOfSmallSetsOfUnsigned<uint64_t>(); }

TEST(WTF_SmallMap, Basic)
{
    SmallMap<int, UniqueRef<int>> map;
    size_t iterations { 0 };

    EXPECT_NULL(map.get(5));
    map.forEach([&] (auto&, auto&) {
        iterations++;
    });
    EXPECT_EQ(iterations, 0u);
    EXPECT_EQ(map.size(), 0u);

    map.remove(6);
    map.ensure(5, [] {
        return makeUniqueRefWithoutFastMallocCheck<int>(6);
    });
    map.remove(6);
    EXPECT_EQ(map.get(5)->get(), 6);
    map.remove(5);
    EXPECT_NULL(map.get(5));
    map.ensure(5, [] {
        return makeUniqueRefWithoutFastMallocCheck<int>(6);
    });
    EXPECT_NULL(map.get(6));
    map.forEach([&] (auto& key, auto& value) {
        EXPECT_EQ(key, 5);
        EXPECT_EQ(value.get(), 6);
        iterations++;
    });
    EXPECT_EQ(iterations, 1u);
    EXPECT_EQ(map.size(), 1u);

    map.ensure(7, [] {
        return makeUniqueRefWithoutFastMallocCheck<int>(8);
    });
    EXPECT_EQ(map.get(5)->get(), 6);
    EXPECT_EQ(map.get(7)->get(), 8);
    EXPECT_NULL(map.get(6));
    bool saw5 { false };
    bool saw7 { false };
    map.forEach([&] (auto& key, auto& value) {
        switch (key) {
        case 5:
            EXPECT_EQ(value.get(), 6);
            EXPECT_FALSE(std::exchange(saw5, true));
            return;
        case 7:
            EXPECT_EQ(value.get(), 8);
            EXPECT_FALSE(std::exchange(saw7, true));
            return;
        default:
            ASSERT_NOT_REACHED();
        }
    });
    EXPECT_EQ(map.size(), 2u);

    map.ensure(9, [] {
        return makeUniqueRefWithoutFastMallocCheck<int>(10);
    });
    iterations = 0;
    map.forEach([&] (auto& key, auto& value) {
        iterations++;
    });
    EXPECT_EQ(iterations, 3u);
    EXPECT_EQ(map.size(), 3u);

    map.remove(5);
    map.remove(7);
    map.remove(9);
    iterations = 0;
    map.forEach([&] (auto& key, auto& value) {
        iterations++;
    });
    EXPECT_EQ(iterations, 0u);
    EXPECT_EQ(map.size(), 0u);
}

} // namespace TestWebKitAPI
