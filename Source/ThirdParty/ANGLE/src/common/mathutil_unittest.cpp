//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mathutil_unittest:
//   Unit tests for the utils defined in mathutil.h
//

#include "mathutil.h"

#include <gtest/gtest.h>

using namespace gl;

namespace
{

// Test the correctness of packSnorm2x16 and unpackSnorm2x16 functions.
// For floats f1 and f2, unpackSnorm2x16(packSnorm2x16(f1, f2)) should be same as f1 and f2.
TEST(MathUtilTest, packAndUnpackSnorm2x16)
{
    const float input[8][2] =
    {
        { 0.0f, 0.0f },
        { 1.0f, 1.0f },
        { -1.0f, 1.0f },
        { -1.0f, -1.0f },
        { 0.875f, 0.75f },
        { 0.00392f, -0.99215f },
        { -0.000675f, 0.004954f },
        { -0.6937f, -0.02146f }
    };
    const float floatFaultTolerance = 0.0001f;
    float outputVal1, outputVal2;

    for (size_t i = 0; i < 8; i++)
    {
        unpackSnorm2x16(packSnorm2x16(input[i][0], input[i][1]), &outputVal1, &outputVal2);
        EXPECT_NEAR(input[i][0], outputVal1, floatFaultTolerance);
        EXPECT_NEAR(input[i][1], outputVal2, floatFaultTolerance);
    }
}

// Test the correctness of packSnorm2x16 and unpackSnorm2x16 functions with infinity values,
// result should be clamped to [-1, 1].
TEST(MathUtilTest, packAndUnpackSnorm2x16Infinity)
{
    const float floatFaultTolerance = 0.0001f;
    float outputVal1, outputVal2;

    unpackSnorm2x16(packSnorm2x16(std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(1.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(1.0f, outputVal2, floatFaultTolerance);

    unpackSnorm2x16(packSnorm2x16(std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(1.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(-1.0f, outputVal2, floatFaultTolerance);

    unpackSnorm2x16(packSnorm2x16(-std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(-1.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(-1.0f, outputVal2, floatFaultTolerance);
}

// Test the correctness of packUnorm2x16 and unpackUnorm2x16 functions.
// For floats f1 and f2, unpackUnorm2x16(packUnorm2x16(f1, f2)) should be same as f1 and f2.
TEST(MathUtilTest, packAndUnpackUnorm2x16)
{
    const float input[8][2] =
    {
        { 0.0f, 0.0f },
        { 1.0f, 1.0f },
        { -1.0f, 1.0f },
        { -1.0f, -1.0f },
        { 0.875f, 0.75f },
        { 0.00392f, -0.99215f },
        { -0.000675f, 0.004954f },
        { -0.6937f, -0.02146f }
    };
    const float floatFaultTolerance = 0.0001f;
    float outputVal1, outputVal2;

    for (size_t i = 0; i < 8; i++)
    {
        unpackUnorm2x16(packUnorm2x16(input[i][0], input[i][1]), &outputVal1, &outputVal2);
        float expected = input[i][0] < 0.0f ? 0.0f : input[i][0];
        EXPECT_NEAR(expected, outputVal1, floatFaultTolerance);
        expected = input[i][1] < 0.0f ? 0.0f : input[i][1];
        EXPECT_NEAR(expected, outputVal2, floatFaultTolerance);
    }
}

// Test the correctness of packUnorm2x16 and unpackUnorm2x16 functions with infinity values,
// result should be clamped to [0, 1].
TEST(MathUtilTest, packAndUnpackUnorm2x16Infinity)
{
    const float floatFaultTolerance = 0.0001f;
    float outputVal1, outputVal2;

    unpackUnorm2x16(packUnorm2x16(std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(1.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(1.0f, outputVal2, floatFaultTolerance);

    unpackUnorm2x16(packUnorm2x16(std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(1.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(0.0f, outputVal2, floatFaultTolerance);

    unpackUnorm2x16(packUnorm2x16(-std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()), &outputVal1, &outputVal2);
    EXPECT_NEAR(0.0f, outputVal1, floatFaultTolerance);
    EXPECT_NEAR(0.0f, outputVal2, floatFaultTolerance);
}

// Test the correctness of packHalf2x16 and unpackHalf2x16 functions.
// For floats f1 and f2, unpackHalf2x16(packHalf2x16(f1, f2)) should be same as f1 and f2.
TEST(MathUtilTest, packAndUnpackHalf2x16)
{
    const float input[8][2] =
    {
        { 0.0f, 0.0f },
        { 1.0f, 1.0f },
        { -1.0f, 1.0f },
        { -1.0f, -1.0f },
        { 0.875f, 0.75f },
        { 0.00392f, -0.99215f },
        { -0.000675f, 0.004954f },
        { -0.6937f, -0.02146f },
    };
    const float floatFaultTolerance = 0.0005f;
    float outputVal1, outputVal2;

    for (size_t i = 0; i < 8; i++)
    {
        unpackHalf2x16(packHalf2x16(input[i][0], input[i][1]), &outputVal1, &outputVal2);
        EXPECT_NEAR(input[i][0], outputVal1, floatFaultTolerance);
        EXPECT_NEAR(input[i][1], outputVal2, floatFaultTolerance);
    }
}

// Test the correctness of gl::isNaN function.
TEST(MathUtilTest, isNaN)
{
    EXPECT_TRUE(isNaN(bitCast<float>(0xffu << 23 | 1u)));
    EXPECT_TRUE(isNaN(bitCast<float>(1u << 31 | 0xffu << 23 | 1u)));
    EXPECT_TRUE(isNaN(bitCast<float>(1u << 31 | 0xffu << 23 | 0x400000u)));
    EXPECT_TRUE(isNaN(bitCast<float>(1u << 31 | 0xffu << 23 | 0x7fffffu)));
    EXPECT_FALSE(isNaN(0.0f));
    EXPECT_FALSE(isNaN(bitCast<float>(1u << 31 | 0xffu << 23)));
    EXPECT_FALSE(isNaN(bitCast<float>(0xffu << 23)));
}

// Test the correctness of gl::isInf function.
TEST(MathUtilTest, isInf)
{
    EXPECT_TRUE(isInf(bitCast<float>(0xffu << 23)));
    EXPECT_TRUE(isInf(bitCast<float>(1u << 31 | 0xffu << 23)));
    EXPECT_FALSE(isInf(0.0f));
    EXPECT_FALSE(isInf(bitCast<float>(0xffu << 23 | 1u)));
    EXPECT_FALSE(isInf(bitCast<float>(1u << 31 | 0xffu << 23 | 1u)));
    EXPECT_FALSE(isInf(bitCast<float>(1u << 31 | 0xffu << 23 | 0x400000u)));
    EXPECT_FALSE(isInf(bitCast<float>(1u << 31 | 0xffu << 23 | 0x7fffffu)));
    EXPECT_FALSE(isInf(bitCast<float>(0xfeu << 23 | 0x7fffffu)));
    EXPECT_FALSE(isInf(bitCast<float>(1u << 31 | 0xfeu << 23 | 0x7fffffu)));
}

TEST(MathUtilTest, CountLeadingZeros)
{
    for (unsigned int i = 0; i < 32u; ++i)
    {
        uint32_t iLeadingZeros = 1u << (31u - i);
        EXPECT_EQ(i, CountLeadingZeros(iLeadingZeros));
    }
    EXPECT_EQ(32u, CountLeadingZeros(0));
}

// Some basic tests. Tests that rounding up zero produces zero.
TEST(MathUtilTest, BasicRoundUp)
{
    EXPECT_EQ(0u, rx::roundUp(0u, 4u));
    EXPECT_EQ(4u, rx::roundUp(1u, 4u));
    EXPECT_EQ(4u, rx::roundUp(4u, 4u));
}

// Test that rounding up zero produces zero for checked ints.
TEST(MathUtilTest, CheckedRoundUpZero)
{
    auto checkedValue = rx::CheckedRoundUp(0u, 4u);
    ASSERT_TRUE(checkedValue.IsValid());
    ASSERT_EQ(0u, checkedValue.ValueOrDie());
}

// Test out-of-bounds with CheckedRoundUp
TEST(MathUtilTest, CheckedRoundUpInvalid)
{
    // The answer to this query is out of bounds.
    auto limit        = std::numeric_limits<unsigned int>::max();
    auto checkedValue = rx::CheckedRoundUp(limit, limit - 1);
    ASSERT_FALSE(checkedValue.IsValid());

    // Our implementation can't handle this query, despite the parameters being in range.
    auto checkedLimit = rx::CheckedRoundUp(limit - 1, limit);
    ASSERT_FALSE(checkedLimit.IsValid());
}

}  // anonymous namespace
