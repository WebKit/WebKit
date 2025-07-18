//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PoolAlloc_unittest:
//   Tests of the PoolAlloc class
//

#include <gtest/gtest.h>

#include "common/PoolAlloc.h"

namespace angle
{

class PoolAllocatorTest : public testing::TestWithParam<size_t>
{
  protected:
    static constexpr size_t kPoolAllocatorPageSize = 16 * 1024;
    static size_t GetAlignment() { return GetParam(); }
};

// Verify the public interface of PoolAllocator class
TEST_P(PoolAllocatorTest, Interface)
{
    size_t numBytes               = 1024;
    constexpr uint32_t kTestValue = 0xbaadbeef;
    // Create a default pool allocator and allocate from it
    PoolAllocator poolAllocator{GetAlignment()};
    void *allocation = poolAllocator.allocate(numBytes);
    // Verify non-zero ptr returned
    EXPECT_NE(nullptr, allocation);
    // Write to allocation to check later
    uint32_t *writePtr = static_cast<uint32_t *>(allocation);
    *writePtr          = kTestValue;
    // Test other allocator creating a new allocation
    {
        PoolAllocator poolAllocator2{GetAlignment()};
        allocation = poolAllocator2.allocate(numBytes);
        EXPECT_NE(nullptr, allocation);
        // Make an allocation that spans multiple pages
        allocation = poolAllocator2.allocate(10 * 1024);
        // Free previous two allocations.
    }
    // Verify first allocation still has data
    EXPECT_EQ(kTestValue, *writePtr);
    // Make a bunch of allocations
    for (uint32_t j = 0; j < 100; ++j)
    {
        for (uint32_t i = 0; i < 1000; ++i)
        {
            numBytes   = (rand() % kPoolAllocatorPageSize) + 1;
            allocation = poolAllocator.allocate(numBytes);
            EXPECT_NE(nullptr, allocation);
            // Write data into full allocation. In debug case if we
            //  overwrite any other allocation we get error.
            memset(allocation, 0xb8, numBytes);
        }
        poolAllocator.reset();
    }
}

// Tests that PoolAllocator returns pointers with expected alignment.
TEST_P(PoolAllocatorTest, Alignment)
{
    PoolAllocator poolAllocator{GetAlignment()};
    for (uint32_t j = 0; j < 10; ++j)
    {
        for (uint32_t i = 0; i < 100; ++i)
        {
            // Vary the allocation size around 16k to hit some multi-page allocations.
            const size_t numBytes = (rand() % kPoolAllocatorPageSize) + 1;
            void *allocation      = poolAllocator.allocate(numBytes);
            // Verify alignment of allocation matches expected default
            EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation) % GetAlignment())
                << "Iteration " << j << ", " << i << " allocating " << numBytes
                << " got: " << allocation;
            memset(allocation, i, numBytes);
        }
        poolAllocator.reset();
    }
}

#if !defined(ANGLE_DISABLE_POOL_ALLOC)

// Test that reset recycles memory.
TEST_P(PoolAllocatorTest, ResetRecyclesMemory)
{
    PoolAllocator poolAllocator{GetAlignment()};
    void *allocation1 = poolAllocator.allocate(1);
    void *allocation2 = poolAllocator.allocate(2);
    memset(allocation1, 11, 1);
    memset(allocation2, 12, 2);
    poolAllocator.reset();
    void *allocation3 = poolAllocator.allocate(1);
    void *allocation4 = poolAllocator.allocate(2);
    memset(allocation3, 21, 1);
    memset(allocation4, 22, 2);
    EXPECT_NE(allocation1, nullptr);
    EXPECT_NE(allocation2, nullptr);
    EXPECT_NE(allocation1, allocation2);
    EXPECT_EQ(allocation1, allocation3);
    EXPECT_EQ(allocation2, allocation4);
}

#endif

INSTANTIATE_TEST_SUITE_P(,
                         PoolAllocatorTest,
                         testing::Values(1, 2, 4, 8, 16, 32, 64, 128),
                         testing::PrintToStringParamName());

#if !defined(ANGLE_DISABLE_POOL_ALLOC) && defined(ANGLE_POOL_ALLOC_GUARD_BLOCKS)

class PoolAllocatorGuardTest : public PoolAllocatorTest
{};

// Verify that alignment guard detects overflowing write.
TEST_P(PoolAllocatorGuardTest, AlignmentGuardDetectsOverflowWrite)
{
    auto testOverflowAlignment = []() {
        PoolAllocator poolAllocator{GetAlignment()};
        void *allocation = poolAllocator.allocate(15);
        memset(allocation, 11, 16);
    };
    ASSERT_DEATH(testOverflowAlignment(), "");
}

// Verify that allocation guard detects overflowing write.
TEST_P(PoolAllocatorGuardTest, AllocationGuardsDetectsOverflowWrite)
{
    auto testOverflow = []() {
        PoolAllocator poolAllocator{GetAlignment()};
        void *allocation1 = poolAllocator.allocate(16);
        memset(allocation1, 11, 17);
    };
    ASSERT_DEATH(testOverflow(), "");
}

// Verify that allocation guard detects underflowing write.
TEST_P(PoolAllocatorGuardTest, AllocationGuardsDetectsUnderflowWrite)
{
    auto testUnderflow = []() {
        PoolAllocator poolAllocator{GetAlignment()};
        void *allocation1 = poolAllocator.allocate(16);
        memset(reinterpret_cast<uint8_t *>(allocation1) - 1, 11, 1);
    };
    ASSERT_DEATH(testUnderflow(), "");
}

INSTANTIATE_TEST_SUITE_P(,
                         PoolAllocatorGuardTest,
                         testing::Values(2, 4, 8, 16, 32, 64, 128),
                         testing::PrintToStringParamName());

#endif

}  // namespace angle
