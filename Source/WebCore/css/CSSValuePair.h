/*
 * Copyright (C) 2021 Tyler Wilcock <twilco.o@protonmail.com>.
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

#include "CSSValue.h"

namespace WebCore {

class CSSValuePair : public CSSValue {
public:
    static Ref<CSSValuePair> create(Ref<CSSValue>, Ref<CSSValue>);
    static Ref<CSSValuePair> createNoncoalescing(Ref<CSSValue>, Ref<CSSValue>);

    const CSSValue& first() const { return m_first; }
    const CSSValue& second() const { return m_second; }

    String customCSSText() const;
    bool equals(const CSSValuePair&) const;

private:
    enum class IdenticalValueSerialization : bool { DoNotCoalesce, Coalesce };
    CSSValuePair(Ref<CSSValue>, Ref<CSSValue>, IdenticalValueSerialization);

    // FIXME: Store coalesce bit in CSSValue to cut down on object size.
    bool m_coalesceIdenticalValues { true };
    Ref<CSSValue> m_first;
    Ref<CSSValue> m_second;
};

inline const CSSValue& CSSValue::first() const
{
    return downcast<CSSValuePair>(*this).first();
}

inline const CSSValue& CSSValue::second() const
{
    return downcast<CSSValuePair>(*this).second();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValuePair, isPair())
