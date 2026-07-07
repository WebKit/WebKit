//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angleutils_unittest.cpp: Unit tests for ANGLE's common utilities.

#include "gtest/gtest.h"

#include "common/angleutils.h"

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

}  // anonymous namespace
