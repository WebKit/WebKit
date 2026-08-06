//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for HandleAllocator.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include <unordered_set>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "libANGLE/HandleAllocator.h"

namespace
{

constexpr GLuint kMaxHandleForTesting = std::numeric_limits<GLuint>::max();

TEST(HandleAllocatorTest, ReservationsWithGaps)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    std::set<GLuint> allocationList;
    for (GLuint id = 2; id < 50; id += 2)
    {
        allocationList.insert(id);
    }

    for (GLuint id : allocationList)
    {
        allocator.reserve(id);
    }

    std::set<GLuint> allocatedList;
    for (size_t allocationNum = 0; allocationNum < allocationList.size() * 2; ++allocationNum)
    {
        GLuint handle = 0;
        EXPECT_TRUE(allocator.allocate(&handle));
        EXPECT_EQ(0u, allocationList.count(handle));
        EXPECT_EQ(0u, allocatedList.count(handle));
        allocatedList.insert(handle);
    }
}

TEST(HandleAllocatorTest, Random)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    std::set<GLuint> allocationList;
    for (size_t iterationCount = 0; iterationCount < 40; ++iterationCount)
    {
        for (size_t randomCount = 0; randomCount < 40; ++randomCount)
        {
            GLuint randomHandle = (rand() % 1000) + 1;
            if (allocationList.count(randomHandle) == 0)
            {
                allocator.reserve(randomHandle);
                allocationList.insert(randomHandle);
            }
        }

        for (size_t normalCount = 0; normalCount < 40; ++normalCount)
        {
            GLuint normalHandle = 0;
            EXPECT_TRUE(allocator.allocate(&normalHandle));
            EXPECT_EQ(0u, allocationList.count(normalHandle));
            allocationList.insert(normalHandle);
        }
    }
}

TEST(HandleAllocatorTest, Reallocation)
{
    // Note: no current test for overflow
    gl::HandleAllocator limitedAllocator(10);

    for (GLuint count = 1; count < 10; count++)
    {
        GLuint result = 0;
        EXPECT_TRUE(limitedAllocator.allocate(&result));
        EXPECT_EQ(count, result);
    }

    for (GLuint count = 1; count < 10; count++)
    {
        limitedAllocator.release(count);
    }

    for (GLuint count = 2; count < 10; count++)
    {
        limitedAllocator.reserve(count);
    }

    GLuint finalResult = 0;
    EXPECT_TRUE(limitedAllocator.allocate(&finalResult));
    EXPECT_EQ(finalResult, 1u);
}

// The following test covers reserving a handle with max uint value. See
// http://anglebug.com/42260058
TEST(HandleAllocatorTest, ReserveMaxUintHandle)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    GLuint maxUintHandle = std::numeric_limits<GLuint>::max();
    allocator.reserve(maxUintHandle);

    GLuint normalHandle = 0;
    EXPECT_TRUE(allocator.allocate(&normalHandle));
    EXPECT_EQ(1u, normalHandle);
}

// The following test covers reserving a handle with max uint value minus one then max uint value.
TEST(HandleAllocatorTest, ReserveMaxUintHandle2)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    GLuint maxUintHandle = std::numeric_limits<GLuint>::max();
    allocator.reserve(maxUintHandle - 1);
    allocator.reserve(maxUintHandle);

    GLuint normalHandle = 0;
    EXPECT_TRUE(allocator.allocate(&normalHandle));
    EXPECT_EQ(1u, normalHandle);
}

// To test if the allocator keep the handle in a sorted order.
TEST(HandleAllocatorTest, SortedOrderHandle)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    allocator.reserve(3);

    GLuint allocatedList[5];
    for (GLuint count = 0; count < 5; count++)
    {
        EXPECT_TRUE(allocator.allocate(&allocatedList[count]));
    }

    EXPECT_EQ(1u, allocatedList[0]);
    EXPECT_EQ(2u, allocatedList[1]);
    EXPECT_EQ(4u, allocatedList[2]);
    EXPECT_EQ(5u, allocatedList[3]);
    EXPECT_EQ(6u, allocatedList[4]);
}

// Tests the reset method.
TEST(HandleAllocatorTest, Reset)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    for (int iteration = 0; iteration < 1; ++iteration)
    {
        allocator.reserve(3);
        GLuint handle = 0;
        EXPECT_TRUE(allocator.allocate(&handle));
        EXPECT_EQ(1u, handle);
        EXPECT_TRUE(allocator.allocate(&handle));
        EXPECT_EQ(2u, handle);
        EXPECT_TRUE(allocator.allocate(&handle));
        EXPECT_EQ(4u, handle);
        allocator.reset();
    }
}

// Tests the reset method of custom allocator works as expected.
TEST(HandleAllocatorTest, ResetAndReallocate)
{
    // Allocates handles - [1, 3]
    gl::HandleAllocator allocator(3);
    const std::unordered_set<GLuint> expectedHandles = {1, 2, 3};
    std::unordered_set<GLuint> handles;

    auto allocateHandle = [&]() {
        GLuint handle = 0;
        EXPECT_TRUE(allocator.allocate(&handle));
        handles.insert(handle);
    };
    EXPECT_EQ(allocator.anyHandleAvailableForAllocation(), true);
    allocateHandle();
    allocateHandle();
    allocateHandle();
    EXPECT_EQ(expectedHandles, handles);
    EXPECT_EQ(allocator.anyHandleAvailableForAllocation(), false);

    // Reset the allocator
    allocator.reset();

    EXPECT_EQ(allocator.anyHandleAvailableForAllocation(), true);
    allocateHandle();
    allocateHandle();
    allocateHandle();
    EXPECT_EQ(expectedHandles, handles);
    EXPECT_EQ(allocator.anyHandleAvailableForAllocation(), false);
}

// Covers a particular bug with reserving and allocating sub ranges.
TEST(HandleAllocatorTest, ReserveAndAllocateIterated)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    for (int iteration = 0; iteration < 3; ++iteration)
    {
        allocator.reserve(5);
        allocator.reserve(6);
        GLuint a = 0, b = 0, c = 0;
        EXPECT_TRUE(allocator.allocate(&a));
        EXPECT_TRUE(allocator.allocate(&b));
        EXPECT_TRUE(allocator.allocate(&c));
        allocator.release(c);
        allocator.release(a);
        allocator.release(b);
        allocator.release(5);
        allocator.release(6);
    }
}

// This test reproduces invalid heap bug when reserve resources after release.
TEST(HandleAllocatorTest, ReserveAfterReleaseBug)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    for (int iteration = 1; iteration <= 16; ++iteration)
    {
        EXPECT_TRUE(allocator.allocate(nullptr));
    }

    allocator.release(15);
    allocator.release(16);

    for (int iteration = 1; iteration <= 14; ++iteration)
    {
        allocator.release(iteration);
    }

    allocator.reserve(1);

    EXPECT_TRUE(allocator.allocate(nullptr));
}

// This test is to verify that we consolidate handle ranges when releasing a handle.
TEST(HandleAllocatorTest, ConsolidateRangeDuringRelease)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);

    // Reserve GLuint(-1)
    allocator.reserve(static_cast<GLuint>(-1));
    // Allocate a few others
    EXPECT_TRUE(allocator.allocate(nullptr));
    EXPECT_TRUE(allocator.allocate(nullptr));

    // Release GLuint(-1)
    allocator.release(static_cast<GLuint>(-1));

    // Allocate one more handle.
    // Since we consolidate handle ranges during a release we do not expect to get back a
    // handle value of GLuint(-1).
    GLuint handle = 0;
    EXPECT_TRUE(allocator.allocate(&handle));
    EXPECT_NE(handle, static_cast<GLuint>(-1));
}

// Test that HandleAllocator::allocate returns false when there are no more available handles.
TEST(HandleAllocatorTest, HandleExhaustion)
{
    constexpr size_t kCount = 16;
    gl::HandleAllocator allocator(kCount);

    // Use all available handles
    std::vector<GLuint> handles;
    for (size_t iteration = 0; iteration < kCount; ++iteration)
    {
        GLuint handle;
        EXPECT_TRUE(allocator.allocate(&handle));
        handles.push_back(handle);
    }

    // allocations should fail
    EXPECT_FALSE(allocator.allocate(nullptr));
    EXPECT_FALSE(allocator.anyHandleAvailableForAllocation());

    // Release one handle, the next allocation should succeed
    allocator.release(handles[0]);
    EXPECT_TRUE(allocator.anyHandleAvailableForAllocation());
    EXPECT_TRUE(allocator.allocate(nullptr));

    // The allocator is full again, allocations should fail
    EXPECT_FALSE(allocator.allocate(nullptr));
    EXPECT_FALSE(allocator.anyHandleAvailableForAllocation());
}

// Verifies that released handles are reused in FIFO order.
TEST(HandleAllocatorTest, ReleaseThenReuseInFIFO)
{
    gl::HandleAllocator allocator(1000);

    // Allocate handles 1-11. The next unallocated index is 12.
    GLuint handles[11];
    for (int i = 0; i < 11; i++)
    {
        EXPECT_TRUE(allocator.allocate(&handles[i]));
    }

    // Release handles in a non-sorted order. None is adjacent to 12, so all go into mReleasedList
    // rather than being consolidated into mUnallocatedList.
    constexpr GLuint kReleaseOrder[] = {3, 1, 5, 2, 4, 8, 6, 10, 7, 9};
    for (GLuint i : kReleaseOrder)
    {
        allocator.release(i);
    }

    // Each allocation should return the next handle in the same order they were released.
    for (GLuint expected : kReleaseOrder)
    {
        GLuint handle = 0;
        EXPECT_TRUE(allocator.allocate(&handle));
        EXPECT_EQ(expected, handle);
    }
}

// Verifies that reserve() correctly removes handles from the released list even when the released
// list has many entries.
TEST(HandleAllocatorTest, ReserveFromLargeReleasedList)
{
    gl::HandleAllocator allocator(1000);

    // Need to allocate 101 to push the next unallocated index to 102, so it is no longer adjacent
    // to 1-100.
    GLuint prevHandle;
    for (GLuint i = 1; i <= 101; i++)
    {
        EXPECT_TRUE(allocator.allocate(&prevHandle));
    }

    // Release 1-100. All should go to mReleasedList.
    for (GLuint i = 1; i <= 100; i++)
    {
        allocator.release(i);
    }

    // Reserve some specific handles.
    allocator.reserve(50);
    allocator.reserve(75);
    allocator.reserve(25);

    // Verify those reserved handles are not allocated.
    GLuint handle;
    for (GLuint i = 1; i <= 100; i++)
    {
        if (allocator.allocate(&handle))
        {
            EXPECT_NE(25u, handle);
            EXPECT_NE(50u, handle);
            EXPECT_NE(75u, handle);
        }
    }
}

// Verifies that interleaved release and allocate operations work correctly.
// Simulates handle usage patterns similar to real-world resource management.
TEST(HandleAllocatorTest, InterleavedReleaseAllocate)
{
    gl::HandleAllocator allocator(kMaxHandleForTesting);
    constexpr GLuint kPoolSize = 16;

    // Allocate more handles than the pool to prevent consolidation.
    std::vector<GLuint> pool;
    for (GLuint i = 0; i < kPoolSize; i++)
    {
        GLuint handle;
        EXPECT_TRUE(allocator.allocate(&handle));
        pool.push_back(handle);
    }

    // Keep the next unallocated handle advanced.
    GLuint gapHandle;
    EXPECT_TRUE(allocator.allocate(&gapHandle));

    // Release half of the pool.
    std::vector<GLuint> releasedHandles;
    for (GLuint i = 0; i < kPoolSize / 2; i++)
    {
        allocator.release(pool[i]);
        releasedHandles.push_back(pool[i]);
    }

    // Release more handles.
    std::vector<GLuint> releasedHandles2;
    for (GLuint i = kPoolSize / 2; i < kPoolSize; i++)
    {
        allocator.release(pool[i]);
        releasedHandles2.push_back(pool[i]);
    }

    // Allocate them back. It should return in FIFO order.
    for (GLuint handle : releasedHandles)
    {
        GLuint newHandle;
        EXPECT_TRUE(allocator.allocate(&newHandle));
        EXPECT_EQ(handle, newHandle);
    }

    for (GLuint handle : releasedHandles2)
    {
        GLuint newHandle;
        EXPECT_TRUE(allocator.allocate(&newHandle));
        EXPECT_EQ(handle, newHandle);
    }
}

}  // anonymous namespace
