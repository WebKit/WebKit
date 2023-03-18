/**
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

#include "config.h"
#include "CSSValuePair.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

CSSValuePair::CSSValuePair(Ref<CSSValue>&& first, Ref<CSSValue>&& second, IdenticalValueSerialization serialization)
    : CSSValue(Type::ValuePair)
    , m_first(WTFMove(first))
    , m_second(WTFMove(second))
{
    m_isNoncoalescing = serialization == IdenticalValueSerialization::DoNotCoalesce;

    // One bit is used for the "has scalar in pointer" flag. There are 31 available for the scalar on 32-bit platforms. Check that we fit.
    static_assert(SecondValueIDShift + IdentBits <= 31);
}

Ref<CSSValuePair> CSSValuePair::create(Ref<CSSValue>&& first, Ref<CSSValue>&& second)
{
    if (first->isValueID() && second->isValueID())
        return create(first->valueID(), second->valueID());
    return adoptRef(*new CSSValuePair(WTFMove(first), WTFMove(second), IdenticalValueSerialization::Coalesce));
}

Ref<CSSValuePair> CSSValuePair::createNoncoalescing(Ref<CSSValue>&& first, Ref<CSSValue>&& second)
{
    if (first->isValueID() && second->isValueID())
        return *reinterpret_cast<CSSValuePair*>(typeScalar(Type::ValuePair) | IsNoncoalescingBit | static_cast<uintptr_t>(first->valueID()) << FirstValueIDShift | static_cast<uintptr_t>(second->valueID()) << SecondValueIDShift);
    return adoptRef(*new CSSValuePair(WTFMove(first), WTFMove(second), IdenticalValueSerialization::DoNotCoalesce));
}

String CSSValuePair::customCSSText() const
{
    String first = this->first().cssText();
    String second = this->second().cssText();
    if (!isNoncoalescing() && first == second)
        return first;
    return makeString(first, ' ', second);
}

bool CSSValuePair::equals(const CSSValuePair& other) const
{
    if (this == &other)
        return true;
    if (eitherHasScalarInPointer(*this, other))
        return false;
    auto& a = *opaque(this);
    auto& b = *opaque(&other);
    return a.m_valueSeparator == b.m_valueSeparator
        && a.m_isNoncoalescing == b.m_isNoncoalescing
        && a.m_first->equals(b.m_first)
        && a.m_second->equals(b.m_second);
}

} // namespace WebCore
