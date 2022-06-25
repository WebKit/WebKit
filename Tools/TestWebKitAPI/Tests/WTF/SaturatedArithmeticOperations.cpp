/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <limits.h>
#include <wtf/SaturatedArithmetic.h>

namespace TestWebKitAPI {

TEST(WTF, SaturatedArithmeticAddition)
{
    EXPECT_EQ(0, saturatedSum<int32_t>(0, 0));
    EXPECT_EQ(1, saturatedSum<int32_t>(0, 1));
    EXPECT_EQ(100, saturatedSum<int32_t>(0, 100));
    EXPECT_EQ(150, saturatedSum<int32_t>(100, 50));

    EXPECT_EQ(-1, saturatedSum<int32_t>(0, -1));
    EXPECT_EQ(0, saturatedSum<int32_t>(1, -1));
    EXPECT_EQ(50, saturatedSum<int32_t>(100, -50));
    EXPECT_EQ(-50, saturatedSum<int32_t>(50, -100));

    EXPECT_EQ(INT_MAX - 1, saturatedSum<int32_t>(INT_MAX - 1, 0));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(INT_MAX - 1, 1));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(INT_MAX - 1, 2));
    EXPECT_EQ(INT_MAX - 1, saturatedSum<int32_t>(0, INT_MAX - 1));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(1, INT_MAX - 1));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(2, INT_MAX - 1));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(INT_MAX - 1, INT_MAX - 1));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(INT_MAX, INT_MAX));

    EXPECT_EQ(INT_MIN, saturatedSum<int32_t>(INT_MIN, 0));
    EXPECT_EQ(INT_MIN + 1, saturatedSum<int32_t>(INT_MIN + 1, 0));
    EXPECT_EQ(INT_MIN + 2, saturatedSum<int32_t>(INT_MIN + 1, 1));
    EXPECT_EQ(INT_MIN + 3, saturatedSum<int32_t>(INT_MIN + 1, 2));
    EXPECT_EQ(INT_MIN, saturatedSum<int32_t>(INT_MIN + 1, -1));
    EXPECT_EQ(INT_MIN, saturatedSum<int32_t>(INT_MIN + 1, -2));
    EXPECT_EQ(INT_MIN + 1, saturatedSum<int32_t>(0, INT_MIN + 1));
    EXPECT_EQ(INT_MIN, saturatedSum<int32_t>(-1, INT_MIN + 1));
    EXPECT_EQ(INT_MIN, saturatedSum<int32_t>(-2, INT_MIN + 1));

    EXPECT_EQ(INT_MAX / 2 + 10000, saturatedSum<int32_t>(INT_MAX / 2, 10000));
    EXPECT_EQ(INT_MAX, saturatedSum<int32_t>(INT_MAX / 2 + 1, INT_MAX / 2 + 1));
    EXPECT_EQ(-1, saturatedSum<int32_t>(INT_MIN, INT_MAX));

    EXPECT_EQ(0U, saturatedSum<uint32_t>(0U, 0U));
    EXPECT_EQ(1U, saturatedSum<uint32_t>(0U, 1U));
    EXPECT_EQ(100U, saturatedSum<uint32_t>(0U, 100U));
    EXPECT_EQ(150U, saturatedSum<uint32_t>(100U, 50U));

    EXPECT_EQ(UINT_MAX - 1, saturatedSum<uint32_t>(UINT_MAX - 1U, 0U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX - 1U, 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX - 1U, 2U));
    EXPECT_EQ(UINT_MAX - 1, saturatedSum<uint32_t>(0U, UINT_MAX - 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(1U, UINT_MAX - 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(2U, UINT_MAX - 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX - 1U, UINT_MAX - 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX, UINT_MAX));

    EXPECT_EQ(UINT_MAX / 2 + 10000, saturatedSum<uint32_t>(UINT_MAX / 2U, 10000U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX / 2U, UINT_MAX / 2U + 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX / 2U + 1U, UINT_MAX / 2U + 1U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX / 3U + 1U, UINT_MAX / 3U, UINT_MAX / 3U));
    EXPECT_EQ(UINT_MAX, saturatedSum<uint32_t>(UINT_MAX / 3U + 1U, UINT_MAX / 3U + 1U, UINT_MAX / 3U + 1U));
}

TEST(WTF, SaturatedArithmeticSubtraction)
{
    EXPECT_EQ(0, saturatedDifference<int32_t>(0, 0));
    EXPECT_EQ(-1, saturatedDifference<int32_t>(0, 1));
    EXPECT_EQ(-100, saturatedDifference<int32_t>(0, 100));
    EXPECT_EQ(50, saturatedDifference<int32_t>(100, 50));
    
    EXPECT_EQ(1, saturatedDifference<int32_t>(0, -1));
    EXPECT_EQ(2, saturatedDifference<int32_t>(1, -1));
    EXPECT_EQ(150, saturatedDifference<int32_t>(100, -50));
    EXPECT_EQ(150, saturatedDifference<int32_t>(50, -100));

    EXPECT_EQ(INT_MAX, saturatedDifference<int32_t>(INT_MAX, 0));
    EXPECT_EQ(INT_MAX - 1, saturatedDifference<int32_t>(INT_MAX, 1));
    EXPECT_EQ(INT_MAX - 1, saturatedDifference<int32_t>(INT_MAX - 1, 0));
    EXPECT_EQ(INT_MAX, saturatedDifference<int32_t>(INT_MAX - 1, -1));
    EXPECT_EQ(INT_MAX, saturatedDifference<int32_t>(INT_MAX - 1, -2));
    EXPECT_EQ(-INT_MAX + 1, saturatedDifference<int32_t>(0, INT_MAX - 1));
    EXPECT_EQ(-INT_MAX, saturatedDifference<int32_t>(-1, INT_MAX - 1));
    EXPECT_EQ(-INT_MAX - 1, saturatedDifference<int32_t>(-2, INT_MAX - 1));
    EXPECT_EQ(-INT_MAX - 1, saturatedDifference<int32_t>(-3, INT_MAX - 1));

    EXPECT_EQ(INT_MIN, saturatedDifference<int32_t>(INT_MIN, 0));
    EXPECT_EQ(INT_MIN + 1, saturatedDifference<int32_t>(INT_MIN + 1, 0));
    EXPECT_EQ(INT_MIN, saturatedDifference<int32_t>(INT_MIN + 1, 1));
    EXPECT_EQ(INT_MIN, saturatedDifference<int32_t>(INT_MIN + 1, 2));

    EXPECT_EQ(0, saturatedDifference<int32_t>(INT_MIN, INT_MIN));
    EXPECT_EQ(0, saturatedDifference<int32_t>(INT_MAX, INT_MAX));
    EXPECT_EQ(INT_MAX, saturatedDifference<int32_t>(INT_MAX, INT_MIN));
}

} // namespace TestWebKitAPI
