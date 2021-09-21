// Taken from LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See Source/WTF/LICENSE-LLVM.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Based on:
// https://github.com/llvm/llvm-project/blob/d480f968/flang/include/flang/Common/leading-zero-bit-count.h

#pragma once

// A fast and portable function that implements Fortran's LEADZ intrinsic
// function, which counts the number of leading (most significant) zero bit
// positions in an integer value. (If the most significant bit is set, the
// leading zero count is zero; if no bit is set, the leading zero count is the
// word size in bits; otherwise, it's the largest left shift count that
// doesn't reduce the number of bits in the word that are set.)

#include <cinttypes>

namespace WTF {

// The following magic constant is a binary deBruijn sequence.
// It has the remarkable property that if one extends it
// (virtually) on the right with 5 more zero bits, then all
// of the 64 contiguous framed blocks of six bits in the
// extended 69-bit sequence are distinct. Consequently,
// if one shifts it left by any shift count [0..63] with
// truncation and extracts the uppermost six bit field
// of the shifted value, each shift count maps to a distinct
// field value. That means that we can map those 64 field
// values back to the shift counts that produce them,
// and (the point) this means that we can shift this value
// by an unknown bit count in [0..63] and then figure out
// what that count must have been.
//    0   7   e   d   d   5   e   5   9   a   4   e   2   8   c   2
// 0000011111101101110101011110010110011010010011100010100011000010
static constexpr std::uint64_t deBruijn {0x07edd5e59a4e28c2};
extern WTF_EXPORT_PRIVATE const std::uint8_t mapping[64];

extern WTF_EXPORT_PRIVATE const std::uint8_t eightBitLeadingZeroBitCount[256];

inline constexpr int leadingZeroBitCount(std::uint64_t x)
{
    if (!x)
        return 64;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    // All of the bits below the uppermost set bit are now also set.
    x -= x >> 1; // All of the bits below the uppermost are now clear.
    // x now has exactly one bit set, so it is a power of two, so
    // multiplication by x is equivalent to a left shift by its
    // base-2 logarithm. We calculate that unknown base-2 logarithm
    // by shifting the deBruijn sequence and mapping the framed value.
    int base2Log {mapping[(x * deBruijn) >> 58]};
    return 63 - base2Log; // convert to leading zero count
}

inline constexpr int leadingZeroBitCount(std::uint32_t x)
{
    return leadingZeroBitCount(static_cast<std::uint64_t>(x)) - 32;
}

inline constexpr int leadingZeroBitCount(std::uint16_t x)
{
    return leadingZeroBitCount(static_cast<std::uint64_t>(x)) - 48;
}

inline int leadingZeroBitCount(std::uint8_t x)
{
    return eightBitLeadingZeroBitCount[x];
}

template <typename A> inline constexpr int bitsNeededFor(A x)
{
    return 8 * sizeof x - leadingZeroBitCount(x);
}

} // namespace WTF

using WTF::leadingZeroBitCount;
using WTF::bitsNeededFor;
