/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <wtf/Bitmap.h>

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
    Bitmap<size, WordType> bitmapZeroes; \
    Bitmap<size, WordType> bitmap1; /* Will hold values specified in expectedBits1. */ \
    Bitmap<size, WordType> bitmap2; /* Will hold values specified in expectedBits2. */ \
    Bitmap<size, WordType> bitmap2Clone; /* Same as bitmap2. */ \
    Bitmap<size, WordType> bitmapOnes; \
    Bitmap<smallSize, WordType> smallBitmapZeroes; \
    Bitmap<smallSize, WordType> smallBitmapOnes; \
    Bitmap<smallSize, WordType> smallBitmap1; /* Will hold values specified in expectedSmallBits1. */ \
    Bitmap<smallSize, WordType> smallBitmap2; /* Will hold values specified in expectedSmallBits2. */ \

#define DECLARE_AND_INIT_BITMAPS_FOR_TEST() \
    DECLARE_BITMAPS_FOR_TEST() \
    for (size_t i = 0; i < size; ++i) { \
        bitmap1.set(i, expectedBits1[i]); \
        bitmap2.set(i, expectedBits2[i]); \
        bitmap2Clone.set(i, expectedBits2[i]); \
        bitmapOnes.set(i); \
    } \
    for (size_t i = 0; i < smallSize; ++i) { \
        smallBitmap1.set(i, expectedSmallBits1[i]); \
        smallBitmap2.set(i, expectedSmallBits2[i]); \
        smallBitmapOnes.set(i); \
    }

template<typename WordType>
void testBitmapSize()
{
    DECLARE_BITMAPS_FOR_TEST();

    auto verifySize = [=] (auto& bitmap, size_t expectedSize, size_t notExpectedSize) {
        EXPECT_EQ(bitmap.size(), expectedSize);
        EXPECT_NE(bitmap.size(), notExpectedSize);
    };

    verifySize(bitmapZeroes, size, smallSize);
    verifySize(bitmap1, size, smallSize);
    verifySize(bitmap2, size, smallSize);
    verifySize(bitmap2Clone, size, smallSize);
    verifySize(bitmapOnes, size, smallSize);
    verifySize(smallBitmapZeroes, smallSize, size);
    verifySize(smallBitmapOnes, smallSize, size);
}

template<typename WordType>
void testBitmapConstructedEmpty()
{
    DECLARE_BITMAPS_FOR_TEST();

    // Verify that the default constructor initializes all bits to 0.
    auto verifyIsEmpty = [=] (const auto& bitmap) {
        EXPECT_TRUE(bitmap.isEmpty());
        EXPECT_EQ(bitmap.count(), zeroSize);
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_FALSE(bitmap.get(i));
    };

    verifyIsEmpty(bitmapZeroes);
    verifyIsEmpty(bitmap1);
    verifyIsEmpty(bitmap2);
    verifyIsEmpty(bitmap2Clone);
    verifyIsEmpty(bitmapOnes);
    verifyIsEmpty(smallBitmapZeroes);
    verifyIsEmpty(smallBitmapOnes);
}

template<typename WordType>
void testBitmapSetGet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyIsEmpty = [=] (const auto& bitmap) {
        EXPECT_TRUE(bitmap.isEmpty());
        EXPECT_EQ(bitmap.count(), zeroSize);
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_FALSE(bitmap.get(i));
    };

    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(bitmap1.get(i), expectedBits1[i]);

    for (size_t i = 0; i < size; ++i) {
        EXPECT_EQ(bitmap2.get(i), expectedBits2[i]);
        EXPECT_EQ(bitmap2Clone.get(i), expectedBits2[i]);
    }
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(bitmap2.get(i), bitmap2Clone.get(i));

    for (size_t i = 0; i < size; ++i)
        EXPECT_TRUE(bitmapOnes.get(i));

    for (size_t i = 0; i < smallSize; ++i)
        EXPECT_TRUE(smallBitmapOnes.get(i));

    // Verify that there is no out of bounds write from the above set operations.
    verifyIsEmpty(bitmapZeroes);
    verifyIsEmpty(smallBitmapZeroes);
}

template<typename WordType>
void testBitmapTestAndSet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_FALSE(bitmap1.isFull());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.testAndSet(i), expectedBits1[i]);
    ASSERT_TRUE(bitmap1.isFull());

    ASSERT_FALSE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_FALSE(smallBitmapZeroes.testAndSet(i));
    ASSERT_TRUE(smallBitmapZeroes.isFull());
}

template<typename WordType>
void testBitmapTestAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_FALSE(bitmap1.isEmpty());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.testAndClear(i), expectedBits1[i]);
    ASSERT_TRUE(bitmap1.isEmpty());

    ASSERT_FALSE(smallBitmapOnes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_TRUE(smallBitmapOnes.testAndClear(i));
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
}

template<typename WordType>
void testBitmapConcurrentTestAndSet()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndSet part of the operation is working.
    ASSERT_FALSE(bitmap1.isFull());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.testAndSet(i), expectedBits1[i]);
    ASSERT_TRUE(bitmap1.isFull());

    ASSERT_FALSE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_FALSE(smallBitmapZeroes.testAndSet(i));
    ASSERT_TRUE(smallBitmapZeroes.isFull());
}

template<typename WordType>
void testBitmapConcurrentTestAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndClear part of the operation is working.
    ASSERT_FALSE(bitmap1.isEmpty());
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.testAndClear(i), expectedBits1[i]);
    ASSERT_TRUE(bitmap1.isEmpty());

    ASSERT_FALSE(smallBitmapOnes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_TRUE(smallBitmapOnes.testAndClear(i));
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
}

template<typename WordType>
void testBitmapClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitmapOnes.isFull());
    for (size_t i = 0; i < size; ++i) {
        if (!expectedBits1[i])
            bitmapOnes.clear(i);
    }
    ASSERT_EQ(bitmapOnes, bitmap1);
}

template<typename WordType>
void testBitmapClearAll()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyIsEmpty = [=] (const auto& bitmap) {
        EXPECT_TRUE(bitmap.isEmpty());
        EXPECT_EQ(bitmap.count(), zeroSize);
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_FALSE(bitmap.get(i));
    };

    auto verifyIsFull = [=] (const auto& bitmap) {
        EXPECT_TRUE(bitmap.isFull());
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_TRUE(bitmap.get(i));
    };

    EXPECT_FALSE(bitmap1.isEmpty());
    bitmap1.clearAll();
    verifyIsEmpty(bitmap1);
    verifyIsFull(bitmapOnes);
    verifyIsFull(smallBitmapOnes);

    EXPECT_FALSE(bitmap2.isEmpty());
    bitmap2.clearAll();
    verifyIsEmpty(bitmap1);
    verifyIsEmpty(bitmap2);
    verifyIsFull(bitmapOnes);
    verifyIsFull(smallBitmapOnes);

    EXPECT_FALSE(bitmap2Clone.isEmpty());
    bitmap2Clone.clearAll();
    verifyIsEmpty(bitmap1);
    verifyIsEmpty(bitmap2);
    verifyIsEmpty(bitmap2Clone);
    verifyIsFull(bitmapOnes);
    verifyIsFull(smallBitmapOnes);
}

template<typename WordType>
void testBitmapInvert()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto verifyInvert = [=] (auto& bitmap) {
        ASSERT_TRUE(bitmap.isFull());
        ASSERT_FALSE(bitmap.isEmpty());
        bitmap.invert();
        ASSERT_FALSE(bitmap.isFull());
        ASSERT_TRUE(bitmap.isEmpty());
        bitmap.invert();
        ASSERT_TRUE(bitmap.isFull());
        ASSERT_FALSE(bitmap.isEmpty());
    };

    verifyInvert(bitmapOnes);
    verifyInvert(smallBitmapOnes);

    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
    bitmap1.invert();
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), !expectedBits1[i]);
    bitmap1.invert();
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
}

template<typename WordType>
void testBitmapFindRunOfZeros()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    const int64_t bitmap1RunsOfZeros[15] = {
        0, 0, 0, 3, 3,
        3, 3, 3, 3, 3,
        3, 3, 3, -1, -1
    };

    const int64_t bitmap2RunsOfZeros[15] = {
        0, 0, 4, 4, 4,
        4, 4, 4, -1, -1,
        -1, -1, -1, -1, -1,
    };

    Bitmap<smallSize, WordType> smallTemp;
    smallTemp.set(1, true); // bits low to high are: 0100 0000 0

    const int64_t smallTempRunsOfZeros[15] = {
        0, 0, 2, 2, 2,
        2, 2, 2, -1, -1,
        -1, -1, -1, -1, -1
    };

    for (size_t runLength = 0; runLength < 15; ++runLength) {
        ASSERT_EQ(bitmapZeroes.findRunOfZeros(runLength), 0ll);
        ASSERT_EQ(bitmap1.findRunOfZeros(runLength), bitmap1RunsOfZeros[runLength]);
        ASSERT_EQ(bitmap2.findRunOfZeros(runLength), bitmap2RunsOfZeros[runLength]);
        ASSERT_EQ(bitmapOnes.findRunOfZeros(runLength), -1ll);

        if (runLength <= smallSize)
            ASSERT_EQ(smallBitmapZeroes.findRunOfZeros(runLength), 0ll);
        else
            ASSERT_EQ(smallBitmapZeroes.findRunOfZeros(runLength), -1ll);

        ASSERT_EQ(smallBitmapOnes.findRunOfZeros(runLength), -1ll);

        ASSERT_EQ(smallTemp.findRunOfZeros(runLength), smallTempRunsOfZeros[runLength]);
    }
}

template<typename WordType>
void testBitmapCount()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_EQ(bitmapZeroes.count(), zeroSize);
    EXPECT_EQ(bitmap1.count(), expectedNumberOfSetBits1);
    EXPECT_EQ(bitmap2.count(), expectedNumberOfSetBits2);
    EXPECT_EQ(bitmap2Clone.count(), expectedNumberOfSetBits2);
    EXPECT_EQ(bitmapOnes.count(), size);
    EXPECT_EQ(smallBitmapZeroes.count(), zeroSize);
    EXPECT_EQ(smallBitmapOnes.count(), smallSize);
}

template<typename WordType>
void testBitmapIsEmpty()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_TRUE(bitmapZeroes.isEmpty());
    EXPECT_FALSE(bitmap1.isEmpty());
    EXPECT_FALSE(bitmap2.isEmpty());
    EXPECT_FALSE(bitmap2Clone.isEmpty());
    EXPECT_FALSE(bitmapOnes.isEmpty());
    EXPECT_TRUE(smallBitmapZeroes.isEmpty());
    EXPECT_FALSE(smallBitmapOnes.isEmpty());

    auto verifyIsEmpty = [=] (const auto& bitmap) {
        EXPECT_TRUE(bitmap.isEmpty());
        EXPECT_EQ(bitmap.count(), zeroSize);
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_FALSE(bitmap.get(i));
    };

    verifyIsEmpty(bitmapZeroes);
    verifyIsEmpty(smallBitmapZeroes);
}

template<typename WordType>
void testBitmapIsFull()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_FALSE(bitmapZeroes.isFull());
    EXPECT_FALSE(bitmap1.isFull());
    EXPECT_FALSE(bitmap2.isFull());
    EXPECT_FALSE(bitmap2Clone.isFull());
    EXPECT_TRUE(bitmapOnes.isFull());
    EXPECT_FALSE(smallBitmapZeroes.isFull());
    EXPECT_TRUE(smallBitmapOnes.isFull());

    auto verifyIsFull = [=] (auto& bitmap) {
        EXPECT_TRUE(bitmap.isFull());
        for (size_t i = 0; i < bitmap.size(); ++i)
            EXPECT_TRUE(bitmap.get(i));
    };

    verifyIsFull(bitmapOnes);
    verifyIsFull(smallBitmapOnes);
}

template<typename WordType>
void testBitmapMerge()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2.merge(bitmapZeroes);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2Clone.merge(bitmap1);
    ASSERT_NE(bitmap2, bitmap2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] | expectedBits1[i];
        ASSERT_EQ(bitmap2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitmapOnes.isFull());
    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmapOnes.merge(bitmapZeroes);
    ASSERT_TRUE(bitmapOnes.isFull());
    ASSERT_TRUE(bitmapZeroes.isEmpty());

    bitmapZeroes.merge(bitmap1);
    ASSERT_EQ(bitmapZeroes, bitmap1);

    bitmap1.merge(bitmapOnes);
    ASSERT_EQ(bitmap1, bitmapOnes);

    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapZeroes.merge(smallBitmapOnes);
    ASSERT_FALSE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    
    smallBitmapZeroes.clearAll();
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapOnes.merge(smallBitmapZeroes);
    ASSERT_TRUE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
}

template<typename WordType>
void testBitmapFilter()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitmapOnes.isFull());
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2.filter(bitmapOnes);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2Clone.filter(bitmap1);
    ASSERT_NE(bitmap2, bitmap2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & expectedBits1[i];
        ASSERT_EQ(bitmap2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmap2.filter(bitmapZeroes);
    ASSERT_EQ(bitmap2, bitmapZeroes);

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmapZeroes.filter(bitmap1);
    ASSERT_TRUE(bitmapZeroes.isEmpty());

    ASSERT_TRUE(bitmapOnes.isFull());
    bitmapOnes.filter(bitmap1);
    ASSERT_EQ(bitmapOnes, bitmap1);

    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapZeroes.filter(smallBitmapOnes);
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_FALSE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    
    smallBitmapOnes.filter(smallBitmapZeroes);
    ASSERT_FALSE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
}

template<typename WordType>
void testBitmapExclude()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2.exclude(bitmapZeroes);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2Clone.exclude(bitmap1);
    ASSERT_NE(bitmap2, bitmap2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & !expectedBits1[i];
        ASSERT_EQ(bitmap2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    ASSERT_TRUE(bitmapOnes.isFull());
    bitmap2.exclude(bitmapOnes);
    ASSERT_EQ(bitmap2, bitmapZeroes);

    ASSERT_TRUE(bitmapOnes.isFull());
    bitmapOnes.exclude(bitmap1);
    bitmap1.invert();
    ASSERT_EQ(bitmapOnes, bitmap1);

    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapZeroes.exclude(smallBitmapOnes);
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_FALSE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    
    smallBitmapOnes.exclude(smallBitmapZeroes);
    ASSERT_TRUE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());

    smallBitmapOnes.exclude(smallBitmapOnes);
    ASSERT_FALSE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
}

template<typename WordType>
void testBitmapConcurrentFilter()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the Filter part of the operation is working.
    ASSERT_TRUE(bitmapOnes.isFull());
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2.concurrentFilter(bitmapOnes);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2, bitmap2Clone);
    bitmap2Clone.concurrentFilter(bitmap1);
    ASSERT_NE(bitmap2, bitmap2Clone);
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits2[i] & expectedBits1[i];
        ASSERT_EQ(bitmap2Clone.get(i), expectedBit);
    }

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmap2.concurrentFilter(bitmapZeroes);
    ASSERT_EQ(bitmap2, bitmapZeroes);

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmapZeroes.concurrentFilter(bitmap1);
    ASSERT_TRUE(bitmapZeroes.isEmpty());

    ASSERT_TRUE(bitmapOnes.isFull());
    bitmapOnes.concurrentFilter(bitmap1);
    ASSERT_EQ(bitmapOnes, bitmap1);

    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapZeroes.concurrentFilter(smallBitmapOnes);
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    ASSERT_FALSE(smallBitmapZeroes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isFull());
    
    smallBitmapOnes.concurrentFilter(smallBitmapZeroes);
    ASSERT_FALSE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
}

template<typename WordType>
void testBitmapSubsumes()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    ASSERT_TRUE(bitmapZeroes.isEmpty());
    ASSERT_TRUE(bitmapOnes.isFull());
    ASSERT_EQ(bitmap2, bitmap2Clone);

    ASSERT_TRUE(bitmapZeroes.subsumes(bitmapZeroes));
    ASSERT_FALSE(bitmapZeroes.subsumes(bitmap1));
    ASSERT_FALSE(bitmapZeroes.subsumes(bitmap2));
    ASSERT_FALSE(bitmapZeroes.subsumes(bitmap2Clone));
    ASSERT_FALSE(bitmapZeroes.subsumes(bitmapOnes));

    ASSERT_TRUE(bitmap1.subsumes(bitmapZeroes));
    ASSERT_TRUE(bitmap1.subsumes(bitmap1));
    ASSERT_FALSE(bitmap1.subsumes(bitmap2));
    ASSERT_FALSE(bitmap1.subsumes(bitmap2Clone));
    ASSERT_FALSE(bitmap1.subsumes(bitmapOnes));

    ASSERT_TRUE(bitmap2.subsumes(bitmapZeroes));
    ASSERT_FALSE(bitmap2.subsumes(bitmap1));
    ASSERT_TRUE(bitmap2.subsumes(bitmap2));
    ASSERT_TRUE(bitmap2.subsumes(bitmap2Clone));
    ASSERT_FALSE(bitmap2.subsumes(bitmapOnes));

    ASSERT_TRUE(bitmap2Clone.subsumes(bitmapZeroes));
    ASSERT_FALSE(bitmap2Clone.subsumes(bitmap1));
    ASSERT_TRUE(bitmap2Clone.subsumes(bitmap2));
    ASSERT_TRUE(bitmap2Clone.subsumes(bitmap2Clone));
    ASSERT_FALSE(bitmap2Clone.subsumes(bitmapOnes));

    ASSERT_TRUE(bitmapOnes.subsumes(bitmapZeroes));
    ASSERT_TRUE(bitmapOnes.subsumes(bitmap1));
    ASSERT_TRUE(bitmapOnes.subsumes(bitmap2));
    ASSERT_TRUE(bitmapOnes.subsumes(bitmap2Clone));
    ASSERT_TRUE(bitmapOnes.subsumes(bitmapOnes));

    ASSERT_TRUE(smallBitmapOnes.subsumes(smallBitmapOnes));
    ASSERT_TRUE(smallBitmapOnes.subsumes(smallBitmapZeroes));

    ASSERT_FALSE(smallBitmapZeroes.subsumes(smallBitmapOnes));
    ASSERT_TRUE(smallBitmapZeroes.subsumes(smallBitmapZeroes));
}

template<typename WordType>
void testBitmapForEachSetBit()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    size_t count = 0;
    ASSERT_TRUE(bitmapZeroes.isEmpty());
    bitmapZeroes.forEachSetBit([&] (size_t i) {
        constexpr bool notReached = false;
        ASSERT_TRUE(notReached);
        count++;
    });
    ASSERT_EQ(count, zeroSize);

    count = 0;
    bitmap1.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(bitmap1.get(i));
        ASSERT_EQ(bitmap1.get(i), expectedBits1[i]);
        count++;
    });
    ASSERT_EQ(count, expectedNumberOfSetBits1);

    count = 0;
    ASSERT_TRUE(bitmapOnes.isFull());
    bitmapOnes.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(bitmapOnes.get(i));
        count++;
    });
    ASSERT_EQ(count, size);

    count = 0;
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    smallBitmapZeroes.forEachSetBit([&] (size_t i) {
        constexpr bool notReached = false;
        ASSERT_TRUE(notReached);
        count++;
    });
    ASSERT_EQ(count, zeroSize);

    count = 0;
    ASSERT_TRUE(smallBitmapOnes.isFull());
    smallBitmapOnes.forEachSetBit([&] (size_t i) {
        ASSERT_TRUE(smallBitmapOnes.get(i));
        count++;
    });
    ASSERT_EQ(count, smallSize);
}

template<typename WordType>
void testBitmapFindBit()
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
        ASSERT_EQ(bitmap1.findBit(i, true), findExpectedBit(expectedBits1, i, true));
        ASSERT_EQ(bitmap1.findBit(i, false), findExpectedBit(expectedBits1, i, false));
        ASSERT_EQ(bitmap2.findBit(i, true), findExpectedBit(expectedBits2, i, true));
        ASSERT_EQ(bitmap2.findBit(i, false), findExpectedBit(expectedBits2, i, false));

        ASSERT_EQ(bitmap2.findBit(i, true), bitmap2Clone.findBit(i, true));
        ASSERT_EQ(bitmap2.findBit(i, false), bitmap2Clone.findBit(i, false));
    }

    ASSERT_EQ(bitmapZeroes.findBit(0, false), zeroSize);
    ASSERT_EQ(bitmapZeroes.findBit(10, false), static_cast<size_t>(10));
    ASSERT_EQ(bitmapZeroes.findBit(size - 1, false), size-1);
    ASSERT_EQ(bitmapZeroes.findBit(size, false), size);
    ASSERT_EQ(bitmapZeroes.findBit(size + 1, false), size);

    ASSERT_EQ(bitmapZeroes.findBit(0, true), size);
    ASSERT_EQ(bitmapZeroes.findBit(10, true), size);
    ASSERT_EQ(bitmapZeroes.findBit(size - 1, true), size);
    ASSERT_EQ(bitmapZeroes.findBit(size, true), size);
    ASSERT_EQ(bitmapZeroes.findBit(size + 1, true), size);

    ASSERT_EQ(bitmapOnes.findBit(0, false), size);
    ASSERT_EQ(bitmapOnes.findBit(10, false), size);
    ASSERT_EQ(bitmapOnes.findBit(size - 1, false), size);
    ASSERT_EQ(bitmapOnes.findBit(size, false), size);
    ASSERT_EQ(bitmapOnes.findBit(size + 1, false), size);

    ASSERT_EQ(bitmapOnes.findBit(0, true), zeroSize);
    ASSERT_EQ(bitmapOnes.findBit(10, true), static_cast<size_t>(10));
    ASSERT_EQ(bitmapOnes.findBit(size - 1, true), size-1);
    ASSERT_EQ(bitmapOnes.findBit(size, true), size);
    ASSERT_EQ(bitmapOnes.findBit(size + 1, true), size);
}

template<typename WordType>
void testBitmapIteration()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    auto computeSetBitsCount = [=] (auto& bitmap) -> size_t {
        size_t count = 0;
        for (auto bitIndex : bitmap) {
            UNUSED_PARAM(bitIndex);
            count++;
        }
        return count;
    };

    EXPECT_EQ(computeSetBitsCount(bitmapZeroes), zeroSize);
    EXPECT_EQ(computeSetBitsCount(bitmap1), expectedNumberOfSetBits1);
    EXPECT_EQ(computeSetBitsCount(bitmap2), expectedNumberOfSetBits2);
    EXPECT_EQ(computeSetBitsCount(bitmap2Clone), expectedNumberOfSetBits2);
    EXPECT_EQ(computeSetBitsCount(bitmapOnes), size);
    EXPECT_EQ(computeSetBitsCount(smallBitmapZeroes), zeroSize);
    EXPECT_EQ(computeSetBitsCount(smallBitmapOnes), smallSize);

    auto verifySetBits = [=] (auto& bitmap, auto& expectedBits) {
        for (auto bitIndex : bitmap) {
            EXPECT_TRUE(bitmap.get(bitIndex));
            EXPECT_EQ(bitmap.get(bitIndex), expectedBits[bitIndex]);
        }
    };

    verifySetBits(bitmap1, expectedBits1);
    verifySetBits(bitmap2, expectedBits2);
    verifySetBits(bitmap2Clone, expectedBits2);

    auto verifyBitsAllSet = [=] (auto& bitmap) {
        size_t lastBitIndex = 0;
        for (auto bitIndex : bitmap) {
            EXPECT_TRUE(bitmap.get(bitIndex));
            if (bitIndex)
                EXPECT_EQ(bitIndex, lastBitIndex + 1);
            lastBitIndex = bitIndex;
        }
    };

    verifyBitsAllSet(bitmapOnes);
    verifyBitsAllSet(smallBitmapOnes);
}

template<typename WordType>
void testBitmapMergeAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    Bitmap<size, WordType> temp;
    Bitmap<size, WordType> savedBitmap1;

    ASSERT_FALSE(bitmap2.isEmpty());

    savedBitmap1.merge(bitmap1);
    ASSERT_EQ(savedBitmap1, bitmap1);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2Clone, bitmap2);
    bitmap1.mergeAndClear(bitmap2Clone);
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, savedBitmap1);
    ASSERT_TRUE(bitmap2Clone.isEmpty());
    for (size_t i = 0; i < size; ++i) {
        bool expectedBit = expectedBits1[i] | expectedBits2[i];
        ASSERT_EQ(bitmap1.get(i), expectedBit);
    }

    bitmap2Clone.merge(bitmap2); // restore bitmap2Clone
    ASSERT_EQ(bitmap2Clone, bitmap2);

    ASSERT_TRUE(temp.isEmpty());
    temp.mergeAndClear(bitmap2Clone);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_EQ(temp, bitmap2);
    ASSERT_TRUE(bitmap2Clone.isEmpty());

    Bitmap<size, WordType> savedBitmapOnes;
    savedBitmapOnes.merge(bitmapOnes);

    temp.clearAll();
    ASSERT_TRUE(temp.isEmpty());
    ASSERT_FALSE(temp.isFull());
    ASSERT_FALSE(bitmapOnes.isEmpty());
    ASSERT_TRUE(bitmapOnes.isFull());
    temp.mergeAndClear(bitmapOnes);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_TRUE(temp.isFull());
    ASSERT_TRUE(bitmapOnes.isEmpty());
    ASSERT_FALSE(bitmapOnes.isFull());

    bitmap2Clone.merge(bitmap2); // restore bitmap2Clone
    ASSERT_EQ(bitmap2Clone, bitmap2);
    bitmapOnes.merge(savedBitmapOnes); // restore bitmapOnes
    ASSERT_TRUE(bitmapOnes.isFull());

    ASSERT_TRUE(temp.isFull());
    temp.mergeAndClear(bitmap2Clone);
    ASSERT_TRUE(temp.isFull());
    ASSERT_EQ(temp, bitmapOnes);
    ASSERT_TRUE(bitmap2Clone.isEmpty());

    Bitmap<smallSize, WordType> smallTemp;

    smallTemp.merge(smallBitmapOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    smallTemp.mergeAndClear(smallBitmapZeroes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_FALSE(smallTemp.isEmpty());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());

    smallTemp.clearAll();
    ASSERT_TRUE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallTemp.isEmpty());
    smallTemp.mergeAndClear(smallBitmapOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
}

template<typename WordType>
void testBitmapSetAndClear()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    Bitmap<size, WordType> temp;
    Bitmap<size, WordType> savedBitmap1;

    ASSERT_FALSE(bitmap2.isEmpty());

    savedBitmap1.merge(bitmap1);
    ASSERT_EQ(savedBitmap1, bitmap1);
    
    ASSERT_NE(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, bitmap2Clone);
    ASSERT_EQ(bitmap2Clone, bitmap2);
    bitmap1.setAndClear(bitmap2Clone);
    ASSERT_EQ(bitmap1, bitmap2);
    ASSERT_NE(bitmap1, savedBitmap1);
    ASSERT_TRUE(bitmap2Clone.isEmpty());

    bitmap2Clone.merge(bitmap2); // restore bitmap2Clone
    ASSERT_EQ(bitmap2Clone, bitmap2);

    ASSERT_TRUE(temp.isEmpty());
    temp.setAndClear(bitmap2Clone);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_EQ(temp, bitmap2);
    ASSERT_TRUE(bitmap2Clone.isEmpty());

    temp.clearAll();
    ASSERT_TRUE(temp.isEmpty());
    ASSERT_FALSE(temp.isFull());
    ASSERT_FALSE(bitmapOnes.isEmpty());
    ASSERT_TRUE(bitmapOnes.isFull());
    temp.setAndClear(bitmapOnes);
    ASSERT_FALSE(temp.isEmpty());
    ASSERT_TRUE(temp.isFull());
    ASSERT_TRUE(bitmapOnes.isEmpty());
    ASSERT_FALSE(bitmapOnes.isFull());

    bitmap2Clone.merge(bitmap2); // restore bitmap2Clone
    ASSERT_EQ(bitmap2Clone, bitmap2);

    ASSERT_TRUE(temp.isFull());
    temp.setAndClear(bitmap2Clone);
    ASSERT_FALSE(temp.isFull());
    ASSERT_EQ(temp, bitmap2);
    ASSERT_TRUE(bitmap2Clone.isEmpty());

    Bitmap<smallSize, WordType> smallTemp;

    smallTemp.merge(smallBitmapOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    smallTemp.setAndClear(smallBitmapZeroes);
    ASSERT_TRUE(smallTemp.isEmpty());
    ASSERT_TRUE(smallBitmapZeroes.isEmpty());
    
    ASSERT_TRUE(smallBitmapOnes.isFull());
    ASSERT_TRUE(smallTemp.isEmpty());
    smallTemp.setAndClear(smallBitmapOnes);
    ASSERT_TRUE(smallTemp.isFull());
    ASSERT_TRUE(smallBitmapOnes.isEmpty());
}

template<typename Bitmap>
void testBitmapSetEachNthBitImpl(size_t size, size_t wordSize, const Bitmap& zeroes, const Bitmap& ones)
{
    Bitmap temp;

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
void testBitmapSetEachNthBit()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    constexpr size_t wordSize = sizeof(WordType) * 8;
    testBitmapSetEachNthBitImpl(size, wordSize, bitmapZeroes, bitmapOnes);
    testBitmapSetEachNthBitImpl(smallSize, wordSize, smallBitmapZeroes, smallBitmapOnes);
}

template<typename WordType>
void testBitmapOperatorEqual()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_TRUE(bitmapZeroes == bitmapZeroes);
    EXPECT_FALSE(bitmapZeroes == bitmap1);
    EXPECT_TRUE(bitmap1 == bitmap1);
    EXPECT_FALSE(bitmap1 == bitmap2);
    EXPECT_FALSE(bitmap1 == bitmap2Clone);
    EXPECT_TRUE(bitmap2 == bitmap2Clone);
    EXPECT_FALSE(bitmapOnes == bitmap1);
    EXPECT_FALSE(smallBitmapZeroes == smallBitmapOnes);
}

template<typename WordType>
void testBitmapOperatorNotEqual()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    EXPECT_FALSE(bitmapZeroes != bitmapZeroes);
    EXPECT_TRUE(bitmapZeroes != bitmap1);
    EXPECT_FALSE(bitmap1 != bitmap1);
    EXPECT_TRUE(bitmap1 != bitmap2);
    EXPECT_TRUE(bitmap1 != bitmap2Clone);
    EXPECT_FALSE(bitmap2 != bitmap2Clone);
    EXPECT_TRUE(bitmapOnes != bitmap1);
    EXPECT_TRUE(smallBitmapZeroes != smallBitmapOnes);
}

template<typename Bitmap>
void testBitmapOperatorAssignmentImpl(const Bitmap& bitmap1, const Bitmap& bitmap2, const Bitmap& zeroes, const Bitmap& ones)
{
    Bitmap temp;
    Bitmap temp2;
    EXPECT_TRUE(temp.isEmpty());
    EXPECT_TRUE(temp2.isEmpty());

    EXPECT_TRUE(temp == zeroes);
    EXPECT_TRUE(temp != bitmap1);
    temp = bitmap1;
    EXPECT_TRUE(temp != zeroes);
    EXPECT_TRUE(temp == bitmap1);

    EXPECT_TRUE(temp != bitmap2);
    temp = bitmap2;
    EXPECT_TRUE(temp == bitmap2);
    EXPECT_TRUE(temp != bitmap1);

    temp.clearAll();
    temp2.clearAll();
    for (size_t i = 0; i < bitmap1.size(); ++i)
        temp.set(i, bitmap1.get(i));

    EXPECT_TRUE(temp == bitmap1);
    EXPECT_TRUE(temp2.isEmpty());
    EXPECT_TRUE(temp2 != bitmap1);
    temp2 = temp;
    EXPECT_TRUE(temp2 == bitmap1);
}

template<typename WordType>
void testBitmapOperatorAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitmapOperatorAssignmentImpl(bitmap1, bitmap2, bitmapZeroes, bitmapOnes);
    testBitmapOperatorAssignmentImpl(smallBitmap1, smallBitmap2, smallBitmapZeroes, smallBitmapOnes);
}

template<typename Bitmap>
void testBitmapOperatorBitOrAssignmentImpl(size_t size, const Bitmap& bitmap1, const Bitmap& bitmap2, const Bitmap& zeroes, const Bitmap& ones)
{
    Bitmap temp;
    Bitmap temp1;

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
    EXPECT_TRUE(temp != bitmap1);
    temp |= bitmap1;
    EXPECT_TRUE(temp == bitmap1);

    temp |= bitmap2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitmap1.get(i) | bitmap2.get(i));

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
void testBitmapOperatorBitOrAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitmapOperatorBitOrAssignmentImpl(size, bitmap1, bitmap2, bitmapZeroes, bitmapOnes);
    testBitmapOperatorBitOrAssignmentImpl(smallSize, smallBitmap1, smallBitmap2, smallBitmapZeroes, smallBitmapOnes);
}

template<typename Bitmap>
void testBitmapOperatorBitAndAssignmentImpl(size_t size, const Bitmap& bitmap1, const Bitmap& bitmap2, const Bitmap& zeroes, const Bitmap& ones)
{
    Bitmap temp;
    Bitmap temp1;

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
    EXPECT_TRUE(temp != bitmap1);
    temp &= bitmap1;
    EXPECT_TRUE(temp == bitmap1);

    temp &= bitmap2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitmap1.get(i) & bitmap2.get(i));

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
void testBitmapOperatorBitAndAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitmapOperatorBitAndAssignmentImpl(size, bitmap1, bitmap2, bitmapZeroes, bitmapOnes);
    testBitmapOperatorBitAndAssignmentImpl(smallSize, smallBitmap1, smallBitmap2, smallBitmapZeroes, smallBitmapOnes);
}

template<typename Bitmap>
void testBitmapOperatorBitXorAssignmentImpl(size_t size, const Bitmap& bitmap1, const Bitmap& bitmap2, const Bitmap& zeroes, const Bitmap& ones)
{
    Bitmap temp;
    Bitmap temp1;

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
    EXPECT_TRUE(temp != bitmap1);
    temp ^= bitmap1;
    EXPECT_TRUE(temp == bitmap1);

    temp ^= bitmap2;
    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(temp.get(i), bitmap1.get(i) ^ bitmap2.get(i));

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
void testBitmapOperatorBitXorAssignment()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();
    testBitmapOperatorBitXorAssignmentImpl(size, bitmap1, bitmap2, bitmapZeroes, bitmapOnes);
    testBitmapOperatorBitXorAssignmentImpl(smallSize, smallBitmap1, smallBitmap2, smallBitmapZeroes, smallBitmapOnes);
}

template<typename WordType>
void testBitmapHash()
{
    DECLARE_AND_INIT_BITMAPS_FOR_TEST();

    constexpr unsigned wordSize = sizeof(WordType) * 8;
    constexpr size_t words = (size + wordSize - 1) / wordSize;

    auto expectedBitmapZeroesHash = [=] () -> unsigned {
        unsigned result = 0;
        WordType zeroWord = 0;
        for (size_t i = 0; i < words; ++i)
            result ^= IntHash<WordType>::hash(zeroWord);
        return result;
    };

    auto expectedBitmapOnesHash = [=] () -> unsigned {
        unsigned result = 0;
        WordType filledWord = static_cast<WordType>(-1);
        for (size_t i = 0; i < words; ++i)
            result ^= IntHash<WordType>::hash(filledWord);
        return result;
    };

    ASSERT_EQ(bitmapZeroes.hash(), expectedBitmapZeroesHash());
    ASSERT_EQ(bitmapOnes.hash(), expectedBitmapOnesHash());

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

    ASSERT_EQ(bitmap1.hash(), expectedHash(expectedBits1, size));
    ASSERT_EQ(bitmap2.hash(), expectedHash(expectedBits2, size));

    static constexpr bool expectedSmallBits[smallSize] = {
        true,  false, false, true,  false,  false, false, true,
        true,
    };
    Bitmap<smallSize, WordType> temp1;
    Bitmap<smallSize, WordType> temp2;

    auto initTemp = [&] (auto& bitmap) {
        for (size_t i = 0; i < smallSize; ++i)
            bitmap.set(i, expectedSmallBits[i]);
    };

    initTemp(temp1);
    ASSERT_EQ(temp1.hash(), expectedHash(expectedSmallBits, smallSize));

    temp2.invert();
    initTemp(temp2);
    ASSERT_EQ(temp2.hash(), expectedHash(expectedSmallBits, smallSize));
    ASSERT_EQ(temp2.hash(), temp1.hash());
}

TEST(WTF_Bitmap, Size_uint32_t) { testBitmapSize<uint32_t>(); }
TEST(WTF_Bitmap, ConstructedEmpty_uint32_t) { testBitmapConstructedEmpty<uint32_t>(); }
TEST(WTF_Bitmap, SetGet_uint32_t) { testBitmapSetGet<uint32_t>(); }
TEST(WTF_Bitmap, TestAndSet_uint32_t) { testBitmapTestAndSet<uint32_t>(); }
TEST(WTF_Bitmap, TestAndClear_uint32_t) { testBitmapTestAndClear<uint32_t>(); }
TEST(WTF_Bitmap, ConcurrentTestAndSet_uint32_t) { testBitmapConcurrentTestAndSet<uint32_t>(); }
TEST(WTF_Bitmap, ConcurrentTestAndClear_uint32_t) { testBitmapConcurrentTestAndClear<uint32_t>(); }
TEST(WTF_Bitmap, Clear_uint32_t) { testBitmapClear<uint32_t>(); }
TEST(WTF_Bitmap, ClearAll_uint32_t) { testBitmapClearAll<uint32_t>(); }
TEST(WTF_Bitmap, Invert_uint32_t) { testBitmapInvert<uint32_t>(); }
TEST(WTF_Bitmap, FindRunOfZeros_uint32_t) { testBitmapFindRunOfZeros<uint32_t>(); }
TEST(WTF_Bitmap, Count_uint32_t) { testBitmapCount<uint32_t>(); }
TEST(WTF_Bitmap, IsEmpty_uint32_t) { testBitmapIsEmpty<uint32_t>(); }
TEST(WTF_Bitmap, IsFull_uint32_t) { testBitmapIsFull<uint32_t>(); }
TEST(WTF_Bitmap, Merge_uint32_t) { testBitmapMerge<uint32_t>(); }
TEST(WTF_Bitmap, Filter_uint32_t) { testBitmapFilter<uint32_t>(); }
TEST(WTF_Bitmap, Exclude_uint32_t) { testBitmapExclude<uint32_t>(); }
TEST(WTF_Bitmap, ConcurrentFilter_uint32_t) { testBitmapConcurrentFilter<uint32_t>(); }
TEST(WTF_Bitmap, Subsumes_uint32_t) { testBitmapSubsumes<uint32_t>(); }
TEST(WTF_Bitmap, ForEachSetBit_uint32_t) { testBitmapForEachSetBit<uint32_t>(); }
TEST(WTF_Bitmap, FindBit_uint32_t) { testBitmapFindBit<uint32_t>(); }
TEST(WTF_Bitmap, Iteration_uint32_t) { testBitmapIteration<uint32_t>(); }
TEST(WTF_Bitmap, MergeAndClear_uint32_t) { testBitmapMergeAndClear<uint32_t>(); }
TEST(WTF_Bitmap, SetAndClear_uint32_t) { testBitmapSetAndClear<uint32_t>(); }
TEST(WTF_Bitmap, SetEachNthBit_uint32_t) { testBitmapSetEachNthBit<uint32_t>(); }
TEST(WTF_Bitmap, OperatorEqualAccess_uint32_t) { testBitmapOperatorEqual<uint32_t>(); }
TEST(WTF_Bitmap, OperatorNotEqualAccess_uint32_t) { testBitmapOperatorNotEqual<uint32_t>(); }
TEST(WTF_Bitmap, OperatorAssignment_uint32_t) { testBitmapOperatorAssignment<uint32_t>(); }
TEST(WTF_Bitmap, OperatorBitOrAssignment_uint32_t) { testBitmapOperatorBitOrAssignment<uint32_t>(); }
TEST(WTF_Bitmap, OperatorBitAndAssignment_uint32_t) { testBitmapOperatorBitAndAssignment<uint32_t>(); }
TEST(WTF_Bitmap, OperatorBitXorAssignment_uint32_t) { testBitmapOperatorBitXorAssignment<uint32_t>(); }
TEST(WTF_Bitmap, Hash_uint32_t) { testBitmapHash<uint32_t>(); }

#if CPU(REGISTER64)

TEST(WTF_Bitmap, Size_uint64_t) { testBitmapSize<uint64_t>(); }
TEST(WTF_Bitmap, ConstructedEmpty_uint64_t) { testBitmapConstructedEmpty<uint64_t>(); }
TEST(WTF_Bitmap, SetGet_uint64_t) { testBitmapSetGet<uint64_t>(); }
TEST(WTF_Bitmap, TestAndSet_uint64_t) { testBitmapTestAndSet<uint64_t>(); }
TEST(WTF_Bitmap, TestAndClear_uint64_t) { testBitmapTestAndClear<uint64_t>(); }
TEST(WTF_Bitmap, ConcurrentTestAndSet_uint64_t) { testBitmapConcurrentTestAndSet<uint64_t>(); }
TEST(WTF_Bitmap, ConcurrentTestAndClear_uint64_t) { testBitmapConcurrentTestAndClear<uint64_t>(); }
TEST(WTF_Bitmap, Clear_uint64_t) { testBitmapClear<uint64_t>(); }
TEST(WTF_Bitmap, ClearAll_uint64_t) { testBitmapClearAll<uint64_t>(); }
TEST(WTF_Bitmap, Invert_uint64_t) { testBitmapInvert<uint64_t>(); }
TEST(WTF_Bitmap, FindRunOfZeros_uint64_t) { testBitmapFindRunOfZeros<uint64_t>(); }
TEST(WTF_Bitmap, Count_uint64_t) { testBitmapCount<uint64_t>(); }
TEST(WTF_Bitmap, IsEmpty_uint64_t) { testBitmapIsEmpty<uint64_t>(); }
TEST(WTF_Bitmap, IsFull_uint64_t) { testBitmapIsFull<uint64_t>(); }
TEST(WTF_Bitmap, Merge_uint64_t) { testBitmapMerge<uint64_t>(); }
TEST(WTF_Bitmap, Filter_uint64_t) { testBitmapFilter<uint64_t>(); }
TEST(WTF_Bitmap, Exclude_uint64_t) { testBitmapExclude<uint64_t>(); }
TEST(WTF_Bitmap, ConcurrentFilter_uint64_t) { testBitmapConcurrentFilter<uint64_t>(); }
TEST(WTF_Bitmap, Subsumes_uint64_t) { testBitmapSubsumes<uint64_t>(); }
TEST(WTF_Bitmap, ForEachSetBit_uint64_t) { testBitmapForEachSetBit<uint64_t>(); }
TEST(WTF_Bitmap, FindBit_uint64_t) { testBitmapFindBit<uint64_t>(); }
TEST(WTF_Bitmap, Iteration_uint64_t) { testBitmapIteration<uint64_t>(); }
TEST(WTF_Bitmap, MergeAndClear_uint64_t) { testBitmapMergeAndClear<uint64_t>(); }
TEST(WTF_Bitmap, SetAndClear_uint64_t) { testBitmapSetAndClear<uint64_t>(); }
TEST(WTF_Bitmap, SetEachNthBit_uint64_t) { testBitmapSetEachNthBit<uint64_t>(); }
TEST(WTF_Bitmap, OperatorEqualAccess_uint64_t) { testBitmapOperatorEqual<uint64_t>(); }
TEST(WTF_Bitmap, OperatorNotEqualAccess_uint64_t) { testBitmapOperatorNotEqual<uint64_t>(); }
TEST(WTF_Bitmap, OperatorAssignment_uint64_t) { testBitmapOperatorAssignment<uint64_t>(); }
TEST(WTF_Bitmap, OperatorBitOrAssignment_uint64_t) { testBitmapOperatorBitOrAssignment<uint64_t>(); }
TEST(WTF_Bitmap, OperatorBitAndAssignment_uint64_t) { testBitmapOperatorBitAndAssignment<uint64_t>(); }
TEST(WTF_Bitmap, OperatorBitXorAssignment_uint64_t) { testBitmapOperatorBitXorAssignment<uint64_t>(); }
TEST(WTF_Bitmap, Hash_uint64_t) { testBitmapHash<uint64_t>(); }

#endif // CPU(REGISTER64)

} // namespace TestWebKitAPI
