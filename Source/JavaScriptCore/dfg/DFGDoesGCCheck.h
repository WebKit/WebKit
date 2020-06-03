/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <wtf/Assertions.h>

namespace JSC {

class VM;

namespace DFG {

struct DoesGCCheck {
    enum class Special {
        ReservedUnused, // so that specials start with 1. isSpecial() relies on this.
        Uninitialized,
        DFGOSRExit,
        FTLOSRExit,
        NumberOfSpecials
    };

    DoesGCCheck()
        : m_value(encode(true, Special::Uninitialized))
    { }

    static uint64_t encode(bool expectDoesGC, unsigned nodeIndex, unsigned nodeOp)
    {
        uint64_t result = bits(nodeIndex) << nodeIndexShift;
        result |= bits(nodeOp) << nodeOpShift;
        result |= bits(expectDoesGC) << expectDoesGCShift;
        return result;
    }

    static uint64_t encode(bool expectDoesGC, Special special)
    {
        ASSERT(bits(special) < numberOfSpecials);
        uint64_t result = bits(special) << specialShift;
        result |= bits(expectDoesGC) << expectDoesGCShift;
        return result;
    }

    void set(bool expectDoesGC, unsigned nodeIndex, unsigned nodeOp)
    {
        m_value = encode(expectDoesGC, nodeIndex, nodeOp);
    }

    void set(bool expectDoesGC, Special special)
    {
        m_value = encode(expectDoesGC, special);
    }

    bool expectDoesGC() { return m_value & expectDoesGCMask; }
    Special special() { return static_cast<Special>(specialIndex()); }
    unsigned nodeIndex() { return m_value >> nodeIndexShift; }
    unsigned nodeOp() { return (m_value >> nodeOpShift) & nodeOpMask; }

    bool isSpecial() { return specialIndex(); }

    void verifyCanGC(VM&);

private:
    unsigned specialIndex() { return (m_value >> specialShift) & specialMask; }

    template<typename T> static uint64_t bits(T value) { return static_cast<uint64_t>(value); }

    static constexpr uint64_t numberOfSpecials = static_cast<uint64_t>(Special::NumberOfSpecials);

    static constexpr unsigned expectDoesGCShift = 0;
    static constexpr unsigned specialShift = 1;
    static constexpr unsigned nodeOpShift = 16;
    static constexpr unsigned nodeIndexShift = 32;

    static constexpr unsigned expectDoesGCMask = 0x1;
    static constexpr unsigned specialMask = 0x7fff;
    static constexpr unsigned nodeOpMask = 0xffff;

    uint64_t m_value { 0 };
};

} // namespace DFG

using DFG::DoesGCCheck;

} // namespace JSC
