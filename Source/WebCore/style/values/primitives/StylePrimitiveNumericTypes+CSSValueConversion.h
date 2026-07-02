/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericUnits.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// Generic implementation of conversion for numeric types.

template<Numeric StyleType, typename... Rest>
auto convertNumericFromCSSValue(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
{
    using CSSRaw = typename StyleType::CSS::Raw;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc& calc) -> StyleType {
            return toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, conversionData, std::forward<Rest>(rest)...);
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> StyleType {
            if constexpr (DimensionPercentageNumeric<StyleType>) {
                using CSSDimensionRaw = typename StyleType::Dimension::CSS::Raw;
                using CSSPercentageRaw = typename StyleType::Percentage::CSS::Raw;

                if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSDimensionRaw(*unit, raw.value), conversionData, std::forward<Rest>(rest)...);
                if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSPercentageRaw(*unit, raw.value), conversionData, std::forward<Rest>(rest)...);
            } else if constexpr (StyleType::category == CSS::Category::Integer || StyleType::category == CSS::Category::Number) {
                if (raw.unit == CSSUnitType::CSS_NUMBER || raw.unit == CSSUnitType::CSS_INTEGER)
                    return toStyle(CSSRaw(raw.value), conversionData, std::forward<Rest>(rest)...);
            } else {
                if (auto unit = CSSRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSRaw(*unit, raw.value), conversionData, std::forward<Rest>(rest)...);
            }

            ASSERT_NOT_REACHED();

            if constexpr (DimensionPercentageNumeric<StyleType>) {
                return StyleType { typename StyleType::Dimension { 0 } };
            } else {
                return StyleType { 0 };
            }
        }
    );
}

template<Numeric StyleType, typename... Rest>
auto convertNumericFromCSSValue(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
{
    using CSSRaw = typename StyleType::CSS::Raw;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc& calc) -> StyleType {
            return toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, state, std::forward<Rest>(rest)...);
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> StyleType {
            if constexpr (DimensionPercentageNumeric<StyleType>) {
                using CSSDimensionRaw = typename StyleType::Dimension::CSS::Raw;
                using CSSPercentageRaw = typename StyleType::Percentage::CSS::Raw;

                if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSDimensionRaw(*unit, raw.value), state, std::forward<Rest>(rest)...);
                if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSPercentageRaw(*unit, raw.value), state, std::forward<Rest>(rest)...);
            } else if constexpr (StyleType::category == CSS::Category::Integer || StyleType::category == CSS::Category::Number) {
                if (raw.unit == CSSUnitType::CSS_NUMBER || raw.unit == CSSUnitType::CSS_INTEGER)
                    return toStyle(CSSRaw(raw.value), state, std::forward<Rest>(rest)...);
            } else {
                if (auto unit = CSSRaw::UnitTraits::validate(raw.unit))
                    return toStyle(CSSRaw(*unit, raw.value), state, std::forward<Rest>(rest)...);
            }

            ASSERT_NOT_REACHED();
            state.setCurrentPropertyInvalidAtComputedValueTime();

            if constexpr (DimensionPercentageNumeric<StyleType>) {
                return StyleType { typename StyleType::Dimension { 0 } };
            } else {
                return StyleType { 0 };
            }
        }
    );
}

template<Numeric StyleType, typename... Rest>
auto convertNumericFromCSSValue(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
{
    RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!protectedValue) {
        if constexpr (DimensionPercentageNumeric<StyleType>) {
            return StyleType { typename StyleType::Dimension { 0 } };
        } else {
            return StyleType { 0 };
        }
    }
    return toStyleFromCSSValue<StyleType>(state, *protectedValue, std::forward<Rest>(rest)...);
}

template<Numeric NumericType> struct CSSValueConversion<NumericType> {
    using StyleType = NumericType;

    template<typename... Rest> auto operator()(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        return convertNumericFromCSSValue<StyleType>(conversionData, value, std::forward<Rest>(rest)...);
    }

    template<typename... Rest> auto operator()(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        return convertNumericFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        return convertNumericFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
};

template<auto nR, auto pR, typename V> struct CSSValueConversion<NumberOrPercentage<nR, pR, V>> {
    using StyleType = NumberOrPercentage<nR, pR, V>;

    template<typename... Rest> auto operator()(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        if (value.isPercentage())
            return toStyleFromCSSValue<typename StyleType::Percentage>(state, value, std::forward<Rest>(rest)...);
        return toStyleFromCSSValue<typename StyleType::Number>(state, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!protectedValue)
            return StyleType { typename StyleType::Number { 0 } };
        return toStyleFromCSSValue<StyleType>(state, *protectedValue, std::forward<Rest>(rest)...);
    }
};

template<auto nR, auto pR, typename V> struct CSSValueConversion<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using StyleType = NumberOrPercentageResolvedToNumber<nR, pR, V>;

    template<typename... Rest> auto operator()(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        if (value.isPercentage())
            return toStyleFromCSSValue<typename StyleType::Percentage>(state, value, std::forward<Rest>(rest)...);
        return toStyleFromCSSValue<typename StyleType::Number>(state, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!protectedValue)
            return StyleType { 0 };
        return toStyleFromCSSValue<StyleType>(state, *protectedValue, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
