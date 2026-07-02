/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+DeprecatedConversions.h"

namespace WebCore {
namespace Style {

// MARK: - Deprecated Conversions

template<LengthWrapperBaseDerived StyleType, typename... Rest>
auto deprecatedConvertLengthWrapperFromCSSValue(const CSSPrimitiveValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    using CSSSpecified = typename StyleType::Specified::CSS;
    using CSSRaw = typename CSSSpecified::Raw;
    using CSSDimensionRaw = typename CSSRaw::Dimension;
    using CSSPercentageRaw = typename CSSRaw::Percentage;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc&) -> std::optional<StyleType> {
            return std::nullopt;
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> std::optional<StyleType> {
            if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                return StyleType { deprecatedToStyle(CSSPercentageRaw(*unit, raw.value), std::forward<Rest>(rest)...) };
            if (raw.unit == CSSUnitType::CSS_PX)
                return StyleType { deprecatedToStyle(CSSDimensionRaw(CSS::LengthUnit::Px, raw.value), std::forward<Rest>(rest)...) };

            return std::nullopt;
        }
    );
}

template<LengthWrapperBaseDerived StyleType, typename... Rest>
auto deprecatedConvertLengthWrapperFromCSSValue(const CSSValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    using namespace CSS::Literals;

    if constexpr (!StyleType::Keywords::count) {
        RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
        if (!primitiveValue)
            return std::nullopt;
        return deprecatedConvertLengthWrapperFromCSSValue<StyleType>(*primitiveValue, std::forward<Rest>(rest)...);
    } else {
        if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
            return deprecatedConvertLengthWrapperFromCSSValue<StyleType>(*primitiveValue, std::forward<Rest>(rest)...);

        RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value);
        if (!keywordValue)
            return std::nullopt;

        // NOTE: The non-deprecated `convertLengthWrapperFromCSSValue` can be used for keywords, since they never require conversion data.
        return convertLengthWrapperFromCSSValue<StyleType>(*keywordValue, std::forward<Rest>(rest)...);
    }
}

template<LengthWrapperBaseDerived StyleType> struct DeprecatedCSSValueConversion<StyleType> {
    template<typename... Rest> auto operator()(const CSSValue& value, Rest&&... rest) -> std::optional<StyleType>
    {
        return deprecatedConvertLengthWrapperFromCSSValue<StyleType>(value, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
