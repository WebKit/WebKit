/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "CSSMathInvert.h"

#include "CSSNumericValue.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMathInvert);

Ref<CSSMathInvert> CSSMathInvert::create(CSSNumberish&& numberish)
{
    return adoptRef(*new CSSMathInvert(WTFMove(numberish)));
}

static CSSNumericType negatedType(const CSSNumberish& numberish)
{
    // https://drafts.css-houdini.org/css-typed-om/#type-of-a-cssmathvalue
    return WTF::switchOn(numberish,
        [] (double) { return CSSNumericType(); },
        [] (const RefPtr<CSSNumericValue>& value) {
            if (!value)
                return CSSNumericType();
            CSSNumericType type = value->type();
            auto negate = [] (auto& optional) {
                if (optional)
                    optional = *optional * -1;
            };
            negate(type.length);
            negate(type.angle);
            negate(type.time);
            negate(type.frequency);
            negate(type.resolution);
            negate(type.flex);
            negate(type.percent);
            return type;
        }
    );
}

CSSMathInvert::CSSMathInvert(CSSNumberish&& numberish)
    : CSSMathValue(negatedType(numberish))
    , m_value(rectifyNumberish(WTFMove(numberish)))
{
}

void CSSMathInvert::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    // https://drafts.css-houdini.org/css-typed-om/#calc-serialization
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(arguments.contains(SerializationArguments::Nested) ? "(" : "calc(");
    builder.append("1 / ");
    m_value->serialize(builder, arguments);
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(')');
}

auto CSSMathInvert::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto values = m_value->toSumValue();
    if (!values)
        return std::nullopt;
    if (values->size() != 1)
        return std::nullopt;
    auto& value = (*values)[0];
    if (!value.value)
        return std::nullopt;
    value.value = 1.0 / value.value;

    UnitMap negatedExponents;
    for (auto& pair : value.units)
        negatedExponents.add(pair.key, -1 * pair.value);
    value.units = WTFMove(negatedExponents);

    return values;
}

bool CSSMathInvert::equals(const CSSNumericValue& other) const
{
    // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
    auto* otherInvert = dynamicDowncast<CSSMathInvert>(other);
    if (!otherInvert)
        return false;
    return m_value->equals(otherInvert->value());
}

} // namespace WebCore
