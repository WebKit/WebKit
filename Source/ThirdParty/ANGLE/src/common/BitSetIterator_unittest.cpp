//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BitSetIteratorTest:
//   Test the IterableBitSet class.
//

#include <gtest/gtest.h>

#include "common/BitSetIterator.h"

using namespace angle;

namespace
{
class BitSetIteratorTest : public testing::Test
{
  protected:
    std::bitset<40> mStateBits;
};

// Simple iterator test.
TEST_F(BitSetIteratorTest, Iterator)
{
    std::set<unsigned long> originalValues;
    originalValues.insert(2);
    originalValues.insert(6);
    originalValues.insert(8);
    originalValues.insert(35);

    for (unsigned long value : originalValues)
    {
        mStateBits.set(value);
    }

    std::set<unsigned long> readValues;
    for (unsigned long bit : IterateBitSet(mStateBits))
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
    for (unsigned long bit : IterateBitSet(mStateBits))
    {
        sawBit = true;
        UNUSED_VARIABLE(bit);
    }
    EXPECT_FALSE(sawBit);
}

// Test iterating a result of combining two bitsets.
TEST_F(BitSetIteratorTest, NonLValueBitset)
{
    std::bitset<40> otherBits;

    mStateBits.set(1);
    mStateBits.set(2);
    mStateBits.set(3);
    mStateBits.set(4);

    otherBits.set(0);
    otherBits.set(1);
    otherBits.set(3);
    otherBits.set(5);

    std::set<unsigned long> seenBits;

    for (unsigned long bit : IterateBitSet(mStateBits & otherBits))
    {
        EXPECT_EQ(0u, seenBits.count(bit));
        seenBits.insert(bit);
        EXPECT_TRUE(mStateBits[bit]);
        EXPECT_TRUE(otherBits[bit]);
    }

    EXPECT_EQ((mStateBits & otherBits).count(), seenBits.size());
}

}  // anonymous namespace
