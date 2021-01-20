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
        Uninitialized,
        DFGOSRExit,
        FTLOSRExit,
        NumberOfSpecials
    };

    DoesGCCheck()
        : m_value(encode(true, Special::Uninitialized))
    { }

    static uint32_t encode(bool expectDoesGC, unsigned nodeIndex, unsigned nodeOp)
    {
        // We know nodeOp always fits because of the static_assert in DFGDoesGCCheck.cpp.
        ASSERT((nodeIndex << nodeIndexShift) >> nodeIndexShift == nodeIndex);
        return nodeIndex << nodeIndexShift | nodeOp << nodeOpShift | bits(expectDoesGC);
    }

    static uint32_t encode(bool expectDoesGC, Special special)
    {
        return bits(special) << specialShift | isSpecialBit | bits(expectDoesGC);
    }

    void set(bool expectDoesGC, unsigned nodeIndex, unsigned nodeOp)
    {
        m_value = encode(expectDoesGC, nodeIndex, nodeOp);
    }

    void set(bool expectDoesGC, Special special)
    {
        m_value = encode(expectDoesGC, special);
    }

    bool expectDoesGC() const { return m_value & expectDoesGCBit; }
    bool isSpecial() const { return m_value & isSpecialBit; }

    Special special() { return static_cast<Special>(m_value >> specialShift); }

    unsigned nodeOp() { return (m_value >> nodeOpShift) & nodeOpMask; }
    unsigned nodeIndex() { return m_value >> nodeIndexShift; }

    JS_EXPORT_PRIVATE void verifyCanGC(VM&);

private:
    template<typename T> static uint32_t bits(T value) { return static_cast<uint32_t>(value); }

    // The value cannot be both a Special and contain node information at the
    // time. Hence, the 2 can have separate encodings. The isSpecial bit
    // determines which encoding is in use.

    static constexpr unsigned expectDoesGCBit = 1 << 0;
    static constexpr unsigned isSpecialBit = 1 << 1;
    static constexpr unsigned commonBits = 2;
    static_assert((expectDoesGCBit | isSpecialBit) == (1 << commonBits) - 1);

    static constexpr unsigned specialShift = commonBits;

    static constexpr unsigned nodeOpBits = 9;
    static constexpr unsigned nodeOpMask = (1 << nodeOpBits) - 1;
    static constexpr unsigned nodeOpShift = commonBits;

    static constexpr unsigned nodeIndexBits = 21;
    static constexpr unsigned nodeIndexShift = nodeOpShift + nodeOpBits;
    static_assert(nodeIndexShift + nodeIndexBits == 32);

    uint32_t m_value { 0 };
};

} // namespace DFG

using DFG::DoesGCCheck;

} // namespace JSC
