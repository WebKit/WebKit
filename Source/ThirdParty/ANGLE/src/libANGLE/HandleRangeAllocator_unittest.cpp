//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for HandleRangeAllocator.
//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "libANGLE/HandleRangeAllocator.h"

namespace
{

class HandleRangeAllocatorTest : public testing::Test
{
  protected:
    gl::HandleRangeAllocator *getAllocator() { return &mAllocator; }

  private:
    gl::HandleRangeAllocator mAllocator;
};

// Checks basic functionality: allocate, release, isUsed.
TEST_F(HandleRangeAllocatorTest, TestBasic)
{
    auto *allocator = getAllocator();
    // Check that resource 1 is not in use
    EXPECT_FALSE(allocator->isUsed(1));

    // Allocate an ID, check that it's in use.
    GLuint id1 = allocator->allocate();
    EXPECT_TRUE(allocator->isUsed(id1));

    // Allocate another ID, check that it's in use, and different from the first
    // one.
    GLuint id2 = allocator->allocate();
    EXPECT_TRUE(allocator->isUsed(id2));
    EXPECT_NE(id1, id2);

    // Free one of the IDs, check that it's not in use any more.
    allocator->release(id1);
    EXPECT_FALSE(allocator->isUsed(id1));

    // Frees the other ID, check that it's not in use any more.
    allocator->release(id2);
    EXPECT_FALSE(allocator->isUsed(id2));
}

// Checks that the resource handles are re-used after being freed.
TEST_F(HandleRangeAllocatorTest, TestAdvanced)
{
    auto *allocator = getAllocator();

    // Allocate the highest possible ID, to make life awkward.
    allocator->allocateAtOrAbove(~static_cast<GLuint>(0));

    // Allocate a significant number of resources.
    const unsigned int kNumResources = 100;
    GLuint ids[kNumResources];
    for (unsigned int i = 0; i < kNumResources; ++i)
    {
        ids[i] = allocator->allocate();
        EXPECT_TRUE(allocator->isUsed(ids[i]));
    }

    // Check that a new allocation re-uses the resource we just freed.
    GLuint id1 = ids[kNumResources / 2];
    allocator->release(id1);
    EXPECT_FALSE(allocator->isUsed(id1));
    GLuint id2 = allocator->allocate();
    EXPECT_TRUE(allocator->isUsed(id2));
    EXPECT_EQ(id1, id2);
}

// Checks that we can choose our own ids and they won't be reused.
TEST_F(HandleRangeAllocatorTest, MarkAsUsed)
{
    auto *allocator = getAllocator();
    GLuint id       = allocator->allocate();
    allocator->release(id);
    EXPECT_FALSE(allocator->isUsed(id));
    EXPECT_TRUE(allocator->markAsUsed(id));
    EXPECT_TRUE(allocator->isUsed(id));
    GLuint id2 = allocator->allocate();
    EXPECT_NE(id, id2);
    EXPECT_TRUE(allocator->markAsUsed(id2 + 1));
    GLuint id3 = allocator->allocate();
    // Checks our algorithm. If the algorithm changes this check should be
    // changed.
    EXPECT_EQ(id3, id2 + 2);
}

// Checks allocateAtOrAbove.
TEST_F(HandleRangeAllocatorTest, AllocateAtOrAbove)
{
    const GLuint kOffset = 123456;
    auto *allocator      = getAllocator();
    GLuint id1           = allocator->allocateAtOrAbove(kOffset);
    EXPECT_EQ(kOffset, id1);
    GLuint id2 = allocator->allocateAtOrAbove(kOffset);
    EXPECT_GT(id2, kOffset);
    GLuint id3 = allocator->allocateAtOrAbove(kOffset);
    EXPECT_GT(id3, kOffset);
}

// Checks that allocateAtOrAbove wraps around at the maximum value.
TEST_F(HandleRangeAllocatorTest, AllocateIdAtOrAboveWrapsAround)
{
    const GLuint kMaxPossibleOffset = ~static_cast<GLuint>(0);
    auto *allocator                 = getAllocator();
    GLuint id1                      = allocator->allocateAtOrAbove(kMaxPossibleOffset);
    EXPECT_EQ(kMaxPossibleOffset, id1);
    GLuint id2 = allocator->allocateAtOrAbove(kMaxPossibleOffset);
    EXPECT_EQ(1u, id2);
    GLuint id3 = allocator->allocateAtOrAbove(kMaxPossibleOffset);
    EXPECT_EQ(2u, id3);
}

// Checks that freeing an already freed range causes no harm.
TEST_F(HandleRangeAllocatorTest, RedundantFreeIsIgnored)
{
    auto *allocator = getAllocator();
    GLuint id1      = allocator->allocate();
    allocator->release(0);
    allocator->release(id1);
    allocator->release(id1);
    allocator->release(id1 + 1);
    GLuint id2 = allocator->allocate();
    GLuint id3 = allocator->allocate();
    EXPECT_NE(id2, id3);
    EXPECT_NE(allocator->kInvalidHandle, id2);
    EXPECT_NE(allocator->kInvalidHandle, id3);
}

// Check allocating and releasing multiple ranges.
TEST_F(HandleRangeAllocatorTest, allocateRange)
{
    const GLuint kMaxPossibleOffset = std::numeric_limits<GLuint>::max();

    auto *allocator = getAllocator();

    GLuint id1 = allocator->allocateRange(1);
    EXPECT_EQ(1u, id1);
    GLuint id2 = allocator->allocateRange(2);
    EXPECT_EQ(2u, id2);
    GLuint id3 = allocator->allocateRange(3);
    EXPECT_EQ(4u, id3);
    GLuint id4 = allocator->allocate();
    EXPECT_EQ(7u, id4);
    allocator->release(3);
    GLuint id5 = allocator->allocateRange(1);
    EXPECT_EQ(3u, id5);
    allocator->release(5);
    allocator->release(2);
    allocator->release(4);
    GLuint id6 = allocator->allocateRange(2);
    EXPECT_EQ(4u, id6);
    GLuint id7 = allocator->allocateAtOrAbove(kMaxPossibleOffset);
    EXPECT_EQ(kMaxPossibleOffset, id7);
    GLuint id8 = allocator->allocateAtOrAbove(kMaxPossibleOffset);
    EXPECT_EQ(2u, id8);
    GLuint id9 = allocator->allocateRange(50);
    EXPECT_EQ(8u, id9);
    GLuint id10 = allocator->allocateRange(50);
    EXPECT_EQ(58u, id10);
    // Remove all the low-numbered ids.
    allocator->release(1);
    allocator->release(15);
    allocator->releaseRange(2, 107);
    GLuint id11 = allocator->allocateRange(100);
    EXPECT_EQ(1u, id11);
    allocator->release(kMaxPossibleOffset);
    GLuint id12 = allocator->allocateRange(100);
    EXPECT_EQ(101u, id12);

    GLuint id13 = allocator->allocateAtOrAbove(kMaxPossibleOffset - 2u);
    EXPECT_EQ(kMaxPossibleOffset - 2u, id13);
    GLuint id14 = allocator->allocateRange(3);
    EXPECT_EQ(201u, id14);
}

// Checks that having allocated a high range doesn't interfere
// with normal low range allocation.
TEST_F(HandleRangeAllocatorTest, AllocateRangeEndNoEffect)
{
    const GLuint kMaxPossibleOffset = std::numeric_limits<GLuint>::max();

    auto *allocator = getAllocator();
    GLuint id1      = allocator->allocateAtOrAbove(kMaxPossibleOffset - 2u);
    EXPECT_EQ(kMaxPossibleOffset - 2u, id1);
    GLuint id3 = allocator->allocateRange(3);
    EXPECT_EQ(1u, id3);
    GLuint id2 = allocator->allocateRange(2);
    EXPECT_EQ(4u, id2);
}

// Checks allocating a range that consumes the whole uint32 space.
TEST_F(HandleRangeAllocatorTest, AllocateMax)
{
    const uint32_t kMaxPossibleRange = std::numeric_limits<uint32_t>::max();

    auto *allocator = getAllocator();
    GLuint id       = allocator->allocateRange(kMaxPossibleRange);
    EXPECT_EQ(1u, id);
    allocator->releaseRange(id, kMaxPossibleRange - 1u);
    GLuint id2 = allocator->allocateRange(kMaxPossibleRange);
    EXPECT_EQ(0u, id2);
    allocator->releaseRange(id, kMaxPossibleRange);
    GLuint id3 = allocator->allocateRange(kMaxPossibleRange);
    EXPECT_EQ(1u, id3);
}

// Checks allocating a range that consumes the whole uint32 space
// causes next allocation to fail.
// Subsequently checks that once the big range is reduced new allocations
// are possible.
TEST_F(HandleRangeAllocatorTest, AllocateFullRange)
{
    const uint32_t kMaxPossibleRange = std::numeric_limits<uint32_t>::max();
    const GLuint kFreedId            = 555u;
    auto *allocator                  = getAllocator();

    GLuint id1 = allocator->allocateRange(kMaxPossibleRange);
    EXPECT_EQ(1u, id1);
    GLuint id2 = allocator->allocate();
    EXPECT_EQ(gl::HandleRangeAllocator::kInvalidHandle, id2);
    allocator->release(kFreedId);
    GLuint id3 = allocator->allocate();
    EXPECT_EQ(kFreedId, id3);
    GLuint id4 = allocator->allocate();
    EXPECT_EQ(0u, id4);
    allocator->release(kFreedId + 1u);
    allocator->release(kFreedId + 4u);
    allocator->release(kFreedId + 3u);
    allocator->release(kFreedId + 5u);
    allocator->release(kFreedId + 2u);
    GLuint id5 = allocator->allocateRange(5);
    EXPECT_EQ(kFreedId + 1u, id5);
}

// Checks that allocating a range that exceeds uint32
// does not wrap incorrectly and fails.
TEST_F(HandleRangeAllocatorTest, AllocateRangeNoWrapInRange)
{
    const uint32_t kMaxPossibleRange = std::numeric_limits<uint32_t>::max();
    const GLuint kAllocId            = 10u;
    auto *allocator                  = getAllocator();

    GLuint id1 = allocator->allocateAtOrAbove(kAllocId);
    EXPECT_EQ(kAllocId, id1);
    GLuint id2 = allocator->allocateRange(kMaxPossibleRange - 5u);
    EXPECT_EQ(0u, id2);
    GLuint id3 = allocator->allocateRange(kMaxPossibleRange - kAllocId);
    EXPECT_EQ(kAllocId + 1u, id3);
}

// Check special cases for 0 range allocations and zero handles.
TEST_F(HandleRangeAllocatorTest, ZeroIdCases)
{
    auto *allocator = getAllocator();
    EXPECT_FALSE(allocator->isUsed(0));
    GLuint id1 = allocator->allocateAtOrAbove(0);
    EXPECT_NE(0u, id1);
    EXPECT_FALSE(allocator->isUsed(0));
    allocator->release(0);
    EXPECT_FALSE(allocator->isUsed(0));
    EXPECT_TRUE(allocator->isUsed(id1));
    allocator->release(id1);
    EXPECT_FALSE(allocator->isUsed(id1));
}

}  // namespace