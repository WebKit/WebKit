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

using namespace CSS::Literals;

template<auto R, typename V> struct CSSValueConversion<Integer<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Integer<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsNumber<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Integer<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_integer;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsNumber<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Number<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Number<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsNumber<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Number<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_number;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsNumber<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Percentage<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Percentage<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsPercentage<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Percentage<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_percentage;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsPercentage<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Angle<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Angle<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsAngle<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Angle<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_deg;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsAngle<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Length<R, V>> {
    static auto selectConversionData(BuilderState& builderState) -> CSSToLengthConversionData
    {
        if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Default) {
            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : builderState.cssToLengthConversionData();
        } else if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(builderState))
                return builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f, R.zoomOptions);

            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f, R.zoomOptions)
                : builderState.cssToLengthConversionData();
        }
    }
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Length<R, V>
    {
        Ref protectedValue = value;
        auto conversionData = selectConversionData(builderState);
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsLength<V>(conversionData)) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Length<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_px;

        auto conversionData = selectConversionData(builderState);
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsLength<V>(conversionData)) };
    }
    auto operator()(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value) -> Length<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsLength<V>(conversionData)) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Time<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Time<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsTime<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Time<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_s;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsTime<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Resolution<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Resolution<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsResolution<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Resolution<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_dppx;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsResolution<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<Flex<R, V>> {
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> Flex<R, V>
    {
        Ref protectedValue = value;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsFlex<V>(builderState.cssToLengthConversionData())) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> Flex<R, V>
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_fr;
        return { CSS::clampToRange<R, V>(protectedValue->resolveAsFlex<V>(builderState.cssToLengthConversionData())) };
    }
};

template<auto R, typename V> struct CSSValueConversion<LengthPercentage<R, V>> {
    using StyleType = LengthPercentage<R, V>;

    static auto selectConversionData(BuilderState& builderState) -> CSSToLengthConversionData
    {
        if constexpr (StyleType::Dimension::range.zoomOptions == CSS::RangeZoomOptions::Default) {
            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : builderState.cssToLengthConversionData();
        } else if constexpr (LengthPercentage<R, V>::Dimension::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(builderState))
                return builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f);

            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : builderState.cssToLengthConversionData();
        }
    }
    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> StyleType
    {
        Ref protectedValue = value;
        auto conversionData = selectConversionData(builderState);
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<R, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        if (protectedValue->isCalculatedPercentageWithLength())
            return typename StyleType::Calc { protect(protectedValue->cssCalcValue())->createCalculationValue(conversionData, CSSCalcSymbolTable { }) };
        return typename StyleType::Dimension { CSS::clampToRange<R, V>(protectedValue->resolveAsLength<V>(conversionData)) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> StyleType
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_px;
        auto conversionData = selectConversionData(builderState);
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<R, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        if (protectedValue->isCalculatedPercentageWithLength())
            return typename StyleType::Calc { protect(protectedValue->cssCalcValue())->createCalculationValue(conversionData, CSSCalcSymbolTable { }) };
        return typename StyleType::Dimension { CSS::clampToRange<R, V>(protectedValue->resolveAsLength<V>(conversionData)) };
    }
};

template<auto nR, auto pR, typename V> struct CSSValueConversion<NumberOrPercentage<nR, pR, V>> {
    using StyleType = NumberOrPercentage<nR, pR, V>;

    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> StyleType
    {
        Ref protectedValue = value;

        auto& conversionData = builderState.cssToLengthConversionData();
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<pR, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        return typename StyleType::Number { CSS::clampToRange<nR, V>(protectedValue->resolveAsNumber<V>(conversionData)) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> StyleType
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_number;

        auto& conversionData = builderState.cssToLengthConversionData();
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<pR, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        return typename StyleType::Number { CSS::clampToRange<nR, V>(protectedValue->resolveAsNumber<V>(conversionData)) };
    }
};

template<auto nR, auto pR, typename V> struct CSSValueConversion<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using StyleType = NumberOrPercentageResolvedToNumber<nR, pR, V>;

    auto operator()(BuilderState& builderState, const CSSPrimitiveValue& value) -> StyleType
    {
        Ref protectedValue = value;

        auto& conversionData = builderState.cssToLengthConversionData();
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<pR, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        return typename StyleType::Number { CSS::clampToRange<nR, V>(protectedValue->resolveAsNumber<V>(conversionData)) };
    }
    auto operator()(BuilderState& builderState, const CSSValue& value) -> StyleType
    {
        RefPtr protectedValue = requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!protectedValue)
            return 0_css_number;

        auto& conversionData = builderState.cssToLengthConversionData();
        if (protectedValue->isPercentage())
            return typename StyleType::Percentage { CSS::clampToRange<pR, V>(protectedValue->resolveAsPercentage<V>(conversionData)) };
        return typename StyleType::Number { CSS::clampToRange<nR, V>(protectedValue->resolveAsNumber<V>(conversionData)) };
    }
};

} // namespace Style
} // namespace WebCore
