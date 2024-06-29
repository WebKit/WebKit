/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/text/StringCommon.h>

namespace WTF {

SUPPRESS_ASAN
const float* findFloatAlignedImpl(const float* pointer, float target, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0b11));

    constexpr simde_uint32x4_t indexMask { 0, 1, 2, 3 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const float* cursor = pointer;
    constexpr size_t stride = SIMD::stride<float>;

    simde_float32x4_t targetsVector = simde_vdupq_n_f32(target);

    while (true) {
        simde_float32x4_t value = simde_vld1q_f32(cursor);
        simde_uint32x4_t mask = simde_vceqq_f32(value, targetsVector);
        if (simde_vget_lane_u64(simde_vreinterpret_u64_u16(simde_vmovn_u32(mask)), 0)) {
            simde_uint32x4_t ranked = simde_vornq_u32(indexMask, mask);
            uint32_t index = simde_vminvq_u32(ranked);
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const double* findDoubleAlignedImpl(const double* pointer, double target, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0b111));

    constexpr simde_uint32x2_t indexMask { 0, 1 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const double* cursor = pointer;
    constexpr size_t stride = SIMD::stride<double>;

    simde_float64x2_t targetsVector = simde_vdupq_n_f64(target);

    while (true) {
        simde_float64x2_t value = simde_vld1q_f64(cursor);
        simde_uint64x2_t mask = simde_vceqq_f64(value, targetsVector);
        simde_uint32x2_t reducedMask = simde_vmovn_u64(mask);
        if (simde_vget_lane_u64(simde_vreinterpret_u64_u32(reducedMask), 0)) {
            simde_uint32x2_t ranked = simde_vorn_u32(indexMask, reducedMask);
            uint32_t index = simde_vminv_u32(ranked);
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const LChar* find8NonASCIIAlignedImpl(std::span<const LChar> data)
{
    constexpr simde_uint8x16_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

    auto* pointer = data.data();
    auto length = data.size();
    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint8_t* cursor = bitwise_cast<const uint8_t*>(pointer);
    constexpr size_t stride = SIMD::stride<uint8_t>;

    simde_uint8x16_t charactersVector = simde_vdupq_n_u8(0x80);

    while (true) {
        simde_uint8x16_t value = simde_vld1q_u8(cursor);
        simde_uint8x16_t mask = simde_vcgeq_u8(value, charactersVector);
        if (simde_vmaxvq_u8(mask)) {
            simde_uint8x16_t ranked = simde_vornq_u8(indexMask, mask);
            uint8_t index = simde_vminvq_u8(ranked);
            return bitwise_cast<const LChar*>((index < length) ? cursor + index : nullptr);
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const UChar* find16NonASCIIAlignedImpl(std::span<const UChar> data)
{
    auto* pointer = data.data();
    auto length = data.size();
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0x1));

    constexpr simde_uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint16_t* cursor = bitwise_cast<const uint16_t*>(pointer);
    constexpr size_t stride = SIMD::stride<uint16_t>;

    simde_uint16x8_t charactersVector = simde_vdupq_n_u16(0x80);

    while (true) {
        simde_uint16x8_t value = simde_vld1q_u16(cursor);
        simde_uint16x8_t mask = simde_vcgeq_u16(value, charactersVector);
        if (simde_vget_lane_u64(simde_vreinterpret_u64_u8(simde_vmovn_u16(mask)), 0)) {
            simde_uint16x8_t ranked = simde_vornq_u16(indexMask, mask);
            uint16_t index = simde_vminvq_u16(ranked);
            return bitwise_cast<const UChar*>((index < length) ? cursor + index : nullptr);
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

} // namespace WTF
