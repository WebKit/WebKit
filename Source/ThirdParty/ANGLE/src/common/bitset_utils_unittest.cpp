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
        UNUSED_VARIABLE(bit);
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

}  // anonymous namespace
