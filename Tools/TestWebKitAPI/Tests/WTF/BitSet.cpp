/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include <wtf/BitSet.h>

namespace TestWebKitAPI {

constexpr size_t size = 128;
constexpr size_t smallSize = 9;
constexpr size_t zeroSize = 0;

static constexpr bool expectedBits1[size] = {
    false, false, true,  false,  false, false, false, false,
    false, false, false, false,  false, false, false, true,
    false, true,  false, false,  false, false, true,  false,
    false, false, true,  false,  false, false, true,  true,
    true,  false, false, false,  false, true,  false, false,
    true,  false, false, false,  false, true,  false, true,
    false, false, false, false,  false, true,  true,  false,
    false, true,  false, false,  false, true,  true,  true,
    false, false, true,  false,  true,  false, false, false,
    false, false, true,  false,  true,  false, false, true,
    true,  false, false, false,  true,  false, true,  false,
    false, false, false, false,  true,  false, true,  true,
    false, false, false, false,  true,  true,  false, false,
    false, true,  false, false,  true,  true,  false, true,
    false, true,  false, false,  true,  true,  true,  false,
    false, false, true,  false,  true,  true,  true,  true,
};

static constexpr bool expectedBits2[size] = {
    false, true,  false, true,   false, false, false, false,
    false, false, false, true,   false, false, false, true,
    false, false, false, true,   false, false, true,  false,
    false, false, false, true,   false, false, true,  true,
    true,  false, false, true,   false, true,  false, false,
    false, false, false, true,   false, true,  false, true,
    false, false, false, true,   false, true,  true,  false,
    false, false, true,  true,   false, true,  true,  true,
    false, true,  false, true,   true,  false, false, false,
    false, true,  false, true,   true,  false, false, true,
    false, false, false, true,   true,  false, true,  false,
    false, false, true,  true,   true,  false, true,  true,
    true,  false, false, true,   true,  true,  false, false,
    false, false, true,  true,   true,  true,  false, true,
    true,  false, false, true,   true,  true,  true,  false,
    false, true,  false, true,   true,  true,  true,  true,
};

static constexpr bool expectedSmallBits1[smallSize] = {
    false, true, true,  false,  true,  false, false, true,
    true,
};

static constexpr bool expectedSmallBits2[smallSize] = {
    true,  true, false, false,  false, false, true,  true,
    false,
};

constexpr size_t countBits(const bool boolArray[], size_t size)
{
    size_t result = 0;
    for (size_t i = 0; i < size; ++i) {
        if (boolArray[i])
            result++;
    }
    return result;
}

constexpr size_t expectedNumberOfSetBits1 = countBits(expectedBits1, size);
constexpr size_t expectedNumberOfSetBits2 = countBits(expectedBits2, size);

#define DECLARE_BITMAPS_FOR_TEST() \
    WTF::BitSet<size, WordType> bitSetZeroes; \
    WTF::BitSet<size, WordType> bitSet1; /* Will hold values specified in expectedBits1. */ \
    WTF::BitSet<size, WordType> bitSet2; /* Will hold values specified in expectedBits2. */ \
    WTF::BitSet<size, WordType> bitSet2Clone; /* Same as bitSet2. */ \
    WTF::BitSet<size, WordType> bitSetOnes; \
    WTF::BitSet<smallSize, WordType> smallBitSetZeroes; \
    WTF::BitSet<smallSize, WordType> smallBitSetOnes; \
    WTF::BitSet<smallSize, WordType> smallBitSet1; /* Will hold values specified in expectedSmallBits1. */ \
    WTF::BitSet<smallSize, WordType> smallBitSet2; /* Will hold values specified in expectedSmallBits2. */ \
    UNUSED_VARIABLE(bitSetZeroes); \
    UNUSED_VARIABLE(bitSet1); \
    UNUSED_VARIABLE(bitSet2); \
    UNUSED_VARIABLE(bitSet2Clone); \
    UNUSED_VARIABLE(bitSetOnes); \
    UNUSED_VARIABLE(smallBitSetZeroes); \
    UNUSED_VARIABLE(smallBitSetOnes); \
    UNUSED_VARIABLE(smallBitSet1); \
    UNUSED_VARIABLE(smallBitSet2);

#define DECLARE_AND_INIT_BITMAPS_FOR_TEST() \
    DECLARE_BITMAPS_FOR_TEST() \
    for (size_t i = 0; i < size; ++i) { \
        bitSet1.set(i, expectedBits1[i]); \
        bitSet2.set(i, expectedBits2[i]); \
        bitSet2Clone.set(i, expectedBits2[i]); \
        bitSetOnes.set(i); \
    } \
    for (size_t i = 0; i < smallSize; ++i) { \
        smallBitSet1.set(i, expectedSmallBits1[i]); \
        smallBitSet2.set(i, expectedSmallBits2[i]); \
        smallBitSetOnes.set(i); \
    }

template<typename WordType>
void testBitSetSize()
{
    DECLARE_BITMAPS_FOR_TEST();

    auto verifySize = [=] (auto& bitSet, size_t expectedSize, size_t notExpectedSize) {
        EXPECT_EQ(bitSet.size(), expectedSize);
        EXPECT_NE(bitSet.size(), notExpectedSize);
    };

    verifySize(bitSetZeroes, size, smallSize);
    verifySize(bitSet1, size, smallSize);
    verifySize(bitSet2, size, smallSize);
    verifySize(bitSet2Clone, size, smallSize);
    verifySize(bitSetOnes, size, smallSize);
    verifySize(smallBitSetZeroes, smallSize, size);
    verifySize(smallBitSetOnes, smallSize, size);
}

template<typename WordType>
void testBitSetConstructedEmpty()
{
    DECLARE_BITMAPS_FOR_TEST();

    // Verify that the default constructor initializes all bits to 0.
    auto verifyIsEmpty = [=] (const auto& bitSet) {
        EXPECT_TRUE(bitSet.isEmpty());
        EXPECT_EQ(bitSet.count(), zeroSize);
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_FALSE(bitSet.get(i));
    };

    verifyIsEmpty(bitSetZeroes);
    verifyIsEmpty(bitSet1);
    verifyIsEmpty(bitSet2);
    verifyIsEmpty(bitSet2Clone);
    verifyIsEmpty(bitSetOnes);
    verifyIsEmpty(smallBitSetZeroes);
    verifyIsEmpty(smallBitSetOnes);
}

template<typename WordType>
void testBitSetSetGet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyIsEmpty = [=] (const auto& bitSet) {
        EXPECT_TRUE(bitSet.isEmpty());
        EXPECT_EQ(bitSet.count(), zeroSize);
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_FALSE(bitSet.get(i));
    };

    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(bitSet1.get(i), expectedBits1[i]);

    for (size_t i = 0; i < size; ++i) {
        EXPECT_EQ(bitSet2.get(i), expectedBits2[i]);
        EXPECT_EQ(bitSet2Clone.get(i), expectedBits2[i]);
    }
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(bitSet2.get(i), bitSet2Clone.get(i));

    for (size_t i = 0; i < size; ++i)
        EXPECT_TRUE(bitSetOnes.get(i));

    for (size_t i = 0; i < smallSize; ++i)
        EXPECT_TRUE(smallBitSetOnes.get(i));

    // Verify that there is no out of bounds write from the above set operations.
    verifyIsEmpty(bitSetZeroes);
    verifyIsEmpty(smallBitSetZeroes);
}

template<typename WordType>
void testBitSetTestAndSet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_FALSE(bitSet1.isFull());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.testAndSet(i), expectedBits1[i]);
    ASSERT_TRUE(bitSet1.isFull());

    ASSERT_FALSE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_FALSE(smallBitSetZeroes.testAndSet(i));
    ASSERT_TRUE(smallBitSetZeroes.isFull());
}

template<typename WordType>
void testBitSetTestAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_FALSE(bitSet1.isEmpty());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.testAndClear(i), expectedBits1[i]);
    ASSERT_TRUE(bitSet1.isEmpty());

    ASSERT_FALSE(smallBitSetOnes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_TRUE(smallBitSetOnes.testAndClear(i));
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
}

template<typename WordType>
void testBitSetConcurrentTestAndSet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndSet part of the operation is working.
    ASSERT_FALSE(bitSet1.isFull());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.concurrentTestAndSet(i), expectedBits1[i]);
    ASSERT_TRUE(bitSet1.isFull());

    ASSERT_FALSE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_FALSE(smallBitSetZeroes.concurrentTestAndSet(i));
    ASSERT_TRUE(smallBitSetZeroes.isFull());
}

template<typename WordType>
void testBitSetConcurrentTestAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndClear part of the operation is working.
    ASSERT_FALSE(bitSet1.isEmpty());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.concurrentTestAndClear(i), expectedBits1[i]);
    ASSERT_TRUE(bitSet1.isEmpty());

    ASSERT_FALSE(smallBitSetOnes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_TRUE(smallBitSetOnes.concurrentTestAndClear(i));
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
}

template<typename WordType>
void testBitSetClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitSetOnes.isFull());
    for (size_t i = 0; i < size; ++i) {
        if (!expectedBits1[i])
            bitSetOnes.clear(i);
    }
    ASSERT_EQ(bitSetOnes, bitSet1);
}

template<typename WordType>
void testBitSetClearAll()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyIsEmpty = [=] (const auto& bitSet) {
        EXPECT_TRUE(bitSet.isEmpty());
        EXPECT_EQ(bitSet.count(), zeroSize);
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_FALSE(bitSet.get(i));
    };

    auto verifyIsFull = [=] (const auto& bitSet) {
        EXPECT_TRUE(bitSet.isFull());
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_TRUE(bitSet.get(i));
    };

    EXPECT_FALSE(bitSet1.isEmpty());
    bitSet1.clearAll();
    verifyIsEmpty(bitSet1);
    verifyIsFull(bitSetOnes);
    verifyIsFull(smallBitSetOnes);

    EXPECT_FALSE(bitSet2.isEmpty());
    bitSet2.clearAll();
    verifyIsEmpty(bitSet1);
    verifyIsEmpty(bitSet2);
    verifyIsFull(bitSetOnes);
    verifyIsFull(smallBitSetOnes);

    EXPECT_FALSE(bitSet2Clone.isEmpty());
    bitSet2Clone.clearAll();
    verifyIsEmpty(bitSet1);
    verifyIsEmpty(bitSet2);
    verifyIsEmpty(bitSet2Clone);
    verifyIsFull(bitSetOnes);
    verifyIsFull(smallBitSetOnes);
}

template<typename WordType>
void testBitSetInvert()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyInvert = [=] (auto& bitSet) {
        ASSERT_TRUE(bitSet.isFull());
        ASSERT_FALSE(bitSet.isEmpty());
        bitSet.invert();
        ASSERT_FALSE(bitSet.isFull());
        ASSERT_TRUE(bitSet.isEmpty());
        bitSet.invert();
        ASSERT_TRUE(bitSet.isFull());
        ASSERT_FALSE(bitSet.isEmpty());
    };

    verifyInvert(bitSetOnes);
    verifyInvert(smallBitSetOnes);

    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
    bitSet1.invert();
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), !expectedBits1[i]);
    bitSet1.invert();
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
}

template<typename WordType>
void testBitSetFindRunOfZeros()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    const int64_t bitSet1RunsOfZeros[15] = {
        0, 0, 0, 3, 3,
        3, 3, 3, 3, 3,
        3, 3, 3, -1, -1
    };

    const int64_t bitSet2RunsOfZeros[15] = {
        0, 0, 4, 4, 4,
        4, 4, 4, -1, -1,
        -1, -1, -1, -1, -1,
    };

    WTF::BitSet<smallSize, WordType> smallTemp;
    smallTemp.set(1, true); // bits low to high are: 0100 0000 0

    const int64_t smallTempRunsOfZeros[15] = {
        0, 0, 2, 2, 2,
        2, 2, 2, -1, -1,
        -1, -1, -1, -1, -1
    };

    for (size_t runLength = 0; runLength < 15; ++runLength) {
        ASSERT_EQ(bitSetZeroes.findRunOfZeros(runLength), 0ll);
        ASSERT_EQ(bitSet1.findRunOfZeros(runLength), bitSet1RunsOfZeros[runLength]);
        ASSERT_EQ(bitSet2.findRunOfZeros(runLength), bitSet2RunsOfZeros[runLength]);
        ASSERT_EQ(bitSetOnes.findRunOfZeros(runLength), -1ll);

        if (runLength <= smallSize)
            ASSERT_EQ(smallBitSetZeroes.findRunOfZeros(runLength), 0ll);
        else
            ASSERT_EQ(smallBitSetZeroes.findRunOfZeros(runLength), -1ll);

        ASSERT_EQ(smallBitSetOnes.findRunOfZeros(runLength), -1ll);

        ASSERT_EQ(smallTemp.findRunOfZeros(runLength), smallTempRunsOfZeros[runLength]);
    }
}

template<typename WordType>
void testBitSetCount()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_EQ(bitSetZeroes.count(), zeroSize);
    EXPECT_EQ(bitSet1.count(), expectedNumberOfSetBits1);
    EXPECT_EQ(bitSet2.count(), expectedNumberOfSetBits2);
    EXPECT_EQ(bitSet2Clone.count(), expectedNumberOfSetBits2);
    EXPECT_EQ(bitSetOnes.count(), size);
    EXPECT_EQ(smallBitSetZeroes.count(), zeroSize);
    EXPECT_EQ(smallBitSetOnes.count(), smallSize);
}

template<typename WordType>
void testBitSetCountAfterSetAll()
{
    WTF::BitSet<9, WordType> smallBitSet;
    smallBitSet.setAll();
    EXPECT_EQ(smallBitSet.count(), 9ull);

    WTF::BitSet<32, WordType> completeBitSet;
    completeBitSet.setAll();
    EXPECT_EQ(completeBitSet.count(), 32ull);

    WTF::BitSet<1008, WordType> largeBitSet;
    largeBitSet.setAll();
    EXPECT_EQ(largeBitSet.count(), 1008ull);
}

template<typename WordType>
void testBitSetIsEmpty()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_TRUE(bitSetZeroes.isEmpty());
    EXPECT_FALSE(bitSet1.isEmpty());
    EXPECT_FALSE(bitSet2.isEmpty());
    EXPECT_FALSE(bitSet2Clone.isEmpty());
    EXPECT_FALSE(bitSetOnes.isEmpty());
    EXPECT_TRUE(smallBitSetZeroes.isEmpty());
    EXPECT_FALSE(smallBitSetOnes.isEmpty());

    auto verifyIsEmpty = [=] (const auto& bitSet) {
        EXPECT_TRUE(bitSet.isEmpty());
        EXPECT_EQ(bitSet.count(), zeroSize);
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_FALSE(bitSet.get(i));
    };

    verifyIsEmpty(bitSetZeroes);
    verifyIsEmpty(smallBitSetZeroes);
}

template<typename WordType>
void testBitSetIsFull()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_FALSE(bitSetZeroes.isFull());
    EXPECT_FALSE(bitSet1.isFull());
    EXPECT_FALSE(bitSet2.isFull());
    EXPECT_FALSE(bitSet2Clone.isFull());
    EXPECT_TRUE(bitSetOnes.isFull());
    EXPECT_FALSE(smallBitSetZeroes.isFull());
    EXPECT_TRUE(smallBitSetOnes.isFull());

    auto verifyIsFull = [=] (auto& bitSet) {
        EXPECT_TRUE(bitSet.isFull());
        for (size_t i = 0; i < bitSet.size(); ++i)
            EXPECT_TRUE(bitSet.get(i));
    };

    verifyIsFull(bitSetOnes);
    verifyIsFull(smallBitSetOnes);
}

template<typename WordType>
void testBitSetMerge()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2.merge(bitSetZeroes);
    ASSERT_EQ(bitSet2, bitSet2Clone);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2Clone.merge(bitSet1);
    ASSERT_NE(bitSet2, bitSet2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] | expectedBits1[i];
        ASSERT_EQ(bitSet2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitSetOnes.isFull());
    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSetOnes.merge(bitSetZeroes);
    ASSERT_TRUE(bitSetOnes.isFull());
    ASSERT_TRUE(bitSetZeroes.isEmpty());

    bitSetZeroes.merge(bitSet1);
    ASSERT_EQ(bitSetZeroes, bitSet1);

    bitSet1.merge(bitSetOnes);
    ASSERT_EQ(bitSet1, bitSetOnes);

    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetZeroes.merge(smallBitSetOnes);
    ASSERT_FALSE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isFull());

    smallBitSetZeroes.clearAll();
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetOnes.merge(smallBitSetZeroes);
    ASSERT_TRUE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
}

template<typename WordType>
void testBitSetFilter()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitSetOnes.isFull());
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2.filter(bitSetOnes);
    ASSERT_EQ(bitSet2, bitSet2Clone);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2Clone.filter(bitSet1);
    ASSERT_NE(bitSet2, bitSet2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & expectedBits1[i];
        ASSERT_EQ(bitSet2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSet2.filter(bitSetZeroes);
    ASSERT_EQ(bitSet2, bitSetZeroes);

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSetZeroes.filter(bitSet1);
    ASSERT_TRUE(bitSetZeroes.isEmpty());

    ASSERT_TRUE(bitSetOnes.isFull());
    bitSetOnes.filter(bitSet1);
    ASSERT_EQ(bitSetOnes, bitSet1);

    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetZeroes.filter(smallBitSetOnes);
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_FALSE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isFull());

    smallBitSetOnes.filter(smallBitSetZeroes);
    ASSERT_FALSE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
}

template<typename WordType>
void testBitSetExclude()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2.exclude(bitSetZeroes);
    ASSERT_EQ(bitSet2, bitSet2Clone);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2Clone.exclude(bitSet1);
    ASSERT_NE(bitSet2, bitSet2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & !expectedBits1[i];
        ASSERT_EQ(bitSet2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    ASSERT_TRUE(bitSetOnes.isFull());
    bitSet2.exclude(bitSetOnes);
    ASSERT_EQ(bitSet2, bitSetZeroes);

    ASSERT_TRUE(bitSetOnes.isFull());
    bitSetOnes.exclude(bitSet1);
    bitSet1.invert();
    ASSERT_EQ(bitSetOnes, bitSet1);

    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetZeroes.exclude(smallBitSetOnes);
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_FALSE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isFull());

    smallBitSetOnes.exclude(smallBitSetZeroes);
    ASSERT_TRUE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());

    smallBitSetOnes.exclude(smallBitSetOnes);
    ASSERT_FALSE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
}

template<typename WordType>
void testBitSetConcurrentFilter()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the Filter part of the operation is working.
    ASSERT_TRUE(bitSetOnes.isFull());
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2.concurrentFilter(bitSetOnes);
    ASSERT_EQ(bitSet2, bitSet2Clone);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2, bitSet2Clone);
    bitSet2Clone.concurrentFilter(bitSet1);
    ASSERT_NE(bitSet2, bitSet2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & expectedBits1[i];
        ASSERT_EQ(bitSet2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSet2.concurrentFilter(bitSetZeroes);
    ASSERT_EQ(bitSet2, bitSetZeroes);

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSetZeroes.concurrentFilter(bitSet1);
    ASSERT_TRUE(bitSetZeroes.isEmpty());

    ASSERT_TRUE(bitSetOnes.isFull());
    bitSetOnes.concurrentFilter(bitSet1);
    ASSERT_EQ(bitSetOnes, bitSet1);

    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetZeroes.concurrentFilter(smallBitSetOnes);
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    ASSERT_FALSE(smallBitSetZeroes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isFull());

    smallBitSetOnes.concurrentFilter(smallBitSetZeroes);
    ASSERT_FALSE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
}

template<typename WordType>
void testBitSetSubsumes()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitSetZeroes.isEmpty());
    ASSERT_TRUE(bitSetOnes.isFull());
    ASSERT_EQ(bitSet2, bitSet2Clone);

    ASSERT_TRUE(bitSetZeroes.subsumes(bitSetZeroes));
    ASSERT_FALSE(bitSetZeroes.subsumes(bitSet1));
    ASSERT_FALSE(bitSetZeroes.subsumes(bitSet2));
    ASSERT_FALSE(bitSetZeroes.subsumes(bitSet2Clone));
    ASSERT_FALSE(bitSetZeroes.subsumes(bitSetOnes));

    ASSERT_TRUE(bitSet1.subsumes(bitSetZeroes));
    ASSERT_TRUE(bitSet1.subsumes(bitSet1));
    ASSERT_FALSE(bitSet1.subsumes(bitSet2));
    ASSERT_FALSE(bitSet1.subsumes(bitSet2Clone));
    ASSERT_FALSE(bitSet1.subsumes(bitSetOnes));

    ASSERT_TRUE(bitSet2.subsumes(bitSetZeroes));
    ASSERT_FALSE(bitSet2.subsumes(bitSet1));
    ASSERT_TRUE(bitSet2.subsumes(bitSet2));
    ASSERT_TRUE(bitSet2.subsumes(bitSet2Clone));
    ASSERT_FALSE(bitSet2.subsumes(bitSetOnes));

    ASSERT_TRUE(bitSet2Clone.subsumes(bitSetZeroes));
    ASSERT_FALSE(bitSet2Clone.subsumes(bitSet1));
    ASSERT_TRUE(bitSet2Clone.subsumes(bitSet2));
    ASSERT_TRUE(bitSet2Clone.subsumes(bitSet2Clone));
    ASSERT_FALSE(bitSet2Clone.subsumes(bitSetOnes));

    ASSERT_TRUE(bitSetOnes.subsumes(bitSetZeroes));
    ASSERT_TRUE(bitSetOnes.subsumes(bitSet1));
    ASSERT_TRUE(bitSetOnes.subsumes(bitSet2));
    ASSERT_TRUE(bitSetOnes.subsumes(bitSet2Clone));
    ASSERT_TRUE(bitSetOnes.subsumes(bitSetOnes));

    ASSERT_TRUE(smallBitSetOnes.subsumes(smallBitSetOnes));
    ASSERT_TRUE(smallBitSetOnes.subsumes(smallBitSetZeroes));

    ASSERT_FALSE(smallBitSetZeroes.subsumes(smallBitSetOnes));
    ASSERT_TRUE(smallBitSetZeroes.subsumes(smallBitSetZeroes));
}

template<typename WordType>
void testBitSetForEachSetBit()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    size_t count = 0;
    ASSERT_TRUE(bitSetZeroes.isEmpty());
    bitSetZeroes.forEachSetBit([&] (size_t i) {
        constexpr bool notReached = false;
        ASSERT_TRUE(notReached);
        count++;
    });
    ASSERT_EQ(count, zeroSize);

    count = 0;
    bitSet1.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(bitSet1.get(i));
        ASSERT_EQ(bitSet1.get(i), expectedBits1[i]);
        count++;
    });
    ASSERT_EQ(count, expectedNumberOfSetBits1);

    count = 0;
    ASSERT_TRUE(bitSetOnes.isFull());
    bitSetOnes.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(bitSetOnes.get(i));
        count++;
    });
    ASSERT_EQ(count, size);

    count = 0;
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    smallBitSetZeroes.forEachSetBit([&] (size_t i) {
        constexpr bool notReached = false;
        ASSERT_TRUE(notReached);
        count++;
    });
    ASSERT_EQ(count, zeroSize);

    count = 0;
    ASSERT_TRUE(smallBitSetOnes.isFull());
    smallBitSetOnes.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(smallBitSetOnes.get(i));
        count++;
    });
    ASSERT_EQ(count, smallSize);
}

template<typename WordType>
void testBitSetFindBit()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto findExpectedBit = [=] (auto expectedBits, size_t startIndex, bool value) -> size_t {
        for (auto index = startIndex; index < size; ++index) {
            if (expectedBits[index] == value)
                return index;
        }
        return startIndex;
    };

    for (size_t i = 1, lastIndex = 1; i < size; lastIndex = i, i += lastIndex) {
        ASSERT_EQ(bitSet1.findBit(i, true), findExpectedBit(expectedBits1, i, true));
        ASSERT_EQ(bitSet1.findBit(i, false), findExpectedBit(expectedBits1, i, false));
        ASSERT_EQ(bitSet2.findBit(i, true), findExpectedBit(expectedBits2, i, true));
        ASSERT_EQ(bitSet2.findBit(i, false), findExpectedBit(expectedBits2, i, false));

        ASSERT_EQ(bitSet2.findBit(i, true), bitSet2Clone.findBit(i, true));
        ASSERT_EQ(bitSet2.findBit(i, false), bitSet2Clone.findBit(i, false));
    }

    ASSERT_EQ(bitSetZeroes.findBit(0, false), zeroSize);
    ASSERT_EQ(bitSetZeroes.findBit(10, false), static_cast<size_t>(10));
    ASSERT_EQ(bitSetZeroes.findBit(size - 1, false), size-1);
    ASSERT_EQ(bitSetZeroes.findBit(size, false), size);
    ASSERT_EQ(bitSetZeroes.findBit(size + 1, false), size);

    ASSERT_EQ(bitSetZeroes.findBit(0, true), size);
    ASSERT_EQ(bitSetZeroes.findBit(10, true), size);
    ASSERT_EQ(bitSetZeroes.findBit(size - 1, true), size);
    ASSERT_EQ(bitSetZeroes.findBit(size, true), size);
    ASSERT_EQ(bitSetZeroes.findBit(size + 1, true), size);

    ASSERT_EQ(bitSetOnes.findBit(0, false), size);
    ASSERT_EQ(bitSetOnes.findBit(10, false), size);
    ASSERT_EQ(bitSetOnes.findBit(size - 1, false), size);
    ASSERT_EQ(bitSetOnes.findBit(size, false), size);
    ASSERT_EQ(bitSetOnes.findBit(size + 1, false), size);

    ASSERT_EQ(bitSetOnes.findBit(0, true), zeroSize);
    ASSERT_EQ(bitSetOnes.findBit(10, true), static_cast<size_t>(10));
    ASSERT_EQ(bitSetOnes.findBit(size - 1, true), size-1);
    ASSERT_EQ(bitSetOnes.findBit(size, true), size);
    ASSERT_EQ(bitSetOnes.findBit(size + 1, true), size);
}

template<typename WordType>
void testBitSetIteration()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto computeSetBitsCount = [=] (auto& bitSet) -> size_t {
        size_t count = 0;
        for (auto bitIndex : bitSet) {
            UNUSED_PARAM(bitIndex);
            count++;
        }
        return count;
    };

    EXPECT_EQ(computeSetBitsCount(bitSetZeroes), zeroSize);
    EXPECT_EQ(computeSetBitsCount(bitSet1), expectedNumberOfSetBits1);
    EXPECT_EQ(computeSetBitsCount(bitSet2), expectedNumberOfSetBits2);
    EXPECT_EQ(computeSetBitsCount(bitSet2Clone), expectedNumberOfSetBits2);
    EXPECT_EQ(computeSetBitsCount(bitSetOnes), size);
    EXPECT_EQ(computeSetBitsCount(smallBitSetZeroes), zeroSize);
    EXPECT_EQ(computeSetBitsCount(smallBitSetOnes), smallSize);

    auto verifySetBits = [=] (auto& bitSet, auto& expectedBits) {
        for (auto bitIndex : bitSet) {
            EXPECT_TRUE(bitSet.get(bitIndex));
            EXPECT_EQ(bitSet.get(bitIndex), expectedBits[bitIndex]);
        }
    };

    verifySetBits(bitSet1, expectedBits1);
    verifySetBits(bitSet2, expectedBits2);
    verifySetBits(bitSet2Clone, expectedBits2);

    auto verifyBitsAllSet = [=] (auto& bitSet) {
        size_t lastBitIndex = 0;
        for (auto bitIndex : bitSet) {
            EXPECT_TRUE(bitSet.get(bitIndex));
            if (bitIndex)
                EXPECT_EQ(bitIndex, lastBitIndex + 1);
            lastBitIndex = bitIndex;
        }
    };

    verifyBitsAllSet(bitSetOnes);
    verifyBitsAllSet(smallBitSetOnes);
}

template<typename WordType>
void testBitSetMergeAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    WTF::BitSet<size, WordType> temp;
    WTF::BitSet<size, WordType> savedBitSet1;

    ASSERT_FALSE(bitSet2.isEmpty());

    savedBitSet1.merge(bitSet1);
    ASSERT_EQ(savedBitSet1, bitSet1);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2Clone, bitSet2);
    bitSet1.mergeAndClear(bitSet2Clone);
    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, savedBitSet1);
    ASSERT_TRUE(bitSet2Clone.isEmpty());
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits1[i] | expectedBits2[i];
        ASSERT_EQ(bitSet1.get(i), expectedBit);
    }

    bitSet2Clone.merge(bitSet2); // restore bitSet2Clone
    ASSERT_EQ(bitSet2Clone, bitSet2);

    ASSERT_TRUE(temp.isEmpty());
    temp.mergeAndClear(bitSet2Clone);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_EQ(temp, bitSet2);
    ASSERT_TRUE(bitSet2Clone.isEmpty());

    WTF::BitSet<size, WordType> savedBitSetOnes;
    savedBitSetOnes.merge(bitSetOnes);

    temp.clearAll();
    ASSERT_TRUE(temp.isEmpty());
    ASSERT_FALSE(temp.isFull());
    ASSERT_FALSE(bitSetOnes.isEmpty());
    ASSERT_TRUE(bitSetOnes.isFull());
    temp.mergeAndClear(bitSetOnes);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_TRUE(temp.isFull());
    ASSERT_TRUE(bitSetOnes.isEmpty());
    ASSERT_FALSE(bitSetOnes.isFull());

    bitSet2Clone.merge(bitSet2); // restore bitSet2Clone
    ASSERT_EQ(bitSet2Clone, bitSet2);
    bitSetOnes.merge(savedBitSetOnes); // restore bitSetOnes
    ASSERT_TRUE(bitSetOnes.isFull());

    ASSERT_TRUE(temp.isFull());
    temp.mergeAndClear(bitSet2Clone);
    ASSERT_TRUE(temp.isFull());
    ASSERT_EQ(temp, bitSetOnes);
    ASSERT_TRUE(bitSet2Clone.isEmpty());

    WTF::BitSet<smallSize, WordType> smallTemp;

    smallTemp.merge(smallBitSetOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    smallTemp.mergeAndClear(smallBitSetZeroes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_FALSE(smallTemp.isEmpty());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());

    smallTemp.clearAll();
    ASSERT_TRUE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallTemp.isEmpty());
    smallTemp.mergeAndClear(smallBitSetOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
}

template<typename WordType>
void testBitSetSetAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    WTF::BitSet<size, WordType> temp;
    WTF::BitSet<size, WordType> savedBitSet1;

    ASSERT_FALSE(bitSet2.isEmpty());

    savedBitSet1.merge(bitSet1);
    ASSERT_EQ(savedBitSet1, bitSet1);

    ASSERT_NE(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, bitSet2Clone);
    ASSERT_EQ(bitSet2Clone, bitSet2);
    bitSet1.setAndClear(bitSet2Clone);
    ASSERT_EQ(bitSet1, bitSet2);
    ASSERT_NE(bitSet1, savedBitSet1);
    ASSERT_TRUE(bitSet2Clone.isEmpty());

    bitSet2Clone.merge(bitSet2); // restore bitSet2Clone
    ASSERT_EQ(bitSet2Clone, bitSet2);

    ASSERT_TRUE(temp.isEmpty());
    temp.setAndClear(bitSet2Clone);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_EQ(temp, bitSet2);
    ASSERT_TRUE(bitSet2Clone.isEmpty());

    temp.clearAll();
    ASSERT_TRUE(temp.isEmpty());
    ASSERT_FALSE(temp.isFull());
    ASSERT_FALSE(bitSetOnes.isEmpty());
    ASSERT_TRUE(bitSetOnes.isFull());
    temp.setAndClear(bitSetOnes);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_TRUE(temp.isFull());
    ASSERT_TRUE(bitSetOnes.isEmpty());
    ASSERT_FALSE(bitSetOnes.isFull());

    bitSet2Clone.merge(bitSet2); // restore bitSet2Clone
    ASSERT_EQ(bitSet2Clone, bitSet2);

    ASSERT_TRUE(temp.isFull());
    temp.setAndClear(bitSet2Clone);
    ASSERT_FALSE(temp.isFull());
    ASSERT_EQ(temp, bitSet2);
    ASSERT_TRUE(bitSet2Clone.isEmpty());

    WTF::BitSet<smallSize, WordType> smallTemp;

    smallTemp.merge(smallBitSetOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());
    smallTemp.setAndClear(smallBitSetZeroes);
    ASSERT_TRUE(smallTemp.isEmpty());
    ASSERT_TRUE(smallBitSetZeroes.isEmpty());

    ASSERT_TRUE(smallBitSetOnes.isFull());
    ASSERT_TRUE(smallTemp.isEmpty());
    smallTemp.setAndClear(smallBitSetOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitSetOnes.isEmpty());
}

template<typename BitSet>
void testBitSetSetEachNthBitImpl(size_t size, size_t wordSize, const BitSet& zeroes, const BitSet& ones)
{
    BitSet temp;

    EXPECT_TRUE(temp == zeroes);
    temp.setEachNthBit(1);
    EXPECT_TRUE(temp == ones);

    size_t nValues[] = { 1, 2, wordSize / 2, wordSize - 1, wordSize, size / 2, size - 1, size };
    size_t nValuesCount = sizeof(nValues) / sizeof(nValues[0]);

    size_t startEndValues[] = { 0, 1, 2, wordSize / 2, wordSize - 1, wordSize, size / 2, size - 1, size };
    constexpr size_t numberOfStartEndValues = sizeof(startEndValues) / sizeof(startEndValues[0]);

    for (size_t start = 0; start < numberOfStartEndValues; ++start) {
        for (size_t end = start; end < numberOfStartEndValues; ++end) {
            size_t startIndex = startEndValues[start];
            size_t endIndex = startEndValues[end];
            if (endIndex < startIndex)
                continue;
            if (startIndex > size)
                continue;
            if (endIndex > size)
                continue;

            for (size_t j = 0; j < nValuesCount; ++j) {
                size_t n = nValues[j];
                temp.clearAll();
                temp.setEachNthBit(n, startIndex, endIndex);

                for (size_t i = 0; i < startIndex; ++i)
                    EXPECT_FALSE(temp.get(i));

                size_t count = 0;
                for (size_t i = startIndex; i < endIndex; ++i) {
                    bool expected = !count;
                    EXPECT_TRUE(temp.get(i) == expected);
                    count++;
                    count = count % n;
                }

                for (size_t i = endIndex; i < size; ++i)
                    EXPECT_FALSE(temp.get(i));
            }
        }
    }
}

template<typename WordType>
void testBitSetSetEachNthBit()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    constexpr size_t wordSize = sizeof(WordType) * 8;
    testBitSetSetEachNthBitImpl(size, wordSize, bitSetZeroes, bitSetOnes);
    testBitSetSetEachNthBitImpl(smallSize, wordSize, smallBitSetZeroes, smallBitSetOnes);
}

template<typename WordType>
void testBitSetOperatorEqual()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_TRUE(bitSetZeroes == bitSetZeroes);
    EXPECT_FALSE(bitSetZeroes == bitSet1);
    EXPECT_TRUE(bitSet1 == bitSet1);
    EXPECT_FALSE(bitSet1 == bitSet2);
    EXPECT_FALSE(bitSet1 == bitSet2Clone);
    EXPECT_TRUE(bitSet2 == bitSet2Clone);
    EXPECT_FALSE(bitSetOnes == bitSet1);
    EXPECT_FALSE(smallBitSetZeroes == smallBitSetOnes);
}

template<typename WordType>
void testBitSetOperatorNotEqual()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_FALSE(bitSetZeroes != bitSetZeroes);
    EXPECT_TRUE(bitSetZeroes != bitSet1);
    EXPECT_FALSE(bitSet1 != bitSet1);
    EXPECT_TRUE(bitSet1 != bitSet2);
    EXPECT_TRUE(bitSet1 != bitSet2Clone);
    EXPECT_FALSE(bitSet2 != bitSet2Clone);
    EXPECT_TRUE(bitSetOnes != bitSet1);
    EXPECT_TRUE(smallBitSetZeroes != smallBitSetOnes);
}

template<typename BitSet>
void testBitSetOperatorAssignmentImpl(const BitSet& bitSet1, const BitSet& bitSet2, const BitSet& zeroes, const BitSet& ones)
{
    BitSet temp;
    BitSet temp2;
    EXPECT_TRUE(temp.isEmpty());
    EXPECT_TRUE(temp2.isEmpty());

    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != bitSet1);
    temp = bitSet1;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == bitSet1);

    EXPECT_TRUE(temp != bitSet2);
    temp = bitSet2;
    EXPECT_TRUE(temp == bitSet2);
    EXPECT_TRUE(temp != bitSet1);

    temp.clearAll();
    temp2.clearAll();
    for (size_t i = 0; i < bitSet1.size(); ++i)
        temp.set(i, bitSet1.get(i));

    EXPECT_TRUE(temp == bitSet1);
    EXPECT_TRUE(temp2.isEmpty());
    EXPECT_TRUE(temp2 != bitSet1);
    temp2 = temp;
    EXPECT_TRUE(temp2 == bitSet1);
}

template<typename WordType>
void testBitSetOperatorAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitSetOperatorAssignmentImpl(bitSet1, bitSet2, bitSetZeroes, bitSetOnes);
    testBitSetOperatorAssignmentImpl(smallBitSet1, smallBitSet2, smallBitSetZeroes, smallBitSetOnes);
}

template<typename BitSet>
void testBitSetOperatorBitOrAssignmentImpl(size_t size, const BitSet& bitSet1, const BitSet& bitSet2, const BitSet& zeroes, const BitSet& ones)
{
    BitSet temp;
    BitSet temp1;

    // 0 | 0
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp |= zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    // 0 | 1
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp |= ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    // 1 | 0
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp |= zeroes;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    // 1 | 1
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp |= ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    temp = zeroes;
    EXPECT_TRUE(temp.isEmpty());
    EXPECT_TRUE(temp != bitSet1);
    temp |= bitSet1;
    EXPECT_TRUE(temp == bitSet1);

    temp |= bitSet2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitSet1.get(i) || bitSet2.get(i));

    temp1 = temp;
    EXPECT_TRUE(temp1 == temp);

    temp |= zeroes;
    EXPECT_TRUE(temp == temp1);
    EXPECT_TRUE(temp != zeroes);

    temp |= ones;
    EXPECT_TRUE(temp != temp1);
    EXPECT_TRUE(temp == ones);
}

template<typename WordType>
void testBitSetOperatorBitOrAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitSetOperatorBitOrAssignmentImpl(size, bitSet1, bitSet2, bitSetZeroes, bitSetOnes);
    testBitSetOperatorBitOrAssignmentImpl(smallSize, smallBitSet1, smallBitSet2, smallBitSetZeroes, smallBitSetOnes);
}

template<typename BitSet>
void testBitSetOperatorBitAndAssignmentImpl(size_t size, const BitSet& bitSet1, const BitSet& bitSet2, const BitSet& zeroes, const BitSet& ones)
{
    BitSet temp;
    BitSet temp1;

    // 0 & 0
    temp = zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp &= zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    // 0 & 1
    temp = zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp &= ones;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    // 1 & 0
    temp = ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp &= zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    // 1 & 1
    temp = ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp &= ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    temp = ones;
    EXPECT_TRUE(temp == ones);
    EXPECT_TRUE(temp != bitSet1);
    temp &= bitSet1;
    EXPECT_TRUE(temp == bitSet1);

    temp &= bitSet2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitSet1.get(i) && bitSet2.get(i));

    EXPECT_TRUE(!temp.isEmpty());
    temp1 = temp;
    EXPECT_TRUE(temp1 == temp);

    temp &= zeroes;
    EXPECT_TRUE(temp != temp1);
    EXPECT_TRUE(temp == zeroes);

    temp = temp1;
    temp &= ones;
    EXPECT_TRUE(temp == temp1);
    EXPECT_TRUE(temp != ones);
}

template<typename WordType>
void testBitSetOperatorBitAndAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitSetOperatorBitAndAssignmentImpl(size, bitSet1, bitSet2, bitSetZeroes, bitSetOnes);
    testBitSetOperatorBitAndAssignmentImpl(smallSize, smallBitSet1, smallBitSet2, smallBitSetZeroes, smallBitSetOnes);
}

template<typename BitSet>
void testBitSetOperatorBitXorAssignmentImpl(size_t size, const BitSet& bitSet1, const BitSet& bitSet2, const BitSet& zeroes, const BitSet& ones)
{
    BitSet temp;
    BitSet temp1;

    // 0 ^ 0
    temp = zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp ^= zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    // 0 ^ 1
    temp = zeroes;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);
    temp ^= ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    // 1 ^ 0
    temp = ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp ^= zeroes;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);

    // 1 ^ 1
    temp = ones;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == ones);
    temp ^= ones;
    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != ones);

    temp.clearAll();
    EXPECT_TRUE(temp.isEmpty());
    EXPECT_TRUE(temp != bitSet1);
    temp ^= bitSet1;
    EXPECT_TRUE(temp == bitSet1);

    temp ^= bitSet2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitSet1.get(i) ^ bitSet2.get(i));

    temp1 = temp;
    EXPECT_TRUE(temp1 == temp);

    temp ^= zeroes;
    EXPECT_TRUE(temp == temp1);
    EXPECT_TRUE(temp != zeroes);

    temp ^= ones;
    EXPECT_TRUE(temp != temp1);
    EXPECT_TRUE(temp != ones);
    temp1.invert();
    EXPECT_TRUE(temp == temp1);
    EXPECT_TRUE(temp != ones);
}

template<typename WordType>
void testBitSetOperatorBitXorAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitSetOperatorBitXorAssignmentImpl(size, bitSet1, bitSet2, bitSetZeroes, bitSetOnes);
    testBitSetOperatorBitXorAssignmentImpl(smallSize, smallBitSet1, smallBitSet2, smallBitSetZeroes, smallBitSetOnes);
}

template<typename WordType>
void testBitSetHash()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    constexpr unsigned wordSize = sizeof(WordType) * 8;
    constexpr size_t words = (size + wordSize - 1) / wordSize;

    auto expectedBitSetZeroesHash = [=] () -> unsigned {
        unsigned result = 0;
        WordType zeroWord = 0;
        for (size_t i = 0; i < words; ++i)
            result ^= IntHash<WordType>::hash(zeroWord);
        return result;
    };

    auto expectedBitSetOnesHash = [=] () -> unsigned {
        unsigned result = 0;
        WordType filledWord = static_cast<WordType>(-1);
        for (size_t i = 0; i < words; ++i)
            result ^= IntHash<WordType>::hash(filledWord);
        return result;
    };

    ASSERT_EQ(bitSetZeroes.hash(), expectedBitSetZeroesHash());
    ASSERT_EQ(bitSetOnes.hash(), expectedBitSetOnesHash());

    auto expectedHash = [=] (auto expectedBits, size_t size) {
        unsigned result = 0;
        for (size_t i = 0; i < size;) {
            WordType word = 0;
            for (size_t j = 0; j < wordSize && i < size; ++i, ++j)
                word |= static_cast<WordType>(!!expectedBits[i]) << j;
            result ^= IntHash<WordType>::hash(word);
        }
        return result;
    };

    ASSERT_EQ(bitSet1.hash(), expectedHash(expectedBits1, size));
    ASSERT_EQ(bitSet2.hash(), expectedHash(expectedBits2, size));

    static constexpr bool expectedSmallBits[smallSize] = {
        true,  false, false, true,  false,  false, false, true,
        true,
    };
    WTF::BitSet<smallSize, WordType> temp1;
    WTF::BitSet<smallSize, WordType> temp2;

    auto initTemp = [&] (auto& bitSet) {
        for (size_t i = 0; i < smallSize; ++i)
            bitSet.set(i, expectedSmallBits[i]);
    };

    initTemp(temp1);
    ASSERT_EQ(temp1.hash(), expectedHash(expectedSmallBits, smallSize));

    temp2.invert();
    initTemp(temp2);
    ASSERT_EQ(temp2.hash(), expectedHash(expectedSmallBits, smallSize));
    ASSERT_EQ(temp2.hash(), temp1.hash());
}

TEST(WTF_BitSet, Size_uint32_t) { testBitSetSize<uint32_t>(); }
TEST(WTF_BitSet, ConstructedEmpty_uint32_t) { testBitSetConstructedEmpty<uint32_t>(); }
TEST(WTF_BitSet, SetGet_uint32_t) { testBitSetSetGet<uint32_t>(); }
TEST(WTF_BitSet, TestAndSet_uint32_t) { testBitSetTestAndSet<uint32_t>(); }
TEST(WTF_BitSet, TestAndClear_uint32_t) { testBitSetTestAndClear<uint32_t>(); }
TEST(WTF_BitSet, ConcurrentTestAndSet_uint32_t) { testBitSetConcurrentTestAndSet<uint32_t>(); }
TEST(WTF_BitSet, ConcurrentTestAndClear_uint32_t) { testBitSetConcurrentTestAndClear<uint32_t>(); }
TEST(WTF_BitSet, Clear_uint32_t) { testBitSetClear<uint32_t>(); }
TEST(WTF_BitSet, ClearAll_uint32_t) { testBitSetClearAll<uint32_t>(); }
TEST(WTF_BitSet, Invert_uint32_t) { testBitSetInvert<uint32_t>(); }
TEST(WTF_BitSet, FindRunOfZeros_uint32_t) { testBitSetFindRunOfZeros<uint32_t>(); }
TEST(WTF_BitSet, Count_uint32_t) { testBitSetCount<uint32_t>(); }
TEST(WTF_BitSet, CountAfterSetAll_uint32_t) { testBitSetCountAfterSetAll<uint32_t>(); }
TEST(WTF_BitSet, IsEmpty_uint32_t) { testBitSetIsEmpty<uint32_t>(); }
TEST(WTF_BitSet, IsFull_uint32_t) { testBitSetIsFull<uint32_t>(); }
TEST(WTF_BitSet, Merge_uint32_t) { testBitSetMerge<uint32_t>(); }
TEST(WTF_BitSet, Filter_uint32_t) { testBitSetFilter<uint32_t>(); }
TEST(WTF_BitSet, Exclude_uint32_t) { testBitSetExclude<uint32_t>(); }
TEST(WTF_BitSet, ConcurrentFilter_uint32_t) { testBitSetConcurrentFilter<uint32_t>(); }
TEST(WTF_BitSet, Subsumes_uint32_t) { testBitSetSubsumes<uint32_t>(); }
TEST(WTF_BitSet, ForEachSetBit_uint32_t) { testBitSetForEachSetBit<uint32_t>(); }
TEST(WTF_BitSet, FindBit_uint32_t) { testBitSetFindBit<uint32_t>(); }
TEST(WTF_BitSet, Iteration_uint32_t) { testBitSetIteration<uint32_t>(); }
TEST(WTF_BitSet, MergeAndClear_uint32_t) { testBitSetMergeAndClear<uint32_t>(); }
TEST(WTF_BitSet, SetAndClear_uint32_t) { testBitSetSetAndClear<uint32_t>(); }
TEST(WTF_BitSet, SetEachNthBit_uint32_t) { testBitSetSetEachNthBit<uint32_t>(); }
TEST(WTF_BitSet, OperatorEqualAccess_uint32_t) { testBitSetOperatorEqual<uint32_t>(); }
TEST(WTF_BitSet, OperatorNotEqualAccess_uint32_t) { testBitSetOperatorNotEqual<uint32_t>(); }
TEST(WTF_BitSet, OperatorAssignment_uint32_t) { testBitSetOperatorAssignment<uint32_t>(); }
TEST(WTF_BitSet, OperatorBitOrAssignment_uint32_t) { testBitSetOperatorBitOrAssignment<uint32_t>(); }
TEST(WTF_BitSet, OperatorBitAndAssignment_uint32_t) { testBitSetOperatorBitAndAssignment<uint32_t>(); }
TEST(WTF_BitSet, OperatorBitXorAssignment_uint32_t) { testBitSetOperatorBitXorAssignment<uint32_t>(); }
TEST(WTF_BitSet, Hash_uint32_t) { testBitSetHash<uint32_t>(); }

#if CPU(REGISTER64)

TEST(WTF_BitSet, Size_uint64_t) { testBitSetSize<uint64_t>(); }
TEST(WTF_BitSet, ConstructedEmpty_uint64_t) { testBitSetConstructedEmpty<uint64_t>(); }
TEST(WTF_BitSet, SetGet_uint64_t) { testBitSetSetGet<uint64_t>(); }
TEST(WTF_BitSet, TestAndSet_uint64_t) { testBitSetTestAndSet<uint64_t>(); }
TEST(WTF_BitSet, TestAndClear_uint64_t) { testBitSetTestAndClear<uint64_t>(); }
TEST(WTF_BitSet, ConcurrentTestAndSet_uint64_t) { testBitSetConcurrentTestAndSet<uint64_t>(); }
TEST(WTF_BitSet, ConcurrentTestAndClear_uint64_t) { testBitSetConcurrentTestAndClear<uint64_t>(); }
TEST(WTF_BitSet, Clear_uint64_t) { testBitSetClear<uint64_t>(); }
TEST(WTF_BitSet, ClearAll_uint64_t) { testBitSetClearAll<uint64_t>(); }
TEST(WTF_BitSet, Invert_uint64_t) { testBitSetInvert<uint64_t>(); }
TEST(WTF_BitSet, FindRunOfZeros_uint64_t) { testBitSetFindRunOfZeros<uint64_t>(); }
TEST(WTF_BitSet, Count_uint64_t) { testBitSetCount<uint64_t>(); }
TEST(WTF_BitSet, CountAfterSetAll_uint64_t) { testBitSetCountAfterSetAll<uint64_t>(); }
TEST(WTF_BitSet, IsEmpty_uint64_t) { testBitSetIsEmpty<uint64_t>(); }
TEST(WTF_BitSet, IsFull_uint64_t) { testBitSetIsFull<uint64_t>(); }
TEST(WTF_BitSet, Merge_uint64_t) { testBitSetMerge<uint64_t>(); }
TEST(WTF_BitSet, Filter_uint64_t) { testBitSetFilter<uint64_t>(); }
TEST(WTF_BitSet, Exclude_uint64_t) { testBitSetExclude<uint64_t>(); }
TEST(WTF_BitSet, ConcurrentFilter_uint64_t) { testBitSetConcurrentFilter<uint64_t>(); }
TEST(WTF_BitSet, Subsumes_uint64_t) { testBitSetSubsumes<uint64_t>(); }
TEST(WTF_BitSet, ForEachSetBit_uint64_t) { testBitSetForEachSetBit<uint64_t>(); }
TEST(WTF_BitSet, FindBit_uint64_t) { testBitSetFindBit<uint64_t>(); }
TEST(WTF_BitSet, Iteration_uint64_t) { testBitSetIteration<uint64_t>(); }
TEST(WTF_BitSet, MergeAndClear_uint64_t) { testBitSetMergeAndClear<uint64_t>(); }
TEST(WTF_BitSet, SetAndClear_uint64_t) { testBitSetSetAndClear<uint64_t>(); }
TEST(WTF_BitSet, SetEachNthBit_uint64_t) { testBitSetSetEachNthBit<uint64_t>(); }
TEST(WTF_BitSet, OperatorEqualAccess_uint64_t) { testBitSetOperatorEqual<uint64_t>(); }
TEST(WTF_BitSet, OperatorNotEqualAccess_uint64_t) { testBitSetOperatorNotEqual<uint64_t>(); }
TEST(WTF_BitSet, OperatorAssignment_uint64_t) { testBitSetOperatorAssignment<uint64_t>(); }
TEST(WTF_BitSet, OperatorBitOrAssignment_uint64_t) { testBitSetOperatorBitOrAssignment<uint64_t>(); }
TEST(WTF_BitSet, OperatorBitAndAssignment_uint64_t) { testBitSetOperatorBitAndAssignment<uint64_t>(); }
TEST(WTF_BitSet, OperatorBitXorAssignment_uint64_t) { testBitSetOperatorBitXorAssignment<uint64_t>(); }
TEST(WTF_BitSet, Hash_uint64_t) { testBitSetHash<uint64_t>(); }

#endif // CPU(REGISTER64)

} // namespace TestWebKitAPI
