/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

template<auto R, typename V> struct DeprecatedToStyle<CSS::LengthRaw<R, V>> {
    using From = CSS::LengthRaw<R, V>;
    using To = typename ToStyleMapping<typename RawToCSSMapping<From>::type>::type;

    template<typename... Rest> auto operator()(const From& from, Rest&&...) -> To
    {
        if (CSS::conversionToCanonicalUnitRequiresConversionData(from.unit))
            return To { 0 };
        return { Style::canonicalize(from, NoConversionDataRequiredToken { }) };
    }
};

template<CSS::NumericRaw RawType> struct DeprecatedToStyle<RawType> {
    using From = RawType;
    using To = typename ToStyleMapping<typename RawToCSSMapping<RawType>::type>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&...) -> To
    {
        return { Style::canonicalize(value, NoConversionDataRequiredToken { }) };
    }
};

template<CSS::NumericRaw RawType> struct DeprecatedToStyle<CSS::UnevaluatedCalc<RawType>> {
    using From = CSS::UnevaluatedCalc<RawType>;
    using To = typename ToStyleMapping<typename RawToCSSMapping<RawType>::type>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&...) -> To
    {
        return { Style::canonicalize(RawType { To::unit, value.evaluateDeprecated() }, NoConversionDataRequiredToken { }) };
    }
};

template<auto R, typename V> struct DeprecatedToStyle<CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>;
    using To = LengthPercentage<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&...) -> To
    {
        // NOTE: Simplification is needed here for the case of the user using the Typed CSSOM
        // to explicitly specify a CSSMath* value for a specified value.

        auto simplifiedCalc = value.simplify();

        // FIXME: This ASSERT and the following extra cases for Category::Length and Category::Percentage
        // should go away once the typed CSSOM learns to set the correct category when creating internal
        // representations of CSSMath* types.

        ASSERT(simplifiedCalc.runtimeCategory() == CSS::Category::LengthPercentage || simplifiedCalc.runtimeCategory() == CSS::Category::Length || simplifiedCalc.runtimeCategory() == CSS::Category::Percentage);

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Length) {
            auto doubleValue = simplifiedCalc.evaluateDeprecated();
            return Style::canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, doubleValue }, NoConversionDataRequiredToken { });
        }

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Percentage) {
            if (simplifiedCalc.rootNodeIsPercentage()) {
                auto doubleValue = simplifiedCalc.evaluateDeprecated();
                return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, NoConversionDataRequiredToken { });
            }
            return typename To::Calc(simplifiedCalc.createCalculationValue(NoConversionDataRequiredToken { }));
        }

        auto simplifiedPrimitiveType = simplifiedCalc.primitiveType();

        if (simplifiedPrimitiveType == CSSUnitType::CSS_PX) {
            auto doubleValue = simplifiedCalc.evaluateDeprecated();
            return canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, doubleValue }, NoConversionDataRequiredToken { });
        }
        if (simplifiedPrimitiveType == CSSUnitType::CSS_PERCENTAGE) {
            auto doubleValue = simplifiedCalc.evaluateDeprecated();
            return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, NoConversionDataRequiredToken { });
        }
        return typename To::Calc(simplifiedCalc.createCalculationValue(NoConversionDataRequiredToken { }));
    }
};

template<CSS::Numeric NumericType> struct DeprecatedToStyle<NumericType> {
    using From = NumericType;
    using To = typename ToStyleMapping<From>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) -> To { return deprecatedToStyle(value, std::forward<Rest>(rest)...); });
    }
};

} // namespace Style
} // namespace WebCore
