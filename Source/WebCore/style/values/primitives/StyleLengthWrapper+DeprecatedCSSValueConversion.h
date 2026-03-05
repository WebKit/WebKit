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

#include "Element.h"
#include "StyleLengthWrapper+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Deprecated Conversions

std::optional<CSSToLengthConversionData> deprecatedLengthConversionCreateCSSToLengthConversionData(RefPtr<Element>);

template<LengthWrapperBaseDerived T> struct DeprecatedCSSValueConversion<T> {
    auto operator()(const RefPtr<Element>& element, const CSSPrimitiveValue& primitiveValue) -> std::optional<T>
    {
        using namespace CSS::Literals;

        auto convertLengthPercentage = [&] -> std::optional<T> {
            auto conversionData = deprecatedLengthConversionCreateCSSToLengthConversionData(element);
            if (!conversionData) {
                if (primitiveValue.isCalculated())
                    return std::nullopt;

                if (primitiveValue.isPx()) {
                    return T {
                        typename T::Fixed {
                            CSS::clampToRange<T::Fixed::range, float>(primitiveValue.resolveAsLengthNoConversionDataRequired(), minValueForCssLength, maxValueForCssLength),
                        },
                        primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EM
                    };
                }

                if (primitiveValue.isPercentage()) {
                    return T {
                        typename T::Percentage {
                            CSS::clampToRange<T::Percentage::range, float>(primitiveValue.resolveAsPercentageNoConversionDataRequired()),
                        }
                    };
                }

                return std::nullopt;
            }

            if (primitiveValue.isLength()) {
                return T {
                    typename T::Fixed {
                        CSS::clampToRange<T::Fixed::range, float>(primitiveValue.resolveAsLength(*conversionData), minValueForCssLength, maxValueForCssLength),
                    },
                    primitiveValue.primitiveType() == CSSUnitType::CSS_QUIRKY_EM
                };
            }

            if (primitiveValue.isPercentage()) {
                return T {
                    typename T::Percentage {
                        CSS::clampToRange<T::Percentage::range, float>(primitiveValue.resolveAsPercentage(*conversionData)),
                    }
                };
            }

            if (primitiveValue.isCalculatedPercentageWithLength()) {
                return T {
                    typename T::Calc {
                        protect(primitiveValue.cssCalcValue())->createCalculationValue(*conversionData, CSSCalcSymbolTable { })
                    }
                };
            }

            return std::nullopt;
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
                (CSSValueConversion<T>::processKeyword(keyword, valueID, result) || ...);
                return result;
            }, keywordsTuple);

            return result;
        }
    }

    auto operator()(const RefPtr<Element>& element, const CSSValue& value) -> std::optional<T>
    {
        using namespace CSS::Literals;

        RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
        if (!primitiveValue)
            return std::nullopt;

        return this->operator()(element, *primitiveValue);
    }
};

} // namespace Style
} // namespace WebCore
