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
#include "CSSMathProduct.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSMathInvert.h"
#include "CSSNumericArray.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSMathProduct);

static std::optional<CSSNumericType> multiplyTypes(const CSSNumericType& a, const CSSNumericType& b)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssnumericvalue-multiply-two-types
    if (a.percentHint && b.percentHint && *a.percentHint != *b.percentHint)
        return std::nullopt;

    auto add = [] (auto left, auto right) -> CSSNumericType::BaseTypeStorage {
        if (!left)
            return right;
        if (!right)
            return left;
        return *left + *right;
    };
    
    return { {
        add(a.length, b.length),
        add(a.angle, b.angle),
        add(a.time, b.time),
        add(a.frequency, b.frequency),
        add(a.resolution, b.resolution),
        add(a.flex, b.flex),
        add(a.percent, b.percent),
        a.percentHint ? a.percentHint : b.percentHint
    } };
}

ExceptionOr<Ref<CSSMathProduct>> CSSMathProduct::create(FixedVector<CSSNumberish> numberishes)
{
    return create(WTF::map(WTFMove(numberishes), rectifyNumberish));
}

ExceptionOr<Ref<CSSMathProduct>> CSSMathProduct::create(Vector<Ref<CSSNumericValue>> values)
{
    if (values.isEmpty())
        return Exception { SyntaxError };
    
    std::optional<CSSNumericType> type = values[0]->type();
    for (size_t i = 1; i < values.size(); i++) {
        type = multiplyTypes(*type, values[i]->type());
        if (!type)
            return Exception { TypeError };
    }

    return adoptRef(*new CSSMathProduct(WTFMove(values), WTFMove(*type)));
}

CSSMathProduct::CSSMathProduct(Vector<Ref<CSSNumericValue>> values, CSSNumericType type)
    : CSSMathValue(WTFMove(type))
    , m_values(CSSNumericArray::create(WTFMove(values)))
{
}

void CSSMathProduct::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    // https://drafts.css-houdini.org/css-typed-om/#calc-serialization
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(arguments.contains(SerializationArguments::Nested) ? "(" : "calc(");
    m_values->forEach([&](auto& numericValue, bool first) {
        OptionSet<SerializationArguments> operandSerializationArguments { SerializationArguments::Nested };
        operandSerializationArguments.set(SerializationArguments::WithoutParentheses, arguments.contains(SerializationArguments::WithoutParentheses));
        if (!first) {
            if (auto* mathNegate = dynamicDowncast<CSSMathInvert>(numericValue)) {
                builder.append(" / ");
                mathNegate->value().serialize(builder, operandSerializationArguments);
                return;
            }
            builder.append(" * ");
        }
        numericValue.serialize(builder, operandSerializationArguments);
    });
    if (!arguments.contains(SerializationArguments::WithoutParentheses))
        builder.append(')');
}

} // namespace WebCore

#endif
