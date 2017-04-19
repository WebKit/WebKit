//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// utilities_unittest.cpp: Unit tests for ANGLE's GL utility functions

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/utilities.h"

namespace
{

TEST(ParseResourceName, ArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseResourceName("foo[123]", &index));
    EXPECT_EQ(123u, index);

    EXPECT_EQ("bar", gl::ParseResourceName("bar[0]", &index));
    EXPECT_EQ(0u, index);
}

TEST(ParseResourceName, NegativeArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseResourceName("foo[-1]", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

TEST(ParseResourceName, NoArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseResourceName("foo", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

TEST(ParseResourceName, NULLArrayIndex)
{
    EXPECT_EQ("foo", gl::ParseResourceName("foo[10]", nullptr));
}

TEST(ParseResourceName, TrailingWhitespace)
{
    size_t index;
    EXPECT_EQ("foo ", gl::ParseResourceName("foo ", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);

    EXPECT_EQ("foo[10] ", gl::ParseResourceName("foo[10] ", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

}
