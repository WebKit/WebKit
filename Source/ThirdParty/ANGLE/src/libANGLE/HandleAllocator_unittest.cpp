//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for HandleAllocator.
//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "libANGLE/HandleAllocator.h"

namespace
{

TEST(HandleAllocatorTest, ReservationsWithGaps)
{
    gl::HandleAllocator allocator;

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
        GLuint handle = allocator.allocate();
        EXPECT_EQ(0u, allocationList.count(handle));
        EXPECT_EQ(0u, allocatedList.count(handle));
        allocatedList.insert(handle);
    }
}

TEST(HandleAllocatorTest, Random)
{
    gl::HandleAllocator allocator;

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
            GLuint normalHandle = allocator.allocate();
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
        GLuint result = limitedAllocator.allocate();
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

    GLint finalResult = limitedAllocator.allocate();
    EXPECT_EQ(finalResult, 1);
}

// The following test covers reserving a handle with max uint value. See http://anglebug.com/1052
TEST(HandleAllocatorTest, ReserveMaxUintHandle)
{
    gl::HandleAllocator allocator;

    GLuint maxUintHandle = std::numeric_limits<GLuint>::max();
    allocator.reserve(maxUintHandle);

    GLuint normalHandle = allocator.allocate();
    EXPECT_EQ(1u, normalHandle);
}

// The following test covers reserving a handle with max uint value minus one then max uint value.
TEST(HandleAllocatorTest, ReserveMaxUintHandle2)
{
    gl::HandleAllocator allocator;

    GLuint maxUintHandle = std::numeric_limits<GLuint>::max();
    allocator.reserve(maxUintHandle - 1);
    allocator.reserve(maxUintHandle);

    GLuint normalHandle = allocator.allocate();
    EXPECT_EQ(1u, normalHandle);
}

// To test if the allocator keep the handle in a sorted order.
TEST(HandleAllocatorTest, SortedOrderHandle)
{
    gl::HandleAllocator allocator;

    allocator.reserve(3);

    GLuint allocatedList[5];
    for (GLuint count = 0; count < 5; count++)
    {
        allocatedList[count] = allocator.allocate();
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
    gl::HandleAllocator allocator;

    for (int iteration = 0; iteration < 1; ++iteration)
    {
        allocator.reserve(3);
        EXPECT_EQ(1u, allocator.allocate());
        EXPECT_EQ(2u, allocator.allocate());
        EXPECT_EQ(4u, allocator.allocate());
        allocator.reset();
    }
}

// Covers a particular bug with reserving and allocating sub ranges.
TEST(HandleAllocatorTest, ReserveAndAllocateIterated)
{
    gl::HandleAllocator allocator;

    for (int iteration = 0; iteration < 3; ++iteration)
    {
        allocator.reserve(5);
        allocator.reserve(6);
        GLuint a = allocator.allocate();
        GLuint b = allocator.allocate();
        GLuint c = allocator.allocate();
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
    gl::HandleAllocator allocator;

    for (int iteration = 1; iteration <= 16; ++iteration)
    {
        allocator.allocate();
    }

    allocator.release(15);
    allocator.release(16);

    for (int iteration = 1; iteration <= 14; ++iteration)
    {
        allocator.release(iteration);
    }

    allocator.reserve(1);

    allocator.allocate();
}

}  // anonymous namespace
