/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "ArithProfile.h"

namespace JSC {

/* This class stores the following components in 16 bits:
 * - ObservedType for right-hand-side
 * - ObservedType for left-hand-side
 * - a bit used by division to indicate whether a special fast path was taken
 */
using CompareProfileBase = uint16_t;
class CompareProfile {
    static constexpr uint32_t rhsObservedTypeShift = 0;
    static constexpr uint32_t lhsObservedTypeShift = rhsObservedTypeShift + ObservedType::numBitsNeeded;

    static_assert(ObservedType::numBitsNeeded == 3, "We make a hard assumption about that here.");
    static constexpr CompareProfileBase clearRhsObservedTypeBitMask = static_cast<CompareProfileBase>(~(0b111 << rhsObservedTypeShift));
    static constexpr CompareProfileBase clearLhsObservedTypeBitMask = static_cast<CompareProfileBase>(~(0b111 << lhsObservedTypeShift));

    static constexpr CompareProfileBase observedTypeMask = (1 << ObservedType::numBitsNeeded) - 1;

public:
    CompareProfile()
    {
        ASSERT(lhsObservedType().isEmpty());
        ASSERT(rhsObservedType().isEmpty());
    }

    static constexpr BinaryArithProfileBase observedIntIntBits()
    {
        constexpr ObservedType observedInt32 { ObservedType().withInt32() };
        constexpr BinaryArithProfileBase bits = (observedInt32.bits() << lhsObservedTypeShift) | (observedInt32.bits() << rhsObservedTypeShift);
        return bits;
    }
    static constexpr BinaryArithProfileBase observedNumberIntBits()
    {
        constexpr ObservedType observedNumber { ObservedType().withNumber() };
        constexpr ObservedType observedInt32 { ObservedType().withInt32() };
        constexpr BinaryArithProfileBase bits = (observedNumber.bits() << lhsObservedTypeShift) | (observedInt32.bits() << rhsObservedTypeShift);
        return bits;
    }
    static constexpr BinaryArithProfileBase observedIntNumberBits()
    {
        constexpr ObservedType observedNumber { ObservedType().withNumber() };
        constexpr ObservedType observedInt32 { ObservedType().withInt32() };
        constexpr BinaryArithProfileBase bits = (observedInt32.bits() << lhsObservedTypeShift) | (observedNumber.bits() << rhsObservedTypeShift);
        return bits;
    }
    static constexpr BinaryArithProfileBase observedNumberNumberBits()
    {
        constexpr ObservedType observedNumber { ObservedType().withNumber() };
        constexpr BinaryArithProfileBase bits = (observedNumber.bits() << lhsObservedTypeShift) | (observedNumber.bits() << rhsObservedTypeShift);
        return bits;
    }

    constexpr ObservedType lhsObservedType() const { return ObservedType((m_bits >> lhsObservedTypeShift) & observedTypeMask); }
    constexpr ObservedType rhsObservedType() const { return ObservedType((m_bits >> rhsObservedTypeShift) & observedTypeMask); }
    void setLhsObservedType(ObservedType type)
    {
        BinaryArithProfileBase bits = m_bits;
        bits &= clearLhsObservedTypeBitMask;
        bits |= type.bits() << lhsObservedTypeShift;
        m_bits = bits;
        ASSERT(lhsObservedType() == type);
    }

    void setRhsObservedType(ObservedType type)
    {
        BinaryArithProfileBase bits = m_bits;
        bits &= clearRhsObservedTypeBitMask;
        bits |= type.bits() << rhsObservedTypeShift;
        m_bits = bits;
        ASSERT(rhsObservedType() == type);
    }

    void lhsSawInt32() { setLhsObservedType(lhsObservedType().withInt32()); }
    void lhsSawNumber() { setLhsObservedType(lhsObservedType().withNumber()); }
    void lhsSawNonNumber() { setLhsObservedType(lhsObservedType().withNonNumber()); }
    void rhsSawInt32() { setRhsObservedType(rhsObservedType().withInt32()); }
    void rhsSawNumber() { setRhsObservedType(rhsObservedType().withNumber()); }
    void rhsSawNonNumber() { setRhsObservedType(rhsObservedType().withNonNumber()); }

    void observeLHS(JSValue lhs)
    {
        auto newProfile = *this;
        if (lhs.isNumber()) {
            if (lhs.isInt32())
                newProfile.lhsSawInt32();
            else
                newProfile.lhsSawNumber();
        } else
            newProfile.lhsSawNonNumber();

        m_bits = newProfile.bits();
    }

    void observeLHSAndRHS(JSValue lhs, JSValue rhs)
    {
        observeLHS(lhs);

        auto newProfile = *this;
        if (rhs.isNumber()) {
            if (rhs.isInt32())
                newProfile.rhsSawInt32();
            else
                newProfile.rhsSawNumber();
        } else
            newProfile.rhsSawNonNumber();

        m_bits = newProfile.bits();
    }

    bool isObservedTypeEmpty()
    {
        return lhsObservedType().isEmpty() && rhsObservedType().isEmpty();
    }

    friend class JSC::LLIntOffsetsExtractor;

    constexpr CompareProfileBase bits() const { return m_bits; }

    CompareProfileBase m_bits { };
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, const JSC::CompareProfile&);

} // namespace WTF
