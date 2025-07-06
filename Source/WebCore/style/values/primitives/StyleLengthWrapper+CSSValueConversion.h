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

#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

template<LengthWrapperBaseDerived T> struct CSSValueConversion<T> {
    auto operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue) -> T
    {
        using namespace CSS::Literals;

        auto convertLengthPercentage = [&] -> T {
            auto conversionData = state.useSVGZoomRulesForLength()
                ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : state.cssToLengthConversionData();

            if (primitiveValue.isLength()) {
                return T { WebCore::Length {
                    CSS::clampToRange<T::Fixed::range, float>(primitiveValue.resolveAsLength(conversionData), minValueForCssLength, maxValueForCssLength),
                    LengthType::Fixed,
                    primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EM
                } };
            }

            if (primitiveValue.isPercentage()) {
                return T { WebCore::Length {
                    CSS::clampToRange<T::Percentage::range, float>(primitiveValue.resolveAsPercentage(conversionData)),
                    LengthType::Percent
                } };
            }

            if (primitiveValue.isCalculatedPercentageWithLength()) {
                return T { WebCore::Length {
                    primitiveValue.protectedCssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })
                } };
            }

            ASSERT_NOT_REACHED();
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        };

        if constexpr (!T::Keywords::count)
            return convertLengthPercentage();
        else {
            switch (primitiveValue.valueID()) {
            case CSSValueInvalid:
                return convertLengthPercentage();
            case CSSValueIntrinsic:
                if constexpr (T::SupportsIntrinsic)
                    return T { WebCore::Length(LengthType::Intrinsic) };
                else
                    break;
            case CSSValueMinIntrinsic:
                if constexpr (T::SupportsMinIntrinsic)
                    return T { WebCore::Length(LengthType::MinIntrinsic) };
                else
                    break;
            case CSSValueMinContent:
            case CSSValueWebkitMinContent:
                if constexpr (T::SupportsMinContent)
                    return T { WebCore::Length(LengthType::MinContent) };
                else
                    break;
            case CSSValueMaxContent:
            case CSSValueWebkitMaxContent:
                if constexpr (T::SupportsMaxContent)
                    return T { WebCore::Length(LengthType::MaxContent) };
                else
                    break;
            case CSSValueWebkitFillAvailable:
                if constexpr (T::SupportsWebkitFillAvailable)
                    return T { WebCore::Length(LengthType::FillAvailable) };
                else
                    break;
            case CSSValueFitContent:
            case CSSValueWebkitFitContent:
                if constexpr (T::SupportsFitContent)
                    return T { WebCore::Length(LengthType::FitContent) };
                else
                    break;
            case CSSValueAuto:
                if constexpr (T::SupportsAuto)
                    return T { WebCore::Length(LengthType::Auto) };
                else
                    break;
            case CSSValueContent:
                if constexpr (T::SupportsContent)
                    return T { WebCore::Length(LengthType::Content) };
                else
                    break;
            case CSSValueNormal:
                if constexpr (T::SupportsNormal)
                    return T { WebCore::Length(LengthType::Normal) };
                else
                    break;
            case CSSValueNone:
                if constexpr (T::SupportsNone)
                    return T { WebCore::Length(LengthType::Undefined) };
                else
                    break;
            default:
                break;
            }

            ASSERT_NOT_REACHED();
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        }
    }

    auto operator()(BuilderState& state, const CSSValue& value) -> T
    {
        using namespace CSS::Literals;

        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue)
            return 0_css_px;

        return this->operator()(state, *primitiveValue);
    }
};

} // namespace Style
} // namespace WebCore
