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
#include "CSSMathMin.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSNumericArray.h"
#include "ExceptionOr.h"
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMathMin);

ExceptionOr<Ref<CSSMathMin>> CSSMathMin::create(FixedVector<CSSNumberish>&& numberishes)
{
    return create(WTF::map(WTFMove(numberishes), rectifyNumberish));
}

ExceptionOr<Ref<CSSMathMin>> CSSMathMin::create(Vector<Ref<CSSNumericValue>>&& values)
{
    if (values.isEmpty())
        return Exception { SyntaxError };

    auto type = CSSNumericType::addTypes(values);
    if (!type)
        return Exception { TypeError };

    return adoptRef(*new CSSMathMin(WTFMove(values), WTFMove(*type)));
}

CSSMathMin::CSSMathMin(Vector<Ref<CSSNumericValue>>&& values, CSSNumericType&& type)
    : CSSMathValue(WTFMove(type))
    , m_values(CSSNumericArray::create(WTFMove(values)))
{
}

const CSSNumericArray& CSSMathMin::values() const
{
    return m_values.get();
}

void CSSMathMin::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    // https://drafts.css-houdini.org/css-typed-om/#calc-serialization
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append("min(");
    m_values->forEach([&](auto& numericValue, bool first) {
        if (!first)
            builder.append(", ");
        numericValue.serialize(builder, { SerializationArguments::Nested, SerializationArguments::WithoutParentheses });
    });
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(')');
}

auto CSSMathMin::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto& valuesArray = m_values->array();
    std::optional<SumValue> currentMax = valuesArray[0]->toSumValue();
    if (!currentMax || currentMax->size() != 1)
        return std::nullopt;
    for (size_t i = 1; i < valuesArray.size(); ++i) {
        auto currentValue = valuesArray[i]->toSumValue();
        if (!currentValue
            || currentValue->size() != 1
            || (*currentValue)[0].units != (*currentMax)[0].units)
            return std::nullopt;
        if ((*currentValue)[0].value < (*currentMax)[0].value)
            currentMax = WTFMove(currentValue);
    }
    return currentMax;
}

} // namespace WebCore

#endif
