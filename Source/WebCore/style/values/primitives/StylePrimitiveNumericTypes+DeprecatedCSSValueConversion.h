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

#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+DeprecatedConversions.h"

namespace WebCore {
namespace Style {

template<Numeric StyleType, typename... Rest>
auto deprecatedConvertNumericFromCSSValue(const CSSPrimitiveValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    using CSSRaw = typename StyleType::CSS::Raw;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc& calc) -> std::optional<StyleType> {
            if constexpr (DimensionPercentageNumeric<StyleType>)
                return std::nullopt;
            else
                return deprecatedToStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, std::forward<Rest>(rest)...);
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> std::optional<StyleType> {
            if constexpr (DimensionPercentageNumeric<StyleType>) {
                using CSSDimensionRaw = typename StyleType::Dimension::CSS::Raw;
                using CSSPercentageRaw = typename StyleType::Percentage::CSS::Raw;

                if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                    return deprecatedToStyle(CSSDimensionRaw(*unit, raw.value), std::forward<Rest>(rest)...);
                if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                    return deprecatedToStyle(CSSPercentageRaw(*unit, raw.value), std::forward<Rest>(rest)...);
            } else if constexpr (StyleType::category == CSS::Category::Integer || StyleType::category == CSS::Category::Number) {
                if (raw.unit == CSSUnitType::CSS_NUMBER || raw.unit == CSSUnitType::CSS_INTEGER)
                    return deprecatedToStyle(CSSRaw(raw.value), std::forward<Rest>(rest)...);
            } else {
                if (auto unit = CSSRaw::UnitTraits::validate(raw.unit))
                    return deprecatedToStyle(CSSRaw(*unit, raw.value), std::forward<Rest>(rest)...);
            }

            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    );
}

template<typename StyleType, typename... Rest>
auto deprecatedConvertNumericFromCSSValue(const CSSValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    RefPtr protectedValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!protectedValue)
        return std::nullopt;
    return deprecatedToStyleFromCSSValue<StyleType>(*protectedValue, std::forward<Rest>(rest)...);
}

template<Numeric NumericType> struct DeprecatedCSSValueConversion<NumericType> {
    using StyleType = NumericType;

    template<typename... Rest> auto operator()(const CSSPrimitiveValue& value, Rest&&... rest) -> std::optional<StyleType>
    {
        return deprecatedConvertNumericFromCSSValue<StyleType>(value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(const CSSValue& value, Rest&&... rest) -> std::optional<StyleType>
    {
        return deprecatedConvertNumericFromCSSValue<StyleType>(value, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
