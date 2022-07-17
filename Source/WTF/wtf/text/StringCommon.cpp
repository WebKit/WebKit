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
const uint16_t* memchr16AlignedImpl(const uint16_t* pointer, uint16_t character, size_t length)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0x1));

    constexpr uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };

    // Our load is always aligned to 16byte. So long as at least one character exists in this range,
    // access must succeed since it does not overlap with the page boundary.

    ASSERT(length);
    ASSERT(!(reinterpret_cast<uintptr_t>(pointer) & 0xf));
    ASSERT((reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(0xf)) == reinterpret_cast<uintptr_t>(pointer));
    const uint16_t* cursor = pointer;

    // Dupe character => |c|c|c|c|c|c|c|c|
    uint16x8_t charactersVector = vdupq_n_u16(character);

    size_t oldLength;
    do {
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
        oldLength = length;
        length -= 8;
        cursor += 8;
    } while (length < oldLength);
    return nullptr;
}
#endif

} // namespace WTF
