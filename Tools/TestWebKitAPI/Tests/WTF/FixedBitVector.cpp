/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include <wtf/FixedBitVector.h>
#include <wtf/MathExtras.h>

namespace TestWebKitAPI {

constexpr size_t size = 128;
constexpr size_t smallSize = 9;
constexpr size_t zeroSize = 0;
constexpr size_t bitVectorMaxInlineBits = (sizeof(void*) << 3) - 1; 

static constexpr bool expectedBits[size] = {
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

static constexpr bool expectedSmallBits[65] = {
    false, true, true,  false,  true,  false, false, true, true,

    // Additional unused booleans to make testFixedBitVectorFindBit not read out of bounds.
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
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

constexpr size_t expectedNumberOfSetBits = countBits(expectedBits, size);
constexpr size_t expectedNumberOfSetBitsSmall = countBits(expectedSmallBits, smallSize);

#define DECLARE_FIXEDBITVECTORS_FOR_TEST()                  \
    FixedBitVector v = FixedBitVector(size);                \
    FixedBitVector vClone = FixedBitVector(size);           \
    FixedBitVector vSmall = FixedBitVector(smallSize);      \
    FixedBitVector vSmallClone = FixedBitVector(smallSize); \
    FixedBitVector vZero = FixedBitVector(zeroSize);        \
    FixedBitVector vZeroClone = FixedBitVector(zeroSize);   \
    UNUSED_VARIABLE(v);                                     \
    UNUSED_VARIABLE(vSmall);                                \
    UNUSED_VARIABLE(vZero);                                 \
    UNUSED_VARIABLE(vClone);                                \
    UNUSED_VARIABLE(vSmallClone);                           \
    UNUSED_VARIABLE(vZeroClone);

#define DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST() \
    DECLARE_FIXEDBITVECTORS_FOR_TEST()              \
    for (size_t i = 0; i < size; ++i) {             \
        if (expectedBits[i]) {                      \
            v.concurrentTestAndSet(i);              \
            vClone.concurrentTestAndSet(i);         \
        }                                           \
    }                                               \
    for (size_t i = 0; i < smallSize; ++i) {        \
        if (expectedSmallBits[i]) {                 \
            vSmall.concurrentTestAndSet(i);         \
            vSmallClone.concurrentTestAndSet(i);    \
        }                                           \
    }

void testFixedBitVectorSize()
{
    DECLARE_FIXEDBITVECTORS_FOR_TEST()

    EXPECT_EQ(v.size(), size);
    EXPECT_EQ(vSmall.size(), bitVectorMaxInlineBits);
    EXPECT_EQ(vZero.size(), bitVectorMaxInlineBits);
}

void testFixedBitVectorTest()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST()

    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(v.test(i), expectedBits[i]);
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_EQ(vSmall.test(i), expectedSmallBits[i]);
}

void testFixedBitVectorTestAndSet()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST()

    for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(v.testAndSet(i), expectedBits[i]);
        ASSERT_EQ(v.test(i), true);
    }

    for (size_t i = 0; i < smallSize; ++i) {
        ASSERT_EQ(vSmall.testAndSet(i), expectedSmallBits[i]);
        ASSERT_EQ(vSmall.test(i), true);
    }
}

void testFixedBitVectorTestAndClear()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST()

    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(v.testAndClear(i), expectedBits[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(v.test(i), false);
    
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_EQ(vSmall.testAndClear(i), expectedSmallBits[i]);
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_EQ(vSmall.test(i), false);
}

void testFixedBitVectorConcurrentTestAndSet()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST()

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndSet part of the operation is working.
    for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(v.concurrentTestAndSet(i), expectedBits[i]);
        ASSERT_EQ(v.test(i), true);
    }

    for (size_t i = 0; i < smallSize; ++i) {
        ASSERT_EQ(vSmall.concurrentTestAndSet(i), expectedSmallBits[i]);
        ASSERT_EQ(vSmall.test(i), true);
    }
}

void testFixedBitVectorConcurrentTestAndClear()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST()

    // FIXME: the following does not test the concurrent part. It only ensures that
    // the TestAndSet part of the operation is working.
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(v.concurrentTestAndClear(i), expectedBits[i]);
    for (size_t i = 0; i < size; ++i)
        ASSERT_EQ(v.test(i), false);

    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_EQ(vSmall.concurrentTestAndClear(i), expectedSmallBits[i]);
    for (size_t i = 0; i < smallSize; ++i)
        ASSERT_EQ(vSmall.test(i), false);
}

void testFixedBitVectorFindBit()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST();

    auto findExpectedBit = [&](auto bitArray, size_t bitArraySize, size_t startIndex, bool value) -> size_t {
        for (auto index = startIndex; index < bitArraySize; ++index) {
            if (bitArray[index] == value)
                return index;
        }
        return bitArraySize;
    };

    for (size_t i = 1; i < smallSize; i++) {
        ASSERT_EQ(vSmall.findBit(i, true), findExpectedBit(expectedSmallBits, bitVectorMaxInlineBits, i, true));
        ASSERT_EQ(vSmall.findBit(i, false), findExpectedBit(expectedSmallBits, bitVectorMaxInlineBits, i, false));
    }
}

void testFixedBitVectorIteration()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST();

    auto computeSetBitsCount = [=](auto& fixedBitVector) -> size_t {
        size_t count = 0;
        for (auto bitIndex : fixedBitVector) {
            UNUSED_PARAM(bitIndex);
            count++;
        }
        return count;
    };

    EXPECT_EQ(computeSetBitsCount(v), expectedNumberOfSetBits);
    EXPECT_EQ(computeSetBitsCount(vSmall), expectedNumberOfSetBitsSmall);
    EXPECT_EQ(computeSetBitsCount(vZero), zeroSize);
}

void testFixedBitVectorOperatorEqual()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST();

    EXPECT_TRUE(v == v);
    EXPECT_TRUE(v == vClone);

    EXPECT_TRUE(vSmall == vSmall);
    EXPECT_TRUE(vSmall == vSmallClone);

    EXPECT_TRUE(vZero == vZero);
    EXPECT_TRUE(vZero == vZeroClone);

    EXPECT_FALSE(v == vSmall);
    EXPECT_FALSE(vSmallClone == vClone);

    EXPECT_FALSE(vZero == vSmall);
}

void testFixedBitVectorHash()
{
    DECLARE_AND_INIT_FIXEDBITVECTORS_FOR_TEST();

    auto expectedHash = [=] (auto expectedBits, size_t size) {
        BitVector vec;
        vec.resize(size);
        for (size_t i = 0; i < size; ++i) {
            if (expectedBits[i])
                vec.set(i);
        }
        return vec.hash();
    };

    ASSERT_EQ(v.hash(), expectedHash(expectedBits, size));
    ASSERT_EQ(vSmall.hash(), expectedHash(expectedSmallBits, smallSize));
    ASSERT_EQ(vZero.hash(), expectedHash(expectedSmallBits, zeroSize));
}

TEST(WTF_FixedBitVector, testSize) { testFixedBitVectorSize(); }

TEST(WTF_FixedBitVector, testTest) { testFixedBitVectorTest(); }
TEST(WTF_FixedBitVector, testtTestAndSet) { testFixedBitVectorTestAndSet(); }
TEST(WTF_FixedBitVector, testtTestAndClear) { testFixedBitVectorTestAndClear(); }

TEST(WTF_FixedBitVector, testConcurrentTestAndSet) { testFixedBitVectorConcurrentTestAndSet(); }
TEST(WTF_FixedBitVector, testConcurrentTestAndClear) { testFixedBitVectorConcurrentTestAndClear(); }

TEST(WTF_FixedBitVector, testVectorFindBit) { testFixedBitVectorFindBit(); }
TEST(WTF_FixedBitVector, testIteration) { testFixedBitVectorIteration(); }
TEST(WTF_FixedBitVector, testOperatorEqual) { testFixedBitVectorOperatorEqual(); }
TEST(WTF_FixedBitVector, testHash) { testFixedBitVectorHash(); }

} // namespace TestWebKitAPI
