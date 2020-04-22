//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// bitset_utils_unittest:
//   Tests bitset helpers and custom classes.
//

#include <gtest/gtest.h>

#include "common/bitset_utils.h"

using namespace angle;

namespace
{
class BitSetTest : public testing::Test
{
  protected:
    BitSet<12> mBits;
};

TEST_F(BitSetTest, Basic)
{
    EXPECT_FALSE(mBits.all());
    EXPECT_FALSE(mBits.any());
    EXPECT_TRUE(mBits.none());
    EXPECT_EQ(mBits.count(), 0u);

    // Set every bit to 1.
    for (size_t i = 0; i < mBits.size(); ++i)
    {
        mBits.set(i);

        EXPECT_EQ(mBits.all(), i + 1 == mBits.size());
        EXPECT_TRUE(mBits.any());
        EXPECT_FALSE(mBits.none());
        EXPECT_EQ(mBits.count(), i + 1);
    }

    // Reset every other bit to 0.
    for (size_t i = 0; i < mBits.size(); i += 2)
    {
        mBits.reset(i);

        EXPECT_FALSE(mBits.all());
        EXPECT_TRUE(mBits.any());
        EXPECT_FALSE(mBits.none());
        EXPECT_EQ(mBits.count(), mBits.size() - i / 2 - 1);
    }

    // Flip all bits.
    for (size_t i = 0; i < mBits.size(); ++i)
    {
        mBits.flip(i);

        EXPECT_FALSE(mBits.all());
        EXPECT_TRUE(mBits.any());
        EXPECT_FALSE(mBits.none());
        EXPECT_EQ(mBits.count(), mBits.size() / 2 + (i % 2 == 0));
    }

    // Make sure the bit pattern is what we expect at this point.
    for (size_t i = 0; i < mBits.size(); ++i)
    {
        EXPECT_EQ(mBits.test(i), i % 2 == 0);
        EXPECT_EQ(static_cast<bool>(mBits[i]), i % 2 == 0);
    }

    // Test that flip, set and reset all bits at once work.
    mBits.flip();
    EXPECT_FALSE(mBits.all());
    EXPECT_TRUE(mBits.any());
    EXPECT_FALSE(mBits.none());
    EXPECT_EQ(mBits.count(), mBits.size() / 2);

    mBits.set();
    EXPECT_TRUE(mBits.all());
    EXPECT_TRUE(mBits.any());
    EXPECT_FALSE(mBits.none());
    EXPECT_EQ(mBits.count(), mBits.size());

    mBits.reset();
    EXPECT_FALSE(mBits.all());
    EXPECT_FALSE(mBits.any());
    EXPECT_TRUE(mBits.none());
    EXPECT_EQ(mBits.count(), 0u);

    // Test that out-of-bound sets don't modify the bitset
    constexpr uint32_t kMask = (1 << 12) - 1;

    EXPECT_EQ(mBits.set(12).bits() & ~kMask, 0u);
    EXPECT_EQ(mBits.set(13).bits() & ~kMask, 0u);
    EXPECT_EQ(mBits.flip(12).bits() & ~kMask, 0u);
    EXPECT_EQ(mBits.flip(13).bits() & ~kMask, 0u);
}

TEST_F(BitSetTest, BitwiseOperators)
{
    // Use a value that has a 1 in the 12th and 13th bits, to make sure masking to exactly 12 bits
    // does not have an off-by-one error.
    constexpr uint32_t kSelfValue  = 0xF9E4;
    constexpr uint32_t kOtherValue = 0x5C6A;

    constexpr uint32_t kMask             = (1 << 12) - 1;
    constexpr uint32_t kSelfMaskedValue  = kSelfValue & kMask;
    constexpr uint32_t kOtherMaskedValue = kOtherValue & kMask;

    constexpr uint32_t kShift            = 3;
    constexpr uint32_t kSelfShiftedLeft  = kSelfMaskedValue << kShift & kMask;
    constexpr uint32_t kSelfShiftedRight = kSelfMaskedValue >> kShift & kMask;

    mBits |= kSelfValue;
    BitSet<12> other(kOtherValue);
    BitSet<12> anded(kSelfMaskedValue & kOtherMaskedValue);
    BitSet<12> ored(kSelfMaskedValue | kOtherMaskedValue);
    BitSet<12> xored(kSelfMaskedValue ^ kOtherMaskedValue);

    EXPECT_EQ(mBits.bits(), kSelfMaskedValue);
    EXPECT_EQ(other.bits(), kOtherMaskedValue);

    EXPECT_EQ(mBits & other, anded);
    EXPECT_EQ(mBits | other, ored);
    EXPECT_EQ(mBits ^ other, xored);

    EXPECT_NE(mBits, other);
    EXPECT_NE(anded, ored);
    EXPECT_NE(anded, xored);
    EXPECT_NE(ored, xored);

    mBits &= other;
    EXPECT_EQ(mBits, anded);

    mBits |= ored;
    EXPECT_EQ(mBits, ored);

    mBits ^= other;
    mBits ^= anded;
    EXPECT_EQ(mBits, BitSet<12>(kSelfValue));

    EXPECT_EQ(mBits << kShift, BitSet<12>(kSelfShiftedLeft));
    EXPECT_EQ(mBits >> kShift, BitSet<12>(kSelfShiftedRight));

    mBits <<= kShift;
    EXPECT_EQ(mBits, BitSet<12>(kSelfShiftedLeft));
    EXPECT_EQ(mBits.bits() & ~kMask, 0u);

    mBits = BitSet<12>(kSelfValue);
    mBits >>= kShift;
    EXPECT_EQ(mBits, BitSet<12>(kSelfShiftedRight));
    EXPECT_EQ(mBits.bits() & ~kMask, 0u);

    mBits |= kSelfMaskedValue;
    EXPECT_EQ(mBits.bits() & ~kMask, 0u);
    mBits ^= kOtherMaskedValue;
    EXPECT_EQ(mBits.bits() & ~kMask, 0u);
}

class BitSetIteratorTest : public testing::Test
{
  protected:
    BitSet<40> mStateBits;
};

// Simple iterator test.
TEST_F(BitSetIteratorTest, Iterator)
{
    std::set<size_t> originalValues;
    originalValues.insert(2);
    originalValues.insert(6);
    originalValues.insert(8);
    originalValues.insert(35);

    for (size_t value : originalValues)
    {
        mStateBits.set(value);
    }

    std::set<size_t> readValues;
    for (size_t bit : mStateBits)
    {
        EXPECT_EQ(1u, originalValues.count(bit));
        EXPECT_EQ(0u, readValues.count(bit));
        readValues.insert(bit);
    }

    EXPECT_EQ(originalValues.size(), readValues.size());
}

// Test an empty iterator.
TEST_F(BitSetIteratorTest, EmptySet)
{
    // We don't use the FAIL gtest macro here since it returns immediately,
    // causing an unreachable code warning in MSVS
    bool sawBit = false;
    for (size_t bit : mStateBits)
    {
        sawBit = true;
        ANGLE_UNUSED_VARIABLE(bit);
    }
    EXPECT_FALSE(sawBit);
}

// Test iterating a result of combining two bitsets.
TEST_F(BitSetIteratorTest, NonLValueBitset)
{
    BitSet<40> otherBits;

    mStateBits.set(1);
    mStateBits.set(2);
    mStateBits.set(3);
    mStateBits.set(4);

    otherBits.set(0);
    otherBits.set(1);
    otherBits.set(3);
    otherBits.set(5);

    std::set<size_t> seenBits;

    angle::BitSet<40> maskedBits = (mStateBits & otherBits);
    for (size_t bit : maskedBits)
    {
        EXPECT_EQ(0u, seenBits.count(bit));
        seenBits.insert(bit);
        EXPECT_TRUE(mStateBits[bit]);
        EXPECT_TRUE(otherBits[bit]);
    }

    EXPECT_EQ((mStateBits & otherBits).count(), seenBits.size());
}

// Test bit assignments.
TEST_F(BitSetIteratorTest, BitAssignment)
{
    std::set<size_t> originalValues;
    originalValues.insert(2);
    originalValues.insert(6);
    originalValues.insert(8);
    originalValues.insert(35);

    for (size_t value : originalValues)
    {
        (mStateBits[value] = false) = true;
    }

    for (size_t value : originalValues)
    {
        EXPECT_TRUE(mStateBits.test(value));
    }
}

// Tests adding bits to the iterator during iteration.
TEST_F(BitSetIteratorTest, SetLaterBit)
{
    std::set<size_t> expectedValues = {1, 3, 5, 7, 9};
    mStateBits.set(1);

    std::set<size_t> actualValues;

    for (auto iter = mStateBits.begin(), end = mStateBits.end(); iter != end; ++iter)
    {
        if (*iter == 1)
        {
            iter.setLaterBit(3);
            iter.setLaterBit(5);
            iter.setLaterBit(7);
            iter.setLaterBit(9);
        }

        actualValues.insert(*iter);
    }

    EXPECT_EQ(expectedValues, actualValues);
}

// Tests removing bits from the iterator during iteration.
TEST_F(BitSetIteratorTest, ResetLaterBit)
{
    std::set<size_t> expectedValues = {1, 3, 5, 7, 9};

    for (size_t index = 1; index <= 9; ++index)
        mStateBits.set(index);

    std::set<size_t> actualValues;

    for (auto iter = mStateBits.begin(), end = mStateBits.end(); iter != end; ++iter)
    {
        if (*iter == 1)
        {
            iter.resetLaterBit(2);
            iter.resetLaterBit(4);
            iter.resetLaterBit(6);
            iter.resetLaterBit(8);
        }

        actualValues.insert(*iter);
    }

    EXPECT_EQ(expectedValues, actualValues);
}
}  // anonymous namespace
