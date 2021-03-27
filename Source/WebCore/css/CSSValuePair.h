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

/**
 * Represents a pair of CSS values.
 */
class CSSValuePair : public CSSValue {
public:
    enum class IdenticalValueEncoding : uint8_t {
        DoNotCoalesce,
        Coalesce
    };

    static Ref<CSSValuePair> create(Ref<CSSValue>&& first, Ref<CSSValue>&& second, ValueSeparator separator)
    {
        return adoptRef(*new CSSValuePair(WTFMove(first), WTFMove(second), separator));
    }
    static Ref<CSSValuePair> create(Ref<CSSValue>&& first, Ref<CSSValue>&& second, ValueSeparator separator, IdenticalValueEncoding encoding)
    {
        return adoptRef(*new CSSValuePair(WTFMove(first), WTFMove(second), separator, encoding));
    }

    Ref<CSSValue> first() const { return m_first; }
    Ref<CSSValue> second() const { return m_second; }

    String customCSSText() const;
    bool equals(const CSSValuePair& other) const;

private:
    explicit CSSValuePair(Ref<CSSValue>&& first, Ref<CSSValue>&& second, ValueSeparator separator)
        : CSSValue(ValuePairClass)
        , m_first(WTFMove(first))
        , m_second(WTFMove(second))
    {
        m_valueSeparator = separator;
    }

    explicit CSSValuePair(Ref<CSSValue>&& first, Ref<CSSValue>&& second, ValueSeparator separator, IdenticalValueEncoding encoding)
        : CSSValue(ValuePairClass)
        , m_encoding(encoding)
        , m_first(WTFMove(first))
        , m_second(WTFMove(second))
    {
        m_valueSeparator = separator;
    }

    IdenticalValueEncoding m_encoding { IdenticalValueEncoding::Coalesce };
    Ref<CSSValue> m_first;
    Ref<CSSValue> m_second;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValuePair, isValuePair())
