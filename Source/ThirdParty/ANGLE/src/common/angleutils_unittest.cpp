//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angleutils_unittest.cpp: Unit tests for ANGLE's common utilities.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/angleutils.h"
#include "common/span.h"

#include <array>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace
{

// Test that multiple array indices are written out in the right order.
TEST(ArrayIndexString, MultipleArrayIndices)
{
    std::vector<unsigned int> indices;
    indices.push_back(12);
    indices.push_back(34);
    indices.push_back(56);
    EXPECT_EQ("[56][34][12]", ArrayIndexString(indices));
}

// Test that ArraySize works for C-style arrays and std::array.
TEST(ArraySize, StandardUsage)
{
    int cArray[5] = {0};
    EXPECT_EQ(size_t(5), ArraySize(cArray));

    std::array<int, 10> stdArray = {0};
    EXPECT_EQ(size_t(10), ArraySize(stdArray));
}

// Test that Reversed iterates over a mutable std::vector in reverse order and allows
// mutating elements in-place.
TEST(Reversed, VectorAndMutation)
{
    std::vector<int> v = {3, 2, 1, 10, 5};
    std::vector<int> result;
    for (int &element : angle::Reversed(v))
    {
        result.push_back(element);
        element += 100;
    }
    EXPECT_THAT(result, testing::ElementsAre(5, 10, 1, 2, 3));
    EXPECT_THAT(v, testing::ElementsAre(103, 102, 101, 110, 105));
}

// Test that Reversed works with an empty std::vector.
TEST(Reversed, EmptyVector)
{
    std::vector<int> v;
    std::vector<int> result;
    for (int element : angle::Reversed(v))
    {
        result.push_back(element);
    }
    EXPECT_TRUE(result.empty());
}

// Test that Reversed works with const containers.
TEST(Reversed, ConstContainer)
{
    const std::vector<int> v = {3, 2, 1, 10, 5};
    std::vector<int> result;
    for (int element : angle::Reversed(v))
    {
        result.push_back(element);
    }
    EXPECT_THAT(result, testing::ElementsAre(5, 10, 1, 2, 3));
}

// Test that Reversed works with std::array.
TEST(Reversed, Array)
{
    std::array<int, 5> arr = {3, 2, 1, 10, 5};
    std::vector<int> result;
    for (int element : angle::Reversed(arr))
    {
        result.push_back(element);
    }
    EXPECT_THAT(result, testing::ElementsAre(5, 10, 1, 2, 3));
}

// Test that Reversed works with angle::Span.
TEST(Reversed, Span)
{
    std::array<int, 5> arr = {3, 2, 1, 10, 5};
    angle::Span<int> span(arr);
    std::vector<int> result;
    for (int element : angle::Reversed(span))
    {
        result.push_back(element);
    }
    EXPECT_THAT(result, testing::ElementsAre(5, 10, 1, 2, 3));
}

// Test that Reversed works with an empty angle::Span.
TEST(Reversed, EmptySpan)
{
    angle::Span<int> span;
    std::vector<int> result;
    for (int element : angle::Reversed(span))
    {
        result.push_back(element);
    }
    EXPECT_TRUE(result.empty());
}

// Test that Reversed safely extends the lifetime of temporary rvalue containers.
TEST(Reversed, TemporaryContainer)
{
    std::vector<int> result;
    for (int element : angle::Reversed(std::vector<int>{3, 2, 1, 10, 5}))
    {
        result.push_back(element);
    }
    EXPECT_THAT(result, testing::ElementsAre(5, 10, 1, 2, 3));
}

}  // anonymous namespace
