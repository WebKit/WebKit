/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"
#include "DeprecatedCSSOMPrimitiveValue.h"

namespace WebCore {
namespace CSS {

// MARK: - Conversion from strongly typed `CSS::` value types to `WebCore::DeprecatedCSSOMValue` types.

inline Ref<DeprecatedCSSOMPrimitiveValue> makeDeprecatedCSSOMPrimitiveValueForNumericRaw(NumericRaw auto const& value, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMPrimitiveValue::create(UnconstrainedPrimitiveNumericRaw { toCSSUnitType(value.unit), value.value }, owner);
}

inline Ref<DeprecatedCSSOMPrimitiveValue> makeDeprecatedCSSOMPrimitiveValueForNumericCalc(Calc auto const& value, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMPrimitiveValue::create(value, owner);
}

template<Numeric CSSType>
Ref<DeprecatedCSSOMPrimitiveValue> makeDeprecatedCSSOMPrimitiveValueForNumeric(const CSSType& value, CSSStyleDeclaration& owner)
{
    return WTF::switchOn(value,
        [&](const typename CSSType::Raw& raw) {
            return makeDeprecatedCSSOMPrimitiveValueForNumericRaw(raw, owner);
        },
        [&](const typename CSSType::Calc& calc) {
            return makeDeprecatedCSSOMPrimitiveValueForNumericCalc(calc, owner);
        }
    );
}

template<NumericRaw CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration& owner, const CSSType& value)
    {
        return makeDeprecatedCSSOMPrimitiveValueForNumericRaw(value, owner);
    }
};

template<Calc CSSType> struct DeprecatedCSSOMValueCreation<CSSType> {
    Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration& owner, const CSSType& value)
    {
        return makeDeprecatedCSSOMPrimitiveValueForNumericCalc(value, owner);
    }
};

} // namespace CSS
} // namespace WebCore
