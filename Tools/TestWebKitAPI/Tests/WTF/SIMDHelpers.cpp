/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include <wtf/SIMDHelpers.h>

#include <array>

namespace TestWebKitAPI {

template<typename LaneType, size_t laneCount>
static std::array<LaneType, laneCount> extractLanes(WTF::SIMD::VectorType<LaneType> vector)
{
    std::array<LaneType, laneCount> lanes { };
    WTF::SIMD::store(vector, lanes.data());
    return lanes;
}

TEST(WTF_SIMDHelpers, LessThanUnsigned8)
{
    std::array<uint8_t, 16> left { 0, 1, 127, 128, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    std::array<uint8_t, 16> right { 1, 0, 128, 127, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    auto result = extractLanes<uint8_t, 16>(WTF::SIMD::lessThan(WTF::SIMD::load(left.data()), WTF::SIMD::load(right.data())));
    EXPECT_EQ(0xffu, result[0]);
    EXPECT_EQ(0x00u, result[1]);
    EXPECT_EQ(0xffu, result[2]);
    EXPECT_EQ(0x00u, result[3]);
    EXPECT_EQ(0x00u, result[4]);
}

TEST(WTF_SIMDHelpers, LessThanUnsigned16)
{
    std::array<uint16_t, 8> left { 0, 1, 0x7fff, 0x8000, 0xffff, 0, 0, 0 };
    std::array<uint16_t, 8> right { 1, 0, 0x8000, 0x7fff, 0xffff, 0, 0, 0 };
    auto result = extractLanes<uint16_t, 8>(WTF::SIMD::lessThan(WTF::SIMD::load(left.data()), WTF::SIMD::load(right.data())));
    EXPECT_EQ(0xffffu, result[0]);
    EXPECT_EQ(0x0000u, result[1]);
    EXPECT_EQ(0xffffu, result[2]);
    EXPECT_EQ(0x0000u, result[3]);
    EXPECT_EQ(0x0000u, result[4]);
}

TEST(WTF_SIMDHelpers, LessThanUnsigned32)
{
    std::array<uint32_t, 4> left { 0, 0x7fffffffu, 0x80000000u, 0xffffffffu };
    std::array<uint32_t, 4> right { 1, 0x80000000u, 0x7fffffffu, 0xffffffffu };
    auto result = extractLanes<uint32_t, 4>(WTF::SIMD::lessThan(WTF::SIMD::load(left.data()), WTF::SIMD::load(right.data())));
    EXPECT_EQ(0xffffffffu, result[0]);
    EXPECT_EQ(0xffffffffu, result[1]);
    EXPECT_EQ(0x00000000u, result[2]);
    EXPECT_EQ(0x00000000u, result[3]);
}

// The unsigned 64-bit compare has no single x86 instruction behind it, so simde emulates it. These
// are the cases that distinguish it from a signed compare.
TEST(WTF_SIMDHelpers, LessThanUnsigned64)
{
    std::array<uint64_t, 2> left { 0x7fffffffffffffffull, 0x8000000000000000ull };
    std::array<uint64_t, 2> right { 0x8000000000000000ull, 0x7fffffffffffffffull };
    auto result = extractLanes<uint64_t, 2>(WTF::SIMD::lessThan(WTF::SIMD::load(left.data()), WTF::SIMD::load(right.data())));
    EXPECT_EQ(0xffffffffffffffffull, result[0]);
    EXPECT_EQ(0x0000000000000000ull, result[1]);
}

TEST(WTF_SIMDHelpers, Bitwise32)
{
    std::array<uint32_t, 4> left { 0xf0f0f0f0u, 0xffffffffu, 0x00000000u, 0x12345678u };
    std::array<uint32_t, 4> right { 0x0ff00ff0u, 0x00000000u, 0xffffffffu, 0x87654321u };
    auto leftVector = WTF::SIMD::load(left.data());
    auto rightVector = WTF::SIMD::load(right.data());

    auto andResult = extractLanes<uint32_t, 4>(WTF::SIMD::bitAnd(leftVector, rightVector));
    auto orResult = extractLanes<uint32_t, 4>(WTF::SIMD::bitOr(leftVector, rightVector));
    auto xorResult = extractLanes<uint32_t, 4>(WTF::SIMD::bitXor(leftVector, rightVector));

    for (unsigned lane = 0; lane < 4; ++lane) {
        EXPECT_EQ(left[lane] & right[lane], andResult[lane]);
        EXPECT_EQ(left[lane] | right[lane], orResult[lane]);
        EXPECT_EQ(left[lane] ^ right[lane], xorResult[lane]);
    }
}

TEST(WTF_SIMDHelpers, Bitwise64)
{
    std::array<uint64_t, 2> left { 0xf0f0f0f0f0f0f0f0ull, 0x0123456789abcdefull };
    std::array<uint64_t, 2> right { 0x0ff00ff00ff00ff0ull, 0xfedcba9876543210ull };
    auto leftVector = WTF::SIMD::load(left.data());
    auto rightVector = WTF::SIMD::load(right.data());

    auto andResult = extractLanes<uint64_t, 2>(WTF::SIMD::bitAnd(leftVector, rightVector));
    auto orResult = extractLanes<uint64_t, 2>(WTF::SIMD::bitOr(leftVector, rightVector));
    auto xorResult = extractLanes<uint64_t, 2>(WTF::SIMD::bitXor(leftVector, rightVector));

    for (unsigned lane = 0; lane < 2; ++lane) {
        EXPECT_EQ(left[lane] & right[lane], andResult[lane]);
        EXPECT_EQ(left[lane] | right[lane], orResult[lane]);
        EXPECT_EQ(left[lane] ^ right[lane], xorResult[lane]);
    }
}

// isNonZero over a lessThan result is the shape callers use to turn a lane comparison into a scalar
// early exit.
TEST(WTF_SIMDHelpers, LessThanFeedsIsNonZero)
{
    auto hasInversion = [](const uint16_t* data) {
        return WTF::SIMD::isNonZero(WTF::SIMD::lessThan(WTF::SIMD::load(data + 1), WTF::SIMD::load(data)));
    };

    std::array<uint16_t, 9> ascending { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::array<uint16_t, 9> withInversion { 1, 2, 3, 4, 5, 6, 8, 7, 9 };
    EXPECT_FALSE(hasInversion(ascending.data()));
    EXPECT_TRUE(hasInversion(withInversion.data()));
}

} // namespace TestWebKitAPI
