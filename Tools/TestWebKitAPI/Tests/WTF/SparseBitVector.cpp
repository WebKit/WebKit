/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <set>
#include <wtf/SparseBitVector.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

template<unsigned elementBits>
static Vector<unsigned> collect(const SparseBitVector<elementBits>& bits)
{
    Vector<unsigned> result;
    bits.forEachSetBit([&] (unsigned index) {
        result.append(index);
    });
    return result;
}

TEST(WTF_SparseBitVector, Empty)
{
    SparseBitVector<> bits;
    EXPECT_TRUE(bits.isEmpty());
    EXPECT_FALSE(bits.contains(0));
    EXPECT_FALSE(bits.contains(42));
    EXPECT_FALSE(bits.contains(1000000));
    EXPECT_TRUE(collect(bits).isEmpty());
}

TEST(WTF_SparseBitVector, SetAndContains)
{
    SparseBitVector<> bits;

    // set() returns true the first time and false afterwards.
    EXPECT_TRUE(bits.set(42));
    EXPECT_FALSE(bits.set(42));
    EXPECT_FALSE(bits.isEmpty());
    EXPECT_TRUE(bits.contains(42));
    EXPECT_FALSE(bits.contains(41));
    EXPECT_FALSE(bits.contains(43));

    EXPECT_TRUE(bits.set(0));
    EXPECT_TRUE(bits.set(1000000));
    EXPECT_TRUE(bits.contains(0));
    EXPECT_TRUE(bits.contains(1000000));
    EXPECT_FALSE(bits.contains(999999));
}

TEST(WTF_SparseBitVector, ForEachSetBitIsAscending)
{
    SparseBitVector<> bits;
    // Insert out of order and across many elements.
    for (unsigned value : { 5000u, 3u, 4999u, 200u, 0u, 127u, 128u, 129u, 1u, 1000003u })
        bits.set(value);

    Vector<unsigned> expected { 0, 1, 3, 127, 128, 129, 200, 4999, 5000, 1000003 };
    EXPECT_EQ(collect(bits), expected);
}

TEST(WTF_SparseBitVector, Clear)
{
    SparseBitVector<> bits;
    bits.set(1);
    bits.set(100000);
    EXPECT_FALSE(bits.isEmpty());

    bits.clear();
    EXPECT_TRUE(bits.isEmpty());
    EXPECT_FALSE(bits.contains(1));
    EXPECT_FALSE(bits.contains(100000));
    EXPECT_TRUE(collect(bits).isEmpty());

    // Reusable after clear.
    EXPECT_TRUE(bits.set(7));
    EXPECT_TRUE(bits.contains(7));
}

TEST(WTF_SparseBitVector, ElementBoundaries)
{
    // With 128-bit elements, exercise the exact boundaries between words and elements.
    SparseBitVector<128> bits;
    for (unsigned value : { 0u, 63u, 64u, 127u, 128u, 191u, 192u, 255u })
        EXPECT_TRUE(bits.set(value));
    Vector<unsigned> expected { 0, 63, 64, 127, 128, 191, 192, 255 };
    EXPECT_EQ(collect(bits), expected);
    for (unsigned value : expected)
        EXPECT_TRUE(bits.contains(value));
    EXPECT_FALSE(bits.contains(62));
    EXPECT_FALSE(bits.contains(193));
}

TEST(WTF_SparseBitVector, RandomizedAgainstReference)
{
    // Cross-check against std::set with a deterministic pseudo-random sequence.
    SparseBitVector<> bits;
    std::set<unsigned> reference;

    uint64_t state = 0x123456789abcdef0ull;
    auto next = [&] {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<unsigned>(state % 20000);
    };

    for (unsigned i = 0; i < 20000; ++i) {
        unsigned value = next();
        bool expectedNewlySet = !reference.contains(value);
        EXPECT_EQ(bits.set(value), expectedNewlySet);
        reference.insert(value);
        EXPECT_TRUE(bits.contains(value));
    }

    EXPECT_EQ(bits.isEmpty(), reference.empty());

    // Membership matches for the whole index range.
    for (unsigned value = 0; value < 20000; ++value)
        EXPECT_EQ(bits.contains(value), reference.contains(value));

    // forEachSetBit yields exactly the reference set, in ascending order.
    Vector<unsigned> collected = collect(bits);
    Vector<unsigned> expected;
    for (unsigned value : reference)
        expected.append(value);
    EXPECT_EQ(collected, expected);
}

TEST(WTF_SparseBitVector, SetAndReset)
{
    SparseBitVector<> bits;

    // reset() returns true iff the bit was set.
    EXPECT_FALSE(bits.reset(42));
    EXPECT_TRUE(bits.set(42));
    EXPECT_TRUE(bits.reset(42));
    EXPECT_FALSE(bits.reset(42));
    EXPECT_FALSE(bits.contains(42));

    // Clearing the last bit of an element makes the vector empty again (the element is erased).
    EXPECT_TRUE(bits.set(1000));
    EXPECT_FALSE(bits.isEmpty());
    EXPECT_TRUE(bits.reset(1000));
    EXPECT_TRUE(bits.isEmpty());

    // Clearing one bit while another in the same element remains stays non-empty.
    bits.set(10);
    bits.set(11);
    EXPECT_TRUE(bits.reset(10));
    EXPECT_FALSE(bits.isEmpty());
    EXPECT_FALSE(bits.contains(10));
    EXPECT_TRUE(bits.contains(11));
    EXPECT_EQ(collect(bits), (Vector<unsigned> { 11 }));
}

TEST(WTF_SparseBitVector, RandomizedAddRemoveAgainstReference)
{
    SparseBitVector<> bits;
    std::set<unsigned> reference;

    uint64_t state = 0x0fedcba987654321ull;
    auto next = [&] {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<unsigned>(state % 5000);
    };

    for (unsigned i = 0; i < 40000; ++i) {
        unsigned value = next();
        if (i & 1) {
            bool expectedNewlySet = !reference.contains(value);
            EXPECT_EQ(bits.set(value), expectedNewlySet);
            reference.insert(value);
        } else {
            bool expectedWasSet = reference.contains(value);
            EXPECT_EQ(bits.reset(value), expectedWasSet);
            reference.erase(value);
        }
        EXPECT_EQ(bits.isEmpty(), reference.empty());
    }

    for (unsigned value = 0; value < 5000; ++value)
        EXPECT_EQ(bits.contains(value), reference.contains(value));

    Vector<unsigned> expected;
    for (unsigned value : reference)
        expected.append(value);
    EXPECT_EQ(collect(bits), expected);
}

template<unsigned elementBits>
static Vector<unsigned> collectViaIterator(const SparseBitVector<elementBits>& bits)
{
    Vector<unsigned> result;
    for (unsigned index : bits)
        result.append(index);
    return result;
}

TEST(WTF_SparseBitVector, IteratorEmpty)
{
    SparseBitVector<> bits;
    EXPECT_TRUE(bits.begin() == bits.end());
    EXPECT_TRUE(collectViaIterator(bits).isEmpty());
}

TEST(WTF_SparseBitVector, IteratorSingleBit)
{
    SparseBitVector<> bits;
    bits.set(42);
    EXPECT_FALSE(bits.begin() == bits.end());

    auto it = bits.begin();
    EXPECT_EQ(*it, 42u);
    ++it;
    EXPECT_TRUE(it == bits.end());
    EXPECT_EQ(collectViaIterator(bits), (Vector<unsigned> { 42 }));
}

TEST(WTF_SparseBitVector, IteratorWithinSingleElement)
{
    // All set bits live in the same element, exercising bit-iterator advancement only.
    SparseBitVector<128> bits;
    for (unsigned value : { 0u, 5u, 63u, 64u, 127u })
        bits.set(value);
    EXPECT_EQ(collectViaIterator(bits), (Vector<unsigned> { 0, 5, 63, 64, 127 }));
}

TEST(WTF_SparseBitVector, IteratorCrossesElementBoundaries)
{
    // Bits spread across multiple elements exercise the element-advance branch in operator++.
    SparseBitVector<128> bits;
    for (unsigned value : { 0u, 127u, 128u, 200u, 255u, 1000u, 100000u })
        bits.set(value);
    EXPECT_EQ(collectViaIterator(bits), (Vector<unsigned> { 0, 127, 128, 200, 255, 1000, 100000 }));
}

TEST(WTF_SparseBitVector, IteratorMatchesForEachSetBit)
{
    // The iterator must yield exactly the same sequence as forEachSetBit.
    SparseBitVector<> bits;
    for (unsigned value : { 5000u, 3u, 4999u, 200u, 0u, 127u, 128u, 129u, 1u, 1000003u })
        bits.set(value);
    EXPECT_EQ(collectViaIterator(bits), collect(bits));
}

TEST(WTF_SparseBitVector, IteratorAfterClear)
{
    SparseBitVector<> bits;
    bits.set(1);
    bits.set(100000);
    bits.clear();
    EXPECT_TRUE(bits.begin() == bits.end());
    EXPECT_TRUE(collectViaIterator(bits).isEmpty());

    bits.set(7);
    EXPECT_EQ(collectViaIterator(bits), (Vector<unsigned> { 7 }));
}

} // namespace TestWebKitAPI

