/*
 * Copyright (C) 2021 Tyler Wilcock <twilco.o@protonmail.com>.
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValue.h>
#include <wtf/Function.h>

namespace WebCore {

class CSSValuePair final : public CSSValue {
public:
    static Ref<CSSValuePair> create(Ref<CSSValue>, Ref<CSSValue>);
    static Ref<CSSValuePair> createSlashSeparated(Ref<CSSValue>, Ref<CSSValue>);
    static Ref<CSSValuePair> createNoncoalescing(Ref<CSSValue>, Ref<CSSValue>);

    const CSSValue& first() const { return m_first; }
    CSSValue& first() { return m_first; }
    const CSSValue& second() const { return m_second; }
    CSSValue& second() { return m_second; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSValuePair&) const;
    bool canBeCoalesced() const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_first.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_second.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }

private:
    friend bool CSSValue::addHash(Hasher&) const;

    enum class IdenticalValueSerialization : bool { DoNotCoalesce, Coalesce };
    CSSValuePair(ValueSeparator, Ref<CSSValue>, Ref<CSSValue>, IdenticalValueSerialization);

    bool addDerivedHash(Hasher&) const;

    // FIXME: Store coalesce bit in CSSValue to cut down on object size.
    bool m_coalesceIdenticalValues { true };
    const Ref<CSSValue> m_first;
    const Ref<CSSValue> m_second;
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
