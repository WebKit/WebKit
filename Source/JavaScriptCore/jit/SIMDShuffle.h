/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 the V8 project authors. All rights reserved.
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

#pragma once

#include <JavaScriptCore/CPU.h>
#include <JavaScriptCore/SIMDInfo.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

// Canonical shuffle patterns recognized for ARM64 specialized instruction selection.
enum class CanonicalShuffle : uint8_t {
    Unknown,
    Identity,
    // 64-bit element patterns
    S64x2UnzipEven,                // UZP1.2D
    S64x2UnzipOdd,                 // UZP2.2D
    S64x2Reverse,                  // EXT #8 (unary): swap two 64-bit halves
    // 32-bit element patterns
    S32x4UnzipEven,                // UZP1.4S
    S32x4UnzipOdd,                 // UZP2.4S
    S32x4ZipLower,                 // ZIP1.4S
    S32x4ZipHigher,                // ZIP2.4S
    S32x4TransposeEven,            // TRN1.4S
    S32x4TransposeOdd,             // TRN2.4S
    S32x2Reverse,                  // REV64.4S
    // 16-bit element patterns
    S16x8UnzipEven,                // UZP1.8H
    S16x8UnzipOdd,                 // UZP2.8H
    S16x8ZipLower,                 // ZIP1.8H
    S16x8ZipHigher,                // ZIP2.8H
    S16x8TransposeEven,            // TRN1.8H
    S16x8TransposeOdd,             // TRN2.8H
    S16x2Reverse,                  // REV32.8H
    S16x4Reverse,                  // REV64.8H
    // 8-bit element patterns
    S8x16UnzipEven,                // UZP1.16B
    S8x16UnzipOdd,                 // UZP2.16B
    S8x16ZipLower,                 // ZIP1.16B
    S8x16ZipHigher,                // ZIP2.16B
    S8x16TransposeEven,            // TRN1.16B
    S8x16TransposeOdd,             // TRN2.16B
    S8x2Reverse,                   // REV16.16B
    S8x4Reverse,                   // REV32.16B
    S8x8Reverse,                   // REV64.16B
};

class SIMDShuffle {
public:
    static std::optional<std::tuple<unsigned, v128_t>> isOnlyOneSideMask(v128_t pattern)
    {
        std::optional<unsigned> index;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned element = pattern.u8x16[i];
            if (element < 16) {
                if (index && index.value() != 0)
                    return std::nullopt;
                index = 0;
            } else if (element < 32) {
                if (index && index.value() != 1)
                    return std::nullopt;
                index = 1;
            } else {
                // Accept OOB.
            }
        }

        if (!index)
            return std::tuple { 2, vectorAllOnes() };

        if (index.value() == 0) {
            auto newPattern = pattern;
            for (unsigned i = 0; i < 16; ++i) {
                unsigned element = pattern.u8x16[i];
                if (element < 16)
                    newPattern.u8x16[i] = element;
                else
                    newPattern.u8x16[i] = 0xFF; // OOB
            }
            return std::tuple { 0, newPattern };
        }

        ASSERT(index.value() == 1);
        auto newPattern = pattern;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned element = pattern.u8x16[i];
            if (element < 32)
                newPattern.u8x16[i] = element - 16;
            else
                newPattern.u8x16[i] = 0xFF; // OOB
        }
        return std::tuple { 1, newPattern };
    }

    static std::optional<uint8_t> isI8x16SameElement(v128_t pattern)
    {
        constexpr unsigned numberOfElements = 16 / sizeof(uint8_t);
        if (std::all_of(pattern.u8x16, pattern.u8x16 + numberOfElements, [&](auto value) { return value == pattern.u8x16[0]; }))
            return pattern.u8x16[0];
        return std::nullopt;
    }

    static std::optional<uint8_t> isI8x16DupElement(v128_t pattern)
    {
        constexpr unsigned numberOfElements = 16 / sizeof(uint8_t);
        if (std::all_of(pattern.u8x16, pattern.u8x16 + numberOfElements, [&](auto value) { return value == pattern.u8x16[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint8_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI16x8DupElement(v128_t pattern)
    {
        if (!isI16x8Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint16_t);
        if (std::all_of(pattern.u16x8, pattern.u16x8 + numberOfElements, [&](auto value) { return value == pattern.u16x8[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint16_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI32x4DupElement(v128_t pattern)
    {
        if (!isI32x4Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint32_t);
        if (std::all_of(pattern.u32x4, pattern.u32x4 + numberOfElements, [&](auto value) { return value == pattern.u32x4[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint32_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI64x2DupElement(v128_t pattern)
    {
        if (!isI64x2Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint64_t);
        if (std::all_of(pattern.u64x2, pattern.u64x2 + numberOfElements, [&](auto value) { return value == pattern.u64x2[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint64_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static bool isI16x8Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 2);
    }

    static bool isI32x4Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 4);
    }

    static bool isI64x2Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 8);
    }

    static bool isIdentity(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 16);
    }

    static bool isAllOutOfBoundsForUnaryShuffle(v128_t pattern)
    {
        for (unsigned i = 0; i < 16; ++i) {
            if constexpr (isX86()) {
                // https://www.felixcloutier.com/x86/pshufb
                // On x64, OOB index means that highest bit is set.
                // The acutal index is extracted by masking with 0b1111.
                // So, for example, 0x11 index (17) will be converted to 0x1 access (not OOB).
                if (!(pattern.u8x16[i] & 0x80))
                    return false;
            } else if constexpr (isARM64()) {
                // https://developer.arm.com/documentation/dui0801/g/A64-SIMD-Vector-Instructions/TBL--vector-
                // On ARM64, OOB index means out of 0..15 range for unary TBL.
                if (pattern.u8x16[i] < 16)
                    return false;
            } else
                return false;
        }
        return true;
    }

    static bool isAllOutOfBoundsForBinaryShuffle(v128_t pattern)
    {
        ASSERT(isARM64()); // Binary Shuffle is only supported by ARM64.
        for (unsigned i = 0; i < 16; ++i) {
            if (pattern.u8x16[i] < 32)
                return false;
        }
        return true;
    }

    // Detect unary EXT (byte rotation) pattern.
    // Returns offset if pattern[i] == (offset + i) % 16 for all i,
    // with offset in [1, 15]. Offset 0 is identity (handled separately).
    static std::optional<uint8_t> isUnaryEXT(v128_t pattern)
    {
        uint8_t first = pattern.u8x16[0];
        if (first == 0 || first >= 16)
            return std::nullopt;
        for (unsigned i = 1; i < 16; ++i) {
            if (pattern.u8x16[i] != ((first + i) % 16))
                return std::nullopt;
        }
        return first;
    }

    // Detect EXT (byte extraction / concatenation) pattern for binary shuffle.
    // Returns the byte offset if the pattern is {offset, offset+1, ..., 31, 0, 1, ...}
    // i.e., pattern[i] == (offset + i) % 32 for some offset in [0, 15].
    // ARM64 EXT instruction: EXT Vd.16B, Vn.16B, Vm.16B, #imm
    // extracts bytes from the concatenation of Vn:Vm.
    // Returns {offset, needsSwap}.
    struct EXTInfo {
        uint8_t offset;
        bool needsSwap;
    };
    static std::optional<EXTInfo> isEXTWithSwap(v128_t pattern)
    {
        uint8_t first = pattern.u8x16[0];
        if (first >= 32)
            return std::nullopt;
        for (unsigned i = 1; i < 16; ++i) {
            if (pattern.u8x16[i] != ((first + i) % 32))
                return std::nullopt;
        }
        if (first < 16)
            return EXTInfo { first, false };
        return EXTInfo { static_cast<uint8_t>(first - 16), true };
    }

    // Try to match a canonical binary shuffle pattern for ARM64 specialized instructions.
    static CanonicalShuffle tryMatchCanonicalBinary(v128_t pattern)
    {
        return tryMatchCanonicalBinaryImpl(pattern, [](v128_t v) constexpr { return v; });
    }

    // Try to match a unary shuffle pattern as a binary canonical shuffle.
    // Binary canonical pattern gets converted to unary canonical pattern with the invariant that both inputs are the same.
    // TRN2.16B {1,17, 3,19, 5,21, 7,23, 9,25, 11,27, 13,29, 15,31} gets converted to
    // {1,1, 3,3, 5,5, 7,7, 9,9, 11,11, 13,13, 15,15}.
    static CanonicalShuffle tryMatchUnaryAsBinaryCanonical(v128_t pattern)
    {
        return tryMatchCanonicalBinaryImpl(pattern,
            [](v128_t v) constexpr {
                for (unsigned i = 0; i < 16; ++i) {
                    if (v.u8x16[i] >= 16)
                        v.u8x16[i] = v.u8x16[i] - 16;
                }
                return v;
            });
    }

    // Try to match a canonical unary shuffle pattern.
    static CanonicalShuffle tryMatchCanonicalUnary(v128_t pattern)
    {
        // REV64.4S: reverse 32-bit pairs within 64-bit lanes
        if (bitEquals(pattern, v128_t::fromU8x16(4,5,6,7, 0,1,2,3, 12,13,14,15, 8,9,10,11)))
            return CanonicalShuffle::S32x2Reverse;
        // REV64.8H: reverse 16-bit elements within 64-bit lanes
        if (bitEquals(pattern, v128_t::fromU8x16(6,7, 4,5, 2,3, 0,1, 14,15, 12,13, 10,11, 8,9)))
            return CanonicalShuffle::S16x4Reverse;
        // REV32.8H: reverse 16-bit pairs within 32-bit groups
        if (bitEquals(pattern, v128_t::fromU8x16(2,3, 0,1, 6,7, 4,5, 10,11, 8,9, 14,15, 12,13)))
            return CanonicalShuffle::S16x2Reverse;
        // REV64.16B: reverse bytes within 64-bit lanes
        if (bitEquals(pattern, v128_t::fromU8x16(7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8)))
            return CanonicalShuffle::S8x8Reverse;
        // REV32.16B: reverse bytes within 32-bit groups
        if (bitEquals(pattern, v128_t::fromU8x16(3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12)))
            return CanonicalShuffle::S8x4Reverse;
        // REV16.16B: reverse bytes within 16-bit groups
        if (bitEquals(pattern, v128_t::fromU8x16(1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14)))
            return CanonicalShuffle::S8x2Reverse;
        // S64x2 reverse: swap two 64-bit halves = {8..15, 0..7}
        if (bitEquals(pattern, v128_t::fromU8x16(8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7)))
            return CanonicalShuffle::S64x2Reverse;

        return CanonicalShuffle::Unknown;
    }

    // Compose two unary shuffles: outer(inner(x)) → combined(x).
    // Both patterns have indices in 0..15 (or OOB >= 16).
    static v128_t composeUnaryShuffle(v128_t outerPattern, v128_t innerPattern)
    {
        v128_t result;
        for (unsigned i = 0; i < 16; ++i) {
            uint8_t outerIdx = outerPattern.u8x16[i];
            if (outerIdx >= 16) {
                result.u8x16[i] = 0xFF; // OOB
                continue;
            }
            result.u8x16[i] = innerPattern.u8x16[outerIdx];
        }
        return result;
    }

    // Compose an outer shuffle with an inner shuffle.
    // Given outer = shuffle(inner_result, other, outerPattern) or
    //       outer = shuffle(other, inner_result, outerPattern)
    // where inner_result = shuffle(innerSrc, innerPattern) [unary]
    // Produces a combined pattern that reads from (innerSrc, other) or (other, innerSrc).
    //
    // innerIsChild0: whether the inner shuffle's result is child(0) of the outer.
    // Returns nullopt if composition is not possible (e.g., indices go out of range).
    static v128_t composeShuffle(v128_t outerPattern, v128_t innerPattern, bool innerIsChild0)
    {
        v128_t result;
        for (unsigned i = 0; i < 16; ++i) {
            uint8_t outerIdx = outerPattern.u8x16[i];
            if (outerIdx >= 32) {
                // Out of bounds in outer — result is 0 on ARM64 TBL.
                result.u8x16[i] = 0xFF; // OOB
                continue;
            }

            if (innerIsChild0) {
                if (outerIdx < 16) {
                    // Reads from inner's output. Chase through inner.
                    uint8_t innerIdx = innerPattern.u8x16[outerIdx];
                    if (innerIdx >= 16) {
                        result.u8x16[i] = 0xFF; // OOB in inner → zero
                        continue;
                    }
                    // innerIdx is 0..15, referring to inner's input.
                    // In the composed shuffle, inner's input is child0, other is child1.
                    result.u8x16[i] = innerIdx;
                } else {
                    // Reads from the other child (child1 of outer).
                    // In composed result, other is child1, offset 16..31.
                    result.u8x16[i] = outerIdx; // stays as 16..31
                }
            } else {
                // inner is child1 of outer
                if (outerIdx >= 16) {
                    // Reads from inner's output (which is child1 of outer).
                    uint8_t innerIdx = innerPattern.u8x16[outerIdx - 16];
                    if (innerIdx >= 16) {
                        result.u8x16[i] = 0xFF;
                        continue;
                    }
                    // inner's input becomes child1 in composed result.
                    result.u8x16[i] = innerIdx + 16;
                } else {
                    // Reads from the other child (child0 of outer).
                    result.u8x16[i] = outerIdx; // stays as 0..15
                }
            }
        }
        return result;
    }

    static v128_t toCanonicalUnaryPattern(v128_t pattern)
    {
        for (unsigned i = 0; i < 16; ++i) {
            if (pattern.u8x16[i] > 15)
                pattern.u8x16[i] = 0xFF; // Force OOB
        }
        return pattern;
    }

private:
    static CanonicalShuffle tryMatchCanonicalBinaryImpl(v128_t pattern, const Invocable<v128_t(v128_t)> auto& canonicalize)
    {
        // 64-bit element patterns
        // UZP1.2D: {a[lo64], b[lo64]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1,2,3,4,5,6,7, 16,17,18,19,20,21,22,23))))
            return CanonicalShuffle::S64x2UnzipEven;
        // UZP2.2D: {a[hi64], b[hi64]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(8,9,10,11,12,13,14,15, 24,25,26,27,28,29,30,31))))
            return CanonicalShuffle::S64x2UnzipOdd;

        // 32-bit element patterns
        // UZP1.4S: {a[0], a[2], b[0], b[2]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1,2,3, 8,9,10,11, 16,17,18,19, 24,25,26,27))))
            return CanonicalShuffle::S32x4UnzipEven;
        // UZP2.4S: {a[1], a[3], b[1], b[3]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(4,5,6,7, 12,13,14,15, 20,21,22,23, 28,29,30,31))))
            return CanonicalShuffle::S32x4UnzipOdd;
        // ZIP1.4S: {a[0], b[0], a[1], b[1]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1,2,3, 16,17,18,19, 4,5,6,7, 20,21,22,23))))
            return CanonicalShuffle::S32x4ZipLower;
        // ZIP2.4S: {a[2], b[2], a[3], b[3]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(8,9,10,11, 24,25,26,27, 12,13,14,15, 28,29,30,31))))
            return CanonicalShuffle::S32x4ZipHigher;
        // TRN1.4S: {a[0], b[0], a[2], b[2]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1,2,3, 16,17,18,19, 8,9,10,11, 24,25,26,27))))
            return CanonicalShuffle::S32x4TransposeEven;
        // TRN2.4S: {a[1], b[1], a[3], b[3]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(4,5,6,7, 20,21,22,23, 12,13,14,15, 28,29,30,31))))
            return CanonicalShuffle::S32x4TransposeOdd;

        // 16-bit element patterns
        // UZP1.8H: {a[0],a[2],a[4],a[6], b[0],b[2],b[4],b[6]}
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1, 4,5, 8,9, 12,13, 16,17, 20,21, 24,25, 28,29))))
            return CanonicalShuffle::S16x8UnzipEven;
        // UZP2.8H
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(2,3, 6,7, 10,11, 14,15, 18,19, 22,23, 26,27, 30,31))))
            return CanonicalShuffle::S16x8UnzipOdd;
        // ZIP1.8H
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1, 16,17, 2,3, 18,19, 4,5, 20,21, 6,7, 22,23))))
            return CanonicalShuffle::S16x8ZipLower;
        // ZIP2.8H
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(8,9, 24,25, 10,11, 26,27, 12,13, 28,29, 14,15, 30,31))))
            return CanonicalShuffle::S16x8ZipHigher;
        // TRN1.8H
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,1, 16,17, 4,5, 20,21, 8,9, 24,25, 12,13, 28,29))))
            return CanonicalShuffle::S16x8TransposeEven;
        // TRN2.8H
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(2,3, 18,19, 6,7, 22,23, 10,11, 26,27, 14,15, 30,31))))
            return CanonicalShuffle::S16x8TransposeOdd;

        // 8-bit element patterns
        // UZP1.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,2,4,6,8,10,12,14, 16,18,20,22,24,26,28,30))))
            return CanonicalShuffle::S8x16UnzipEven;
        // UZP2.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(1,3,5,7,9,11,13,15, 17,19,21,23,25,27,29,31))))
            return CanonicalShuffle::S8x16UnzipOdd;
        // ZIP1.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,16, 1,17, 2,18, 3,19, 4,20, 5,21, 6,22, 7,23))))
            return CanonicalShuffle::S8x16ZipLower;
        // ZIP2.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(8,24, 9,25, 10,26, 11,27, 12,28, 13,29, 14,30, 15,31))))
            return CanonicalShuffle::S8x16ZipHigher;
        // TRN1.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(0,16, 2,18, 4,20, 6,22, 8,24, 10,26, 12,28, 14,30))))
            return CanonicalShuffle::S8x16TransposeEven;
        // TRN2.16B
        if (bitEquals(pattern, canonicalize(v128_t::fromU8x16(1,17, 3,19, 5,21, 7,23, 9,25, 11,27, 13,29, 15,31))))
            return CanonicalShuffle::S8x16TransposeOdd;

        return CanonicalShuffle::Unknown;
    }

    static bool isLargerElementShuffle(v128_t pattern, unsigned size)
    {
        unsigned numberOfElements = 16 / size;
        for (unsigned i = 0; i < numberOfElements; ++i) {
            unsigned firstIndex = i * size;
            unsigned first = pattern.u8x16[firstIndex];
            if (first % size != 0)
                return false;
            for (unsigned j = 1; j < size; ++j) {
                unsigned index = j + firstIndex;
                if (pattern.u8x16[index] != (first + j))
                    return false;
            }
        }
        return true;
    }
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
