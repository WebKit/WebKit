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

#include "CSSCalcValue.h"
#include "CSSMathValue.h"
#include "CSSOMKeywordValue.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSUnevaluatedCalc.h"
#include "CSSUnitValue.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

template<typename StyleType> struct AbsoluteCSSNumericValueConversion;

template<typename StyleType> struct AbsoluteCSSNumericValueConversionInvoker {
    template<typename... Rest> std::optional<StyleType> operator()(const CSSNumericValue& value, Rest&&... rest) const
    {
        return AbsoluteCSSNumericValueConversion<StyleType>{}(value, std::forward<Rest>(rest)...);
    }
};
template<typename StyleType> inline constexpr AbsoluteCSSNumericValueConversionInvoker<StyleType> toAbsoluteStyleFromCSSNumericValue{};

template<auto R, typename T> struct AbsoluteCSSNumericValueConversion<LengthPercentage<R, T>> {
    using StyleType = LengthPercentage<R, T>;
    using CSSType = StyleType::CSS;

    std::optional<StyleType> operator()(const CSSNumericValue& value) const
    {
        if (RefPtr unitValue = dynamicDowncast<CSSUnitValue>(value)) {
            auto lengthPercentageUnit = CSS::toLengthPercentageUnit(unitValue->unitEnum());
            if (!lengthPercentageUnit)
                return std::nullopt;
            if (CSS::conversionToCanonicalUnitRequiresConversionData(*lengthPercentageUnit))
                return std::nullopt;

            return toStyle(typename CSSType::Raw { *lengthPercentageUnit, static_cast<float>(unitValue->value()) }, NoConversionDataRequiredToken { });
        }
        if (RefPtr mathValue = dynamicDowncast<CSSMathValue>(value)) {
            auto calcValue = mathValue->toCSSCalcValue();
            if (!calcValue)
                return std::nullopt;
            if (auto category = calcValue->category(); category != CSS::Category::LengthPercentage && category != CSS::Category::Length && category != CSS::Category::Percentage)
                return std::nullopt;
            if (calcValue->tree().requiresConversionData)
                return std::nullopt;

            return toStyle(typename CSSType::Calc { calcValue.releaseNonNull() }, NoConversionDataRequiredToken { });
        }

        return std::nullopt;
    }
};

} // namespace Style
} // namespace WebCore
