//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FixedVector_unittest:
//   Tests of the FastVector class
//

#include <gtest/gtest.h>

#include "common/FastVector.h"

namespace angle
{
// Make sure the various constructors compile and do basic checks
TEST(FastVector, Constructors)
{
    FastVector<int, 5> defaultContructor;
    EXPECT_EQ(0u, defaultContructor.size());

    FastVector<int, 5> count(3);
    EXPECT_EQ(3u, count.size());

    FastVector<int, 5> countAndValue(3, 2);
    EXPECT_EQ(3u, countAndValue.size());
    EXPECT_EQ(2, countAndValue[1]);

    FastVector<int, 5> copy(countAndValue);
    EXPECT_EQ(copy, countAndValue);

    FastVector<int, 5> copyRValue(std::move(count));
    EXPECT_EQ(3u, copyRValue.size());

    FastVector<int, 5> initializerList{1, 2, 3, 4, 5};
    EXPECT_EQ(5u, initializerList.size());
    EXPECT_EQ(3, initializerList[2]);

    FastVector<int, 5> assignCopy(copyRValue);
    EXPECT_EQ(3u, assignCopy.size());

    FastVector<int, 5> assignRValue(std::move(assignCopy));
    EXPECT_EQ(3u, assignRValue.size());

    FastVector<int, 5> assignmentInitializerList = {1, 2, 3, 4, 5};
    EXPECT_EQ(5u, assignmentInitializerList.size());
    EXPECT_EQ(3, assignmentInitializerList[2]);
}

// Test indexing operations (at, operator[])
TEST(FastVector, Indexing)
{
    FastVector<int, 5> vec = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(i, vec.at(i));
        EXPECT_EQ(vec[i], vec.at(i));
    }
}

// Test the push_back functions
TEST(FastVector, PushBack)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    EXPECT_EQ(1, vec[0]);
    vec.push_back(1);
    vec.push_back(1);
    vec.push_back(1);
    vec.push_back(1);
    EXPECT_EQ(5u, vec.size());
}

// Tests growing the fast vector beyond the fixed storage.
TEST(FastVector, Growth)
{
    constexpr size_t kSize = 4;
    FastVector<size_t, kSize> vec;

    for (size_t i = 0; i < kSize * 2; ++i)
    {
        vec.push_back(i);
    }

    EXPECT_EQ(kSize * 2, vec.size());

    for (size_t i = kSize * 2; i > 0; --i)
    {
        ASSERT_EQ(vec.back(), i - 1);
        vec.pop_back();
    }

    EXPECT_EQ(0u, vec.size());
}

// Test the pop_back function
TEST(FastVector, PopBack)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    EXPECT_EQ(1, (int)vec.size());
    vec.pop_back();
    EXPECT_EQ(0, (int)vec.size());
}

// Test the back function
TEST(FastVector, Back)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    EXPECT_EQ(2, vec.back());
}

// Test the back function
TEST(FastVector, Front)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    EXPECT_EQ(1, vec.front());
}

// Test the sizing operations
TEST(FastVector, Size)
{
    FastVector<int, 5> vec;
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(0u, vec.size());

    vec.push_back(1);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(1u, vec.size());
}

// Test clearing the vector
TEST(FastVector, Clear)
{
    FastVector<int, 5> vec = {0, 1, 2, 3, 4};
    vec.clear();
    EXPECT_TRUE(vec.empty());
}

// Test clearing the vector larger than the fixed size.
TEST(FastVector, ClearWithLargerThanFixedSize)
{
    FastVector<int, 3> vec = {0, 1, 2, 3, 4};
    vec.clear();
    EXPECT_TRUE(vec.empty());
}

// Test resizing the vector
TEST(FastVector, Resize)
{
    FastVector<int, 5> vec;
    vec.resize(5u, 1);
    EXPECT_EQ(5u, vec.size());
    for (int i : vec)
    {
        EXPECT_EQ(1, i);
    }

    vec.resize(2u);
    EXPECT_EQ(2u, vec.size());
    for (int i : vec)
    {
        EXPECT_EQ(1, i);
    }

    // Resize to larger than minimum
    vec.resize(10u, 2);
    EXPECT_EQ(10u, vec.size());

    for (size_t index = 0; index < 2u; ++index)
    {
        EXPECT_EQ(1, vec[index]);
    }
    for (size_t index = 2u; index < 10u; ++index)
    {
        EXPECT_EQ(2, vec[index]);
    }

    // Resize back to smaller
    vec.resize(2u, 2);
    EXPECT_EQ(2u, vec.size());
}

// Test iterating over the vector
TEST(FastVector, Iteration)
{
    FastVector<int, 5> vec = {0, 1, 2, 3};

    int vistedCount = 0;
    for (int value : vec)
    {
        EXPECT_EQ(vistedCount, value);
        vistedCount++;
    }
    EXPECT_EQ(4, vistedCount);
}

// Tests that equality comparisons work even if reserved size differs.
TEST(FastVector, EqualityWithDifferentReservedSizes)
{
    FastVector<int, 3> vec1 = {1, 2, 3, 4, 5};
    FastVector<int, 5> vec2 = {1, 2, 3, 4, 5};
    EXPECT_EQ(vec1, vec2);
    vec2.push_back(6);
    EXPECT_NE(vec1, vec2);
}

// Tests vector operations with a non copyable type.
TEST(FastVector, NonCopyable)
{
    struct s : angle::NonCopyable
    {
        s() : x(0) {}
        s(int xin) : x(xin) {}
        s(s &&other) : x(other.x) {}
        s &operator=(s &&other)
        {
            x = other.x;
            return *this;
        }
        int x;
    };

    FastVector<s, 3> vec;
    vec.push_back(3);
    EXPECT_EQ(3, vec[0].x);

    FastVector<s, 3> copy = std::move(vec);
    EXPECT_EQ(1u, copy.size());
    EXPECT_EQ(3, copy[0].x);
}
}  // namespace angle
