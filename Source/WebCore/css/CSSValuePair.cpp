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

#include "config.h"
#include "CSSValuePair.h"
#include <wtf/Hasher.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSValuePair::CSSValuePair(ValueSeparator separator, Ref<CSSValue> first, Ref<CSSValue> second, IdenticalValueSerialization serialization)
    : CSSValue(ValuePairClass)
    , m_coalesceIdenticalValues(serialization != IdenticalValueSerialization::DoNotCoalesce)
    , m_first(WTFMove(first))
    , m_second(WTFMove(second))
{
    m_valueSeparator = separator;
}

Ref<CSSValuePair> CSSValuePair::create(Ref<CSSValue> first, Ref<CSSValue> second)
{
    return adoptRef(*new CSSValuePair(SpaceSeparator, WTFMove(first), WTFMove(second), IdenticalValueSerialization::Coalesce));
}

Ref<CSSValuePair> CSSValuePair::createSlashSeparated(Ref<CSSValue> first, Ref<CSSValue> second)
{
    return adoptRef(*new CSSValuePair(SlashSeparator, WTFMove(first), WTFMove(second), IdenticalValueSerialization::DoNotCoalesce));
}

Ref<CSSValuePair> CSSValuePair::createNoncoalescing(Ref<CSSValue> first, Ref<CSSValue> second)
{
    return adoptRef(*new CSSValuePair(SpaceSeparator, WTFMove(first), WTFMove(second), IdenticalValueSerialization::DoNotCoalesce));
}

bool CSSValuePair::canBeCoalesced() const
{
    return m_coalesceIdenticalValues && m_first->equals(m_second);
}

String CSSValuePair::customCSSText() const
{
    String first = m_first->cssText();
    String second = m_second->cssText();
    if (m_coalesceIdenticalValues && first == second)
        return first;
    return makeString(first, separatorCSSText(), second);
}

bool CSSValuePair::equals(const CSSValuePair& other) const
{
    return m_valueSeparator == other.m_valueSeparator
        && m_coalesceIdenticalValues == other.m_coalesceIdenticalValues
        && m_first->equals(other.m_first)
        && m_second->equals(other.m_second);
}

bool CSSValuePair::addDerivedHash(Hasher& hasher) const
{
    add(hasher, m_valueSeparator, m_coalesceIdenticalValues);
    return m_first->addHash(hasher) && m_second->addHash(hasher);
}

} // namespace WebCore
