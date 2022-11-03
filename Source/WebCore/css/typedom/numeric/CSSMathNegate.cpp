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
#include "CSSMathNegate.h"

#include "CSSCalcNegateNode.h"
#include "CSSNumericValue.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMathNegate);

static CSSNumericType copyType(const CSSNumberish& numberish)
{
    return WTF::switchOn(numberish,
        [] (double) { return CSSNumericType(); },
        [] (const RefPtr<CSSNumericValue>& value) {
            if (!value)
                return CSSNumericType();
            return value->type();
        }
    );
}

CSSMathNegate::CSSMathNegate(CSSNumberish&& numberish)
    : CSSMathValue(copyType(numberish))
    , m_value(rectifyNumberish(WTFMove(numberish)))
{
}

void CSSMathNegate::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    // https://drafts.css-houdini.org/css-typed-om/#calc-serialization
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(arguments.contains(SerializationArguments::Nested) ? "(" : "calc(");
    builder.append('-');
    m_value->serialize(builder, arguments);
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(')');
}

auto CSSMathNegate::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto values = m_value->toSumValue();
    if (!values)
        return std::nullopt;
    for (auto& value : *values)
        value.value = value.value * -1;
    return values;
}

bool CSSMathNegate::equals(const CSSNumericValue& other) const
{
    // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
    auto* otherNegate = dynamicDowncast<CSSMathNegate>(other);
    if (!otherNegate)
        return false;
    return m_value->equals(otherNegate->value());
}

RefPtr<CSSCalcExpressionNode> CSSMathNegate::toCalcExpressionNode() const
{
    if (auto value = m_value->toCalcExpressionNode())
        return CSSCalcNegateNode::create(value.releaseNonNull());
    return nullptr;
}

} // namespace WebCore
