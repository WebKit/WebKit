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
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

template<LengthWrapperBaseDerived T> struct CSSValueConversion<T> {
    template<typename K>
    static auto processKeyword(const K& keyword, CSSValueID valueID, std::optional<T>& result) -> bool
    {
        if (valueID == keyword.value) {
            result = T { keyword };
            return true;
        }

        // A few keywords have alternative spellings.
        // FIXME: Find a generic solution to this problem.
        if constexpr (std::same_as<K, CSS::Keyword::MinContent>) {
            if (valueID == CSSValueWebkitMinContent) {
                result = T { keyword };
                return true;
            }
        } else if constexpr (std::same_as<K, CSS::Keyword::MaxContent>) {
            if (valueID == CSSValueWebkitMaxContent) {
                result = T { keyword };
                return true;
            }
        } else if constexpr (std::same_as<K, CSS::Keyword::FitContent>) {
            if (valueID == CSSValueWebkitFitContent) {
                result = T { keyword };
                return true;
            }
        }

        return false;
    }

    static auto selectConversionData(BuilderState& builderState) -> CSSToLengthConversionData
    {
        if constexpr (T::Fixed::range.zoomOptions == CSS::RangeZoomOptions::Default) {
            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : builderState.cssToLengthConversionData();
        } else if constexpr (T::Fixed::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(builderState))
                return builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f, T::Fixed::range.zoomOptions);

            return builderState.useSVGZoomRulesForLength()
                ? builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : builderState.cssToLengthConversionData();
        }
    }

    auto operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue) -> T
    {
        using namespace CSS::Literals;

        auto convertLengthPercentage = [&] -> T {
            auto conversionData = selectConversionData(state);

            if (primitiveValue.isLength()) {
                return T {
                    typename T::Fixed {
                        CSS::clampToRange<T::Fixed::range, float>(primitiveValue.resolveAsLength(conversionData), minValueForCssLength, maxValueForCssLength),
                    },
                    primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EM
                };
            }

            if (primitiveValue.isPercentage()) {
                return T {
                    typename T::Percentage {
                        CSS::clampToRange<T::Percentage::range, float>(primitiveValue.resolveAsPercentage(conversionData)),
                    }
                };
            }

            if (primitiveValue.isCalculatedPercentageWithLength()) {
                return T {
                    typename T::Calc {
                        protect(primitiveValue.cssCalcValue())->createCalculationValue(conversionData, CSSCalcSymbolTable { })
                    }
                };
            }

            ASSERT_NOT_REACHED();
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        };

        if constexpr (!T::Keywords::count)
            return convertLengthPercentage();
        else {
            auto valueID = primitiveValue.valueID();
            if (valueID == CSSValueInvalid)
                return convertLengthPercentage();

            constexpr auto keywordsTuple = T::Keywords::tuple;

            auto result = std::apply([&](const auto& ...keyword) {
                std::optional<T> result;
                (processKeyword(keyword, valueID, result) || ...);
                return result;
            }, keywordsTuple);

            if (result)
                return *result;

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
