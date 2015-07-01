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

TEST(ParseUniformName, ArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseUniformName("foo[123]", &index));
    EXPECT_EQ(123u, index);

    EXPECT_EQ("bar", gl::ParseUniformName("bar[0]", &index));
    EXPECT_EQ(0u, index);
}

TEST(ParseUniformName, NegativeArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseUniformName("foo[-1]", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

TEST(ParseUniformName, NoArrayIndex)
{
    size_t index;
    EXPECT_EQ("foo", gl::ParseUniformName("foo", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

TEST(ParseUniformName, NULLArrayIndex)
{
    EXPECT_EQ("foo", gl::ParseUniformName("foo[10]", nullptr));
}

TEST(ParseUniformName, TrailingWhitespace)
{
    size_t index;
    EXPECT_EQ("foo ", gl::ParseUniformName("foo ", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);

    EXPECT_EQ("foo[10] ", gl::ParseUniformName("foo[10] ", &index));
    EXPECT_EQ(GL_INVALID_INDEX, index);
}

}
