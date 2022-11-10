/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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
 *
 * Vigna, Sebastiano (2014). "Further scramblings of Marsaglia's xorshift
 * generators". arXiv:1404.0390 (http://arxiv.org/abs/1404.0390)
 *
 * See also https://en.wikipedia.org/wiki/Xorshift.
 */

#pragma once

#include <limits.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// The code used to generate random numbers are inlined manually in JIT code.
// So it needs to stay in sync with the JIT one.
class WeakRandom final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WeakRandom(unsigned seed = cryptographicallyRandomNumber())
    {
        setSeed(seed);
    }

    void setSeed(unsigned seed)
    {
        m_seed = seed;

        // A zero seed would cause an infinite series of zeroes.
        if (!seed)
            seed = 1;

        m_low = seed;
        m_high = seed;
        advance();
    }

    unsigned seed() const { return m_seed; }

    double get()
    {
        uint64_t value = advance() & ((1ULL << 53) - 1);
        return value * (1.0 / (1ULL << 53));
    }

    unsigned getUint32()
    {
        return static_cast<unsigned>(advance());
    }

    unsigned getUint32(unsigned limit)
    {
        if (limit <= 1)
            return 0;
        uint64_t cutoff = (static_cast<uint64_t>(std::numeric_limits<unsigned>::max()) + 1) / limit * limit;
        for (;;) {
            uint64_t value = getUint32();
            if (value >= cutoff)
                continue;
            return value % limit;
        }
    }

    uint64_t getUint64()
    {
        return advance();
    }

    bool returnTrueWithProbability(double probability)
    {
        ASSERT(0.0 <= probability && probability <= 1.0);

        if (!probability)
            return false;

        double value = getUint32();
        if (value <= static_cast<double>(std::numeric_limits<unsigned>::max()) * probability)
            return true;
        return false;
    }

    static unsigned lowOffset() { return OBJECT_OFFSETOF(WeakRandom, m_low); }
    static unsigned highOffset() { return OBJECT_OFFSETOF(WeakRandom, m_high); }

    static constexpr uint64_t nextState(uint64_t x, uint64_t y)
    {
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        return x;
    }

    static constexpr uint64_t generate(unsigned seed)
    {
        if (!seed)
            seed = 1;
        uint64_t low = seed;
        uint64_t high = seed;
        high = nextState(low, high);
        return low + high;
    }

private:
    uint64_t advance()
    {
        uint64_t x = m_low;
        uint64_t y = m_high;
        m_low = y;
        m_high = nextState(x, y);
        return m_high + m_low;
    }

    unsigned m_seed;
    uint64_t m_low;
    uint64_t m_high;
};

} // namespace WTF

using WTF::WeakRandom;
