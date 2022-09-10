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

#if CPU(ARM64)
#include <arm_neon.h>
#endif

namespace WTF {

#if CPU(ARM64)
// Suppress ASan because this code intentionally loads out-of-bound memory, but it must be safe since we do not overlap page boundary.
SUPPRESS_ASAN
const uint16_t* find16AlignedImpl(const uint16_t* pointer, uint16_t character, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0x1));

    constexpr uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };

    // Our load is always aligned to 16byte. So long as at least one character exists in this range,
    // access must succeed since it does not overlap with the page boundary.

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint16_t* cursor = pointer;
    constexpr size_t stride = 16 / sizeof(uint16_t);

    // Dupe character => |c|c|c|c|c|c|c|c|
    uint16x8_t charactersVector = vdupq_n_u16(character);

    while (true) {
        // Load target value. It is possible that this includes unrelated part of the memory.
        uint16x8_t value = vld1q_u16(cursor);
        // If the character is the same, then it becomes all-1. Otherwise, it becomes 0.
        uint16x8_t mask = vceqq_u16(value, charactersVector);

        //  value                     |c|c|c|C|c|C|c|c| (c is character, C is matching character)
        //  eq with charactersVector  |0|0|0|X|0|X|0|0| (X is all-1)
        //  Reduce to uint8x8_t       |0|0|0|X|0|X|0|0| => reinterpret it as uint64_t. If it is non-zero, matching character exists.
        if (vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(mask)), 0)) {
            // Found elements are all-1 and the other elements are 0. But it is possible that this vector
            // includes multiple found characters. We perform [0, 1, 2, 3, 4, 5, 6, 7] OR-NOT with this mask,
            // to assign the index to found characters.
            uint16x8_t ranked = vornq_u16(indexMask, mask);
            // Find the smallest value. Because of [0, 1, 2, 3, 4, 5, 6, 7], the value should be index in this vector.
            uint16_t index = vminvq_u16(ranked);
            // If the index less than length, it is within the requested pointer. Otherwise, nullptr.
            //
            // Example
            //     mask        |0|0|0|X|0|X|0|0| (X is all-one)
            //     not-mask    |X|X|X|0|X|0|X|X|
            //     index-mask  |0|1|2|3|4|5|6|7|
            //     ranked      |X|X|X|3|X|5|X|X|
            //     index       3, the smallest number from this vector, and it is the same to the index.
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const uint32_t* find32AlignedImpl(const uint32_t* pointer, uint32_t character, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0b11));

    constexpr uint32x4_t indexMask { 0, 1, 2, 3 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint32_t* cursor = pointer;
    constexpr size_t stride = 16 / sizeof(uint32_t);

    uint32x4_t charactersVector = vdupq_n_u32(character);

    while (true) {
        uint32x4_t value = vld1q_u32(cursor);
        uint32x4_t mask = vceqq_u32(value, charactersVector);
        if (vget_lane_u64(vreinterpret_u64_u16(vmovn_u32(mask)), 0)) {
            uint32x4_t ranked = vornq_u32(indexMask, mask);
            uint32_t index = vminvq_u32(ranked);
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const uint64_t* find64AlignedImpl(const uint64_t* pointer, uint64_t character, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0b111));

    constexpr uint32x2_t indexMask { 0, 1 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint64_t* cursor = pointer;
    constexpr size_t stride = 16 / sizeof(uint64_t);

    uint64x2_t charactersVector = vdupq_n_u64(character);

    while (true) {
        uint64x2_t value = vld1q_u64(cursor);
        uint64x2_t mask = vceqq_u64(value, charactersVector);
        uint32x2_t reducedMask = vmovn_u64(mask);
        if (vget_lane_u64(vreinterpret_u64_u32(reducedMask), 0)) {
            uint32x2_t ranked = vorn_u32(indexMask, reducedMask);
            uint64_t index = vminv_u32(ranked);
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const float* findFloatAlignedImpl(const float* pointer, float target, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0b11));

    constexpr uint32x4_t indexMask { 0, 1, 2, 3 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const float* cursor = pointer;
    constexpr size_t stride = 16 / sizeof(float);

    float32x4_t targetsVector = vdupq_n_f32(target);

    while (true) {
        float32x4_t value = vld1q_f32(cursor);
        uint32x4_t mask = vceqq_f32(value, targetsVector);
        if (vget_lane_u64(vreinterpret_u64_u16(vmovn_u32(mask)), 0)) {
            uint32x4_t ranked = vornq_u32(indexMask, mask);
            uint32_t index = vminvq_u32(ranked);
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

    constexpr uint32x2_t indexMask { 0, 1 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const double* cursor = pointer;
    constexpr size_t stride = 16 / sizeof(double);

    float64x2_t targetsVector = vdupq_n_f64(target);

    while (true) {
        float64x2_t value = vld1q_f64(cursor);
        uint64x2_t mask = vceqq_f64(value, targetsVector);
        uint32x2_t reducedMask = vmovn_u64(mask);
        if (vget_lane_u64(vreinterpret_u64_u32(reducedMask), 0)) {
            uint32x2_t ranked = vorn_u32(indexMask, reducedMask);
            uint32_t index = vminv_u32(ranked);
            return (index < length) ? cursor + index : nullptr;
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const LChar* find8NonASCIIAlignedImpl(const LChar* pointer, size_t length)
{
    constexpr uint8x16_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint8_t* cursor = bitwise_cast<const uint8_t*>(pointer);
    constexpr size_t stride = 16 / sizeof(uint8_t);

    uint8x16_t charactersVector = vdupq_n_u8(0x80);

    while (true) {
        uint8x16_t value = vld1q_u8(cursor);
        uint8x16_t mask = vcgeq_u8(value, charactersVector);
        if (vmaxvq_u8(mask)) {
            uint8x16_t ranked = vornq_u8(indexMask, mask);
            uint8_t index = vminvq_u8(ranked);
            return bitwise_cast<const LChar*>((index < length) ? cursor + index : nullptr);
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}

SUPPRESS_ASAN
const UChar* find16NonASCIIAlignedImpl(const UChar* pointer, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0x1));

    constexpr uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint16_t* cursor = bitwise_cast<const uint16_t*>(pointer);
    constexpr size_t stride = 16 / sizeof(uint16_t);

    uint16x8_t charactersVector = vdupq_n_u16(0x80);

    while (true) {
        uint16x8_t value = vld1q_u16(cursor);
        uint16x8_t mask = vcgeq_u16(value, charactersVector);
        if (vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(mask)), 0)) {
            uint16x8_t ranked = vornq_u16(indexMask, mask);
            uint16_t index = vminvq_u16(ranked);
            return bitwise_cast<const UChar*>((index < length) ? cursor + index : nullptr);
        }
        if (length <= stride)
            return nullptr;
        length -= stride;
        cursor += stride;
    }
}
#endif

} // namespace WTF
