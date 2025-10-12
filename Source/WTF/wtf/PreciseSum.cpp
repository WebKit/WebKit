/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

/*
 * This is a C++ port of the xsum implementation, 
 * originally invented by Radford M. Neal, and ported by Keita Nonaka.
 * 
 * The original C implementation is from https://gitlab.com/radfordneal/xsum
 * The C++ implementation is from https://github.com/Gumichocopengin8/xsum.cpp
 *
 * The LICENSE is as follows:
 *
 * This software for exact summation of floating-point values is licensed
 * under the "MIT license", included here.
 * 
 * Copyright 2015, 2018, 2021, 2024 Radford M. Neal
 * Copyright 2025 Keita Nonaka
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include <wtf/PreciseSum.h>

#include <array>
#include <cmath>

namespace WTF {

namespace {

// CONSTANTS DEFINING THE FLOATING POINT FORMAT
constexpr int64_t XSUM_MANTISSA_BITS = 52; // Bits in fp mantissa, excludes implict 1
constexpr int64_t XSUM_EXP_BITS = 11; // Bits in fp exponent
constexpr int64_t XSUM_MANTISSA_MASK = (1LL << XSUM_MANTISSA_BITS) - 1; // Mask for mantissa bits
constexpr int64_t XSUM_EXP_MASK = (1 << XSUM_EXP_BITS) - 1; // Mask for exponent
constexpr int64_t XSUM_EXP_BIAS = (1 << (XSUM_EXP_BITS - 1)) - 1; // Bias added to signed exponent
constexpr int64_t XSUM_SIGN_BIT = XSUM_MANTISSA_BITS + XSUM_EXP_BITS; // Position of sign bit
constexpr uint64_t XSUM_SIGN_MASK = 1ULL << XSUM_SIGN_BIT; // Mask for sign bit

// CONSTANTS DEFINING THE SMALL ACCUMULATOR FORMAT
constexpr int64_t XSUM_SCHUNK_BITS = 64; // Bits in chunk of the small accumulator
constexpr int64_t XSUM_LOW_EXP_BITS = 5; // # of low bits of exponent, in one chunk
constexpr int64_t XSUM_LOW_EXP_MASK = (1 << XSUM_LOW_EXP_BITS) - 1; // Mask for low-order exponent bits
constexpr int64_t XSUM_HIGH_EXP_BITS = XSUM_EXP_BITS - XSUM_LOW_EXP_BITS; // # of high exponent bits for index
constexpr int64_t XSUM_SCHUNKS = (1 << XSUM_HIGH_EXP_BITS) + 3; // # of chunks in small accumulator
constexpr int64_t XSUM_LOW_MANTISSA_BITS = 1 << XSUM_LOW_EXP_BITS; // Bits in low part of mantissa
constexpr int64_t XSUM_LOW_MANTISSA_MASK = (1LL << XSUM_LOW_MANTISSA_BITS) - 1; // Mask for low bits
constexpr int64_t XSUM_SMALL_CARRY_BITS = (XSUM_SCHUNK_BITS - 1) - XSUM_MANTISSA_BITS; // Bits sums can carry into
constexpr int64_t XSUM_SMALL_CARRY_TERMS = (1 << XSUM_SMALL_CARRY_BITS) - 1; // # terms can add before need prop.

// CONSTANTS DEFINING THE LARGE ACCUMULATOR FORMAT
constexpr int64_t XSUM_LCOUNT_BITS = 64 - XSUM_MANTISSA_BITS; // # of bits in count
constexpr int64_t XSUM_LCHUNKS = 1 << (XSUM_EXP_BITS + 1); // # of chunks in large accumulator

} // anonymous namespace

namespace Xsum {

// SmallAccumulator

SmallAccumulator::SmallAccumulator()
    : chunk(XSUM_SCHUNKS, 0LL), addsUntilPropagate { XSUM_SMALL_CARRY_TERMS }, inf { 0 }, nan { 0 },
    sizeCount { 0 }, hasPosNumber { false } { }

SmallAccumulator::SmallAccumulator(
    Vector<int64_t> &&chunk, const int addsUntilPropagate, const int64_t inf, const int64_t nan,
    const size_t sizeCount, const bool hasPosNumber)
: chunk(WTFMove(chunk)), addsUntilPropagate { addsUntilPropagate }, inf { inf }, nan { nan },
    sizeCount { sizeCount }, hasPosNumber { hasPosNumber } { }

/*
ADD AN INF OR NAN TO A SMALL ACCUMULATOR. This only changes the flags,
not the chunks in the accumulator, which retains the sum of the finite
terms (which is perhaps sometimes useful to access, though no function
to do so is defined at present). A nan with larger payload (seen as a
52-bit unsigned integer) takes precedence, with the sign of the nan always
being positive. This ensures that the order of summing nan values doesn't
matter.
*/
COLD void SmallAccumulator::addInfNan(int64_t ivalue)
{
    const int64_t mantissa = ivalue & XSUM_MANTISSA_MASK;
    if (!mantissa) {
        if (!inf)
            inf = ivalue;
        else if (inf != ivalue) {
            double fltv = std::bit_cast<double>(ivalue);
            fltv = fltv - fltv;
            inf = std::bit_cast<int64_t>(fltv);
        }
    } else {
        if ((nan & XSUM_MANTISSA_MASK) <= mantissa)
            nan = ivalue & ~XSUM_SIGN_MASK;
    }
}

/*
PROPAGATE CARRIES TO NEXT CHUNK IN A SMALL ACCUMULATOR. Needs to
be called often enough that accumulated carries don't overflow out
the top, as indicated by addsUntilPropagate.  
Returns the index of the uppermost non-zero chunk (0 if number is zero).

After carry propagation, the uppermost non-zero chunk will indicate
the sign of the number, and will not be -1 (all 1s). It will be in
the range -2^XSUM_LOW_MANTISSA_BITS to 2^XSUM_LOW_MANTISSA_BITS - 1.
Lower chunks will be non-negative, and in the range from 0 up to
2^XSUM_LOW_MANTISSA_BITS - 1.
*/
int SmallAccumulator::carryPropagate()
{
    int u = XSUM_SCHUNKS - 1;
    while (0 <= u && !chunk[u]) {
        if (!u) {
            addsUntilPropagate = XSUM_SMALL_CARRY_TERMS - 1;
            return 0;
        }
        --u;
    }

    int i = 0;
    int uix = -1;
    do {
        int64_t c;
        do {
            c = chunk[i];
            if (c)
                break;
            i += 1;
        } while (i <= u);

        if (i > u)
            break;

        const int64_t chigh = c >> XSUM_LOW_MANTISSA_BITS;
        if (!chigh) {
            uix = i;
            i += 1;
            continue;
        }

        if (u == i) {
            if (chigh == -1) {
                uix = i;
                break;
            }
            u = i + 1;
        }

        const int64_t clow = c & XSUM_LOW_MANTISSA_MASK;
        if (clow)
            uix = i;

        chunk[i] = clow;
        if (i + 1 >= XSUM_SCHUNKS) [[unlikely]] {
            this->addInfNan(
                (static_cast<int64_t>(XSUM_EXP_MASK) << XSUM_MANTISSA_BITS) | XSUM_MANTISSA_MASK
            );
            u = i;
        } else
            chunk[i + 1] += chigh;
        i += 1;
    } while (i <= u);

    if (uix < 0) {
        uix = 0;
        addsUntilPropagate = XSUM_SMALL_CARRY_TERMS - 1;
        return uix;
    }

    while (chunk[uix] == -1 && uix > 0) {
        chunk[uix - 1] += -(1LL << XSUM_LOW_MANTISSA_BITS);
        chunk[uix] = 0;
        uix -= 1;
    }
    addsUntilPropagate = XSUM_SMALL_CARRY_TERMS - 1;
    return uix;
}

/*
ADD ONE NUMBER TO A SMALL ACCUMULATOR ASSUMING NO CARRY PROPAGATION REQ'D.
*/
inline void SmallAccumulator::add1NoCarry(double value)
{
    const int64_t ivalue = std::bit_cast<int64_t>(value);
    const int_fast16_t exp = (ivalue >> XSUM_MANTISSA_BITS) & XSUM_EXP_MASK;
    int64_t mantissa = ivalue & XSUM_MANTISSA_MASK;
    const int_fast16_t highExp = exp >> XSUM_LOW_EXP_BITS;
    int_fast16_t lowExp = exp & XSUM_LOW_EXP_MASK;

    if (!exp) {
        if (!mantissa)
            return;
        lowExp = 1;
    } else if (exp == XSUM_EXP_MASK) [[unlikely]] {
        this->addInfNan(ivalue);
        return;
    } else
        mantissa |= 1LL << XSUM_MANTISSA_BITS;

    const std::array<int64_t, 2> splitMantissa {
        static_cast<int64_t>((static_cast<uint64_t>(mantissa) << lowExp) & XSUM_LOW_MANTISSA_MASK),
        mantissa >> (XSUM_LOW_MANTISSA_BITS - lowExp)
    };

    if (ivalue < 0) {
        chunk[highExp] -= splitMantissa[0];
        chunk[highExp + 1] -= splitMantissa[1];
    } else {
        chunk[highExp] += splitMantissa[0];
        chunk[highExp + 1] += splitMantissa[1];
    }
}

/*
Increment sizeCount and check positive value every time when value is added.
This is needed to return -0 (negative zero) if applicable.
*/
ALWAYS_INLINE void SmallAccumulator::incrementWhenValueAdded(double value)
{
    sizeCount++;
    hasPosNumber = hasPosNumber || !std::signbit(value);
}

// LargeAccumulator

LargeAccumulator::LargeAccumulator()
    : chunk(XSUM_LCHUNKS), count(XSUM_LCHUNKS, -1), chunksUsed(XSUM_LCHUNKS / 64, 0), usedUsed { 0 }, sacc { } { }

/*
ADD CHUNK FROM A LARGE ACCUMULATOR TO THE SMALL ACCUMULATOR WITHIN IT.
The large accumulator chunk to add is indexed by ix.  This chunk will
be cleared to zero and its count reset after it has been added to the
small accumulator (except no add is done for a new chunk being initialized).
This procedure should not be called for the special chunks correspnding to
Inf or NaN, whose counts should always remain at -1.
*/
void LargeAccumulator::addLchunkToSmall(int_fast16_t ix)
{
    const int_fast16_t countElement = count[ix];

    if (countElement >= 0) {
        if (!sacc.addsUntilPropagate)
            sacc.carryPropagate();

        uint64_t chunkElement = chunk[ix];
        if (countElement > 0)
            chunkElement += static_cast<uint64_t>(countElement * ix) << XSUM_MANTISSA_BITS;

        const int_fast16_t exp = ix & XSUM_EXP_MASK;
        int_fast16_t lowExp = exp & XSUM_LOW_EXP_MASK;
        int_fast16_t highExp = exp >> XSUM_LOW_EXP_BITS;
        if (!exp) {
            lowExp = 1;
            highExp = 0;
        }

        const uint64_t lowChunk = (chunkElement << lowExp) & XSUM_LOW_MANTISSA_MASK;
        uint64_t midChunk = chunkElement >> (XSUM_LOW_MANTISSA_BITS - lowExp);
        if (exp) {
            midChunk += static_cast<uint64_t>((1 << XSUM_LCOUNT_BITS) - countElement)
                << (XSUM_MANTISSA_BITS - XSUM_LOW_MANTISSA_BITS + lowExp);
        }

        const uint64_t highChunk = midChunk >> XSUM_LOW_MANTISSA_BITS;
        midChunk &= XSUM_LOW_MANTISSA_MASK;
        if (ix & (1 << XSUM_EXP_BITS)) {
            sacc.chunk[highExp] -= lowChunk;
            sacc.chunk[highExp + 1] -= midChunk;
            sacc.chunk[highExp + 2] -= highChunk;
        } else {
            sacc.chunk[highExp] += lowChunk;
            sacc.chunk[highExp + 1] += midChunk;
            sacc.chunk[highExp + 2] += highChunk;
        }
        sacc.addsUntilPropagate -= 1;
    }
    chunk[ix] = 0;
    count[ix] = 1 << XSUM_LCOUNT_BITS;
    chunksUsed[ix >> 6] |= 1ULL << (ix & 0x3f);
    usedUsed |= 1ULL << (ix >> 6);
}

/*
ADD A CHUNK TO THE LARGE ACCUMULATOR OR PROCESS NAN OR INF.  This routine
is called when the count for a chunk is negative after decrementing, which
indicates either inf/nan, or that the chunk has not been initialized, or
that the chunk needs to be transferred to the small accumulator.
*/
COLD void LargeAccumulator::largeAddValueInfNan(int_fast16_t ix, uint64_t uintv)
{
    if ((ix & XSUM_EXP_MASK) == XSUM_EXP_MASK)
        sacc.addInfNan(uintv);
    else {
        this->addLchunkToSmall(ix);
        count[ix] -= 1;
        chunk[ix] += uintv;
    }
}

/*
TRANSFER ALL CHUNKS IN LARGE ACCUMULATOR TO ITS SMALL ACCUMULATOR.
*/
void LargeAccumulator::transferToSmall()
{
    const size_t chunksUsedSize = chunksUsed.size();
    size_t p = 0;
    uint64_t uu = usedUsed;

    if (!(uu & 0xffffffff)) {
        uu >>= 32;
        p += 32;
    }
    if (!(uu & 0xffff)) {
        uu >>= 16;
        p += 16;
    }
    if (!(uu & 0xff))
        p += 8;

    uint64_t u = chunksUsed[p];
    do {
        for (;;) {
            u = chunksUsed[p];
            if (u)
                break;
            p += 1;
            if (p == chunksUsedSize)
                return;
            u = chunksUsed[p];
            if (u)
                break;
            p += 1;
            if (p == chunksUsedSize)
                return;
            u = chunksUsed[p];
            if (u)
                break;
            p += 1;
            if (p == chunksUsedSize)
                return;
            u = chunksUsed[p];
            if (u)
                break;
            p += 1;
            if (p == chunksUsedSize)
                return;
        }

        int ix = p << 6;
        if (!(u & 0xffffffff)) {
            u >>= 32;
            ix += 32;
        }
        if (!(u & 0xffff)) {
            u >>= 16;
            ix += 16;
        }
        if (!(u & 0xff)) {
            u >>= 8;
            ix += 8;
        }
        do {
            if (count[ix] >= 0)
                this->addLchunkToSmall(ix);
            ix += 1;
            u >>= 1;
        } while (u);
        p += 1;
    } while (p < chunksUsedSize);
}

// XsumSmall

XsumSmall::XsumSmall()
    : m_smallAccumulator { } { }

XsumSmall::XsumSmall(SmallAccumulator sacc)
    : m_smallAccumulator {
        WTFMove(sacc.chunk), sacc.addsUntilPropagate, sacc.inf, sacc.nan, sacc.sizeCount, sacc.hasPosNumber
    } { }

/*
ADD A VECTOR OF FLOATING-POINT NUMBERS TO A SMALL ACCUMULATOR. Mixes
calls of carryPropagate with calls of add1NoCarry.
*/
void XsumSmall::addList(const std::span<const double> vec)
{
    size_t offset = 0;
    size_t n = vec.size();

    while (0 < n) {
        if (!m_smallAccumulator.addsUntilPropagate)
            m_smallAccumulator.carryPropagate();
        size_t m = std::min(n, static_cast<size_t>(m_smallAccumulator.addsUntilPropagate));
        for (const double value : vec.subspan(offset, m)) {
            m_smallAccumulator.incrementWhenValueAdded(value);
            m_smallAccumulator.add1NoCarry(value);
        }
        m_smallAccumulator.addsUntilPropagate -= m;
        offset += m;
        n -= m;
    }
}

/*
Add one double to a small accumulator.
*/
void XsumSmall::add(double value)
{
    m_smallAccumulator.incrementWhenValueAdded(value);
    if (!m_smallAccumulator.addsUntilPropagate)
        m_smallAccumulator.carryPropagate();
    m_smallAccumulator.add1NoCarry(value);
    m_smallAccumulator.addsUntilPropagate -= 1;
}

/*
RETURN THE RESULT OF ROUNDING A SMALL ACCUMULATOR. The rounding mode
is to nearest, with ties to even. The small accumulator may be modified
by this operation (by carry propagation being done), but the value it
represents should not change.
*/
double XsumSmall::compute()
{
    if (m_smallAccumulator.nan) [[unlikely]]
        return std::bit_cast<double>(m_smallAccumulator.nan);
    if (m_smallAccumulator.inf) [[unlikely]]
        return std::bit_cast<double>(m_smallAccumulator.inf);
    if (!m_smallAccumulator.sizeCount) [[unlikely]]
        return -0.0;

    const int i = m_smallAccumulator.carryPropagate();
    int64_t ivalue = m_smallAccumulator.chunk[i];
    int64_t intv = 0;
    if (i <= 1) {
        if (!ivalue) [[unlikely]]
            return !m_smallAccumulator.hasPosNumber ? -0.0 : 0.0;
        if (!i) {
            intv = 0 <= ivalue ? ivalue : -ivalue;
            intv >>= 1;
            if (ivalue < 0)
                intv |= XSUM_SIGN_MASK;
            return std::bit_cast<double>(intv);
        }
        int64_t intv = ivalue * (1LL << (XSUM_LOW_MANTISSA_BITS - 1)) + (m_smallAccumulator.chunk[0] >> 1);
        if (intv < 0) {
            if (intv > -(1LL << XSUM_MANTISSA_BITS)) {
                intv = (-intv) | XSUM_SIGN_MASK;
                return std::bit_cast<double>(intv);
            }
        } else if (static_cast<uint64_t>(intv) < 1ULL << XSUM_MANTISSA_BITS)
            return std::bit_cast<double>(intv);
    }

    const double fltv = static_cast<double>(ivalue);
    intv = std::bit_cast<int64_t>(fltv);
    int e = (intv >> XSUM_MANTISSA_BITS) & XSUM_EXP_MASK;
    int more = 2 + XSUM_MANTISSA_BITS + XSUM_EXP_BIAS - e;

    ivalue *= 1LL << more;
    int j = i - 1;
    int64_t lower = m_smallAccumulator.chunk[j];
    if (more >= XSUM_LOW_MANTISSA_BITS) {
        more -= XSUM_LOW_MANTISSA_BITS;
        ivalue += lower << more;
        j -= 1;
        lower = j < 0 ? 0 : m_smallAccumulator.chunk[j];
    }
    ivalue += lower >> (XSUM_LOW_MANTISSA_BITS - more);
    lower &= (1LL << (XSUM_LOW_MANTISSA_BITS - more)) - 1;

    bool shouldRoundAwayFromZero = false;
    if (0 <= ivalue) {
        intv = 0;
        if (!(ivalue & 2)) {
            // this is not required,
            // but removing the branch would change the logic
            // leave it as it is
            shouldRoundAwayFromZero = false;
        } else if ((ivalue & 1))
            shouldRoundAwayFromZero = true;
        else if ((ivalue & 4))
            shouldRoundAwayFromZero = true;
        else {
            if (!lower) {
                while (j > 0) {
                    j -= 1;
                    if (m_smallAccumulator.chunk[j]) {
                        lower = 1;
                        break;
                    }
                }
            }
            if (lower)
                shouldRoundAwayFromZero = true;
        }
    } else {
        if (!((-ivalue) & (1LL << (XSUM_MANTISSA_BITS + 2)))) {
            const int pos = 1LL << (XSUM_LOW_MANTISSA_BITS - 1 - more);
            ivalue *= 2;
            if (lower & pos) {
                ivalue += 1;
                lower &= ~pos;
            }
            e -= 1;
        }

        intv = XSUM_SIGN_MASK;
        ivalue = -ivalue;
        if ((ivalue & 3) == 3)
            shouldRoundAwayFromZero = true;
        if (!lower) {
            while (j > 0) {
                j -= 1;
                if (m_smallAccumulator.chunk[j]) {
                    lower = 1;
                    break;
                }
            }
        }
        if (!lower)
            shouldRoundAwayFromZero = true;
    }

    if (shouldRoundAwayFromZero) {
        ivalue += 4;
        if (ivalue & (1LL << (XSUM_MANTISSA_BITS + 3))) {
            ivalue >>= 1;
            e += 1;
        }
    }

    ivalue >>= 2;
    e += (i << XSUM_LOW_EXP_BITS) - XSUM_EXP_BIAS - XSUM_MANTISSA_BITS;
    if (e >= XSUM_EXP_MASK) {
        intv |= XSUM_EXP_MASK << XSUM_MANTISSA_BITS;
        return std::bit_cast<double>(intv);
    }
    intv += (static_cast<int64_t>(e) << XSUM_MANTISSA_BITS) + (ivalue & XSUM_MANTISSA_MASK);
    return std::bit_cast<double>(intv);
}

// XsumLarge

XsumLarge::XsumLarge()
    : m_largeAccumulator { } { }

/*
ADD A VECTOR OF FLOATING-POINT NUMBERS TO A LARGE ACCUMULATOR.
*/
void XsumLarge::addList(const std::span<const double> vec)
{
    for (const auto value : vec)
        this->add(value);
}

/*
ADD ONE DOUBLE TO A LARGE ACCUMULATOR.
*/
void XsumLarge::add(double value)
{
    const uint64_t uintv = std::bit_cast<uint64_t>(value);
    const int_fast16_t ix = uintv >> XSUM_MANTISSA_BITS;
    const int_least16_t count = m_largeAccumulator.count[ix] - 1;

    m_largeAccumulator.sacc.incrementWhenValueAdded(value);

    if (count < 0) [[unlikely]]
        m_largeAccumulator.largeAddValueInfNan(ix, uintv);
    else {
        m_largeAccumulator.count[ix] = count;
        m_largeAccumulator.chunk[ix] += uintv;
    }
}

/*
RETURN RESULT OF ROUNDING A LARGE ACCUMULATOR.  Rounding mode is to nearest,
with ties to even.
This is done by adding all the chunks in the large accumulator to the
small accumulator, and then calling its rounding procedure.
*/
double XsumLarge::compute()
{
    m_largeAccumulator.transferToSmall();
    XsumSmall xsumSmall { m_largeAccumulator.sacc };
    return xsumSmall.compute();
}

} // namespace Xsum

} // namespace WTF
