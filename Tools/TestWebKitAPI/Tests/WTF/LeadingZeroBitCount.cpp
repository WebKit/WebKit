// Taken from LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See Source/WTF/LICENSE-LLVM.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Based on:
// https://github.com/llvm/llvm-project/blob/d480f968/flang/unittests/Evaluate/leading-zero-bit-count.cpp

#include "config.h"
#include <wtf/LeadingZeroBitCount.h>

#include <limits>

template <typename IntType>
static bool countWithTwoShiftedBits(int j, int k)
{
    IntType x = (IntType {1} << j) | (IntType {1} << k);
    return leadingZeroBitCount(x) == std::numeric_limits<IntType>::digits - 1 - j;
}

TEST(WTF_LeadingZeroBitCount, Basic)
{
    EXPECT_EQ(leadingZeroBitCount(std::uint64_t {0}), 64);
    for (int j {0}; j < 64; ++j) {
        for (int k {0}; k < j; ++k)
            EXPECT_PRED2(countWithTwoShiftedBits<std::uint64_t>, j, k);
    }

    EXPECT_EQ(leadingZeroBitCount(std::uint32_t {0}), 32);
    for (int j {0}; j < 32; ++j) {
        for (int k {0}; k < j; ++k)
            EXPECT_PRED2(countWithTwoShiftedBits<std::uint32_t>, j, k);
    }

    EXPECT_EQ(leadingZeroBitCount(std::uint16_t {0}), 16);
    for (int j {0}; j < 16; ++j) {
        for (int k {0}; k < j; ++k)
            EXPECT_PRED2(countWithTwoShiftedBits<std::uint16_t>, j, k);
    }

    EXPECT_EQ(leadingZeroBitCount(std::uint8_t {0}), 8);
    for (int j {0}; j < 8; ++j) {
        for (int k {0}; k < j; ++k)
            EXPECT_PRED2(countWithTwoShiftedBits<std::uint8_t>, j, k);
    }
}
