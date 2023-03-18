/*
 * Copyright (C) 2021 Tyler Wilcock <twilco.o@protonmail.com>.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "CSSIdentValue.h"

namespace WebCore {

class CSSValuePair : public CSSValue {
public:
    static Ref<CSSValuePair> create(Ref<CSSValue>&&, Ref<CSSValue>&&);
    static Ref<CSSValuePair> createNoncoalescing(Ref<CSSValue>&&, Ref<CSSValue>&&);
    static Ref<CSSValuePair> create(CSSValueID, CSSValueID);

    const CSSValue& first() const;
    const CSSValue& second() const;
    bool isNoncoalescing() const;

    String customCSSText() const;
    bool equals(const CSSValuePair&) const;

private:
    // Encode values in the scalar.
    static constexpr uintptr_t IsNoncoalescingBit = 1 << PayloadShift;
    static constexpr uint8_t FirstValueIDShift = PayloadShift + 1;
    static constexpr uint8_t SecondValueIDShift = FirstValueIDShift + IdentBits;

    enum class IdenticalValueSerialization : bool { DoNotCoalesce, Coalesce };
    CSSValuePair(Ref<CSSValue>&&, Ref<CSSValue>&&, IdenticalValueSerialization);

    Ref<CSSValue> m_first;
    Ref<CSSValue> m_second;
};

inline const CSSValue& CSSValuePair::first() const
{
    if (hasScalarInPointer())
        return CSSIdentValue::create(static_cast<CSSValueID>(scalar() >> FirstValueIDShift & IdentMask)).get();
    return opaque(this)->m_first.get();
}

inline const CSSValue& CSSValuePair::second() const
{
    if (hasScalarInPointer())
        return CSSIdentValue::create(static_cast<CSSValueID>(scalar() >> SecondValueIDShift)).get();
    return opaque(this)->m_second.get();
}

inline bool CSSValuePair::isNoncoalescing() const
{
    if (hasScalarInPointer())
        return scalar() & IsNoncoalescingBit;
    return opaque(this)->m_isNoncoalescing;
}

inline Ref<CSSValuePair> CSSValuePair::create(CSSValueID first, CSSValueID second)
{
    ASSERT(first);
    ASSERT(second);
    return *reinterpret_cast<CSSValuePair*>(typeScalar(Type::ValuePair)
        | static_cast<uintptr_t>(first) << FirstValueIDShift
        | static_cast<uintptr_t>(second) << SecondValueIDShift);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValuePair, isPair())
