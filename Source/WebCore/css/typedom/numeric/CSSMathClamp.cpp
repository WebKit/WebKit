/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CSSMathClamp.h"

#include "CSSNumericValue.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMathClamp);

ExceptionOr<Ref<CSSMathClamp>> CSSMathClamp::create(CSSNumberish&& lower, CSSNumberish&& value, CSSNumberish&& upper)
{
    auto rectifiedLower = rectifyNumberish(WTFMove(lower));
    auto rectifiedValue = rectifyNumberish(WTFMove(value));
    auto rectifiedUpper = rectifyNumberish(WTFMove(upper));

    auto addedType = CSSNumericType::addTypes(rectifiedLower->type(), rectifiedValue->type());
    if (!addedType)
        return Exception { TypeError };
    addedType = CSSNumericType::addTypes(*addedType, rectifiedUpper->type());
    if (!addedType)
        return Exception { TypeError };

    return adoptRef(*new CSSMathClamp(WTFMove(*addedType), WTFMove(rectifiedLower), WTFMove(rectifiedValue), WTFMove(rectifiedUpper)));
}

CSSMathClamp::CSSMathClamp(CSSNumericType&& type, Ref<CSSNumericValue>&& lower, Ref<CSSNumericValue>&& value, Ref<CSSNumericValue>&& upper)
    : CSSMathValue(WTFMove(type))
    , m_lower(WTFMove(lower))
    , m_value(WTFMove(value))
    , m_upper(WTFMove(upper))
{
}

void CSSMathClamp::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    // https://drafts.css-houdini.org/css-typed-om/#calc-serialization
    OptionSet<SerializationArguments> serializationArguments { SerializationArguments::Nested, SerializationArguments::WithoutParentheses };
    builder.append("clamp("_s);
    m_lower->serialize(builder, serializationArguments);
    builder.append(", "_s);
    m_value->serialize(builder, serializationArguments);
    builder.append(", "_s);
    m_upper->serialize(builder, serializationArguments);
    builder.append(')');
}

auto CSSMathClamp::toSumValue() const -> std::optional<SumValue>
{
    auto validateSumValue = [](const std::optional<SumValue>& sumValue, const UnitMap* expectedUnits) {
        return sumValue && sumValue->size() == 1 && (!expectedUnits || *expectedUnits == sumValue->first().units);
    };

    auto lower = m_lower->toSumValue();
    if (!validateSumValue(lower, nullptr))
        return std::nullopt;
    auto value = m_value->toSumValue();
    if (!validateSumValue(value, &lower->first().units))
        return std::nullopt;
    auto upper = m_upper->toSumValue();
    if (!validateSumValue(upper, &lower->first().units))
        return std::nullopt;

    value->first().value = std::max(lower->first().value, std::min(value->first().value, upper->first().value));
    return value;
}

bool CSSMathClamp::equals(const CSSNumericValue& other) const
{
    // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
    auto* otherClamp = dynamicDowncast<CSSMathClamp>(other);
    if (!otherClamp)
        return false;
    return m_lower->equals(otherClamp->m_lower)
        && m_value->equals(otherClamp->m_value)
        && m_upper->equals(otherClamp->m_upper);
}

} // namespace WebCore
