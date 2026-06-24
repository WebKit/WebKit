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

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

// Generic implementation of conversion of keywords and numeric values for length wrapper types.

// MARK: Keyword conversion

template<PrimitiveNumericOrKeywordDerived StyleType, typename K>
static auto processKeywordForCSSValueConversion(const K& keyword, CSSValueID valueID, std::optional<StyleType>& result) -> bool
{
    if (valueID == keyword.value) {
        result = StyleType { keyword };
        return true;
    }

    // A few keywords have alternative spellings.
    // FIXME: Find a generic solution to this problem.
    if constexpr (std::same_as<K, CSS::Keyword::MinContent>) {
        if (valueID == CSSValueWebkitMinContent) {
            result = StyleType { keyword };
            return true;
        }
    } else if constexpr (std::same_as<K, CSS::Keyword::MaxContent>) {
        if (valueID == CSSValueWebkitMaxContent) {
            result = StyleType { keyword };
            return true;
        }
    } else if constexpr (std::same_as<K, CSS::Keyword::FitContent>) {
        if (valueID == CSSValueWebkitFitContent) {
            result = StyleType { keyword };
            return true;
        }
    }

    return false;
}

template<PrimitiveNumericOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(const CSSKeywordValue& value, Rest&&...) -> std::optional<StyleType>
{
    auto valueID = value.valueID();

    constexpr auto keywordsTuple = StyleType::Keywords::tuple;

    return std::apply([&](const auto& ...keyword) {
        std::optional<StyleType> result;
        (processKeywordForCSSValueConversion<StyleType>(keyword, valueID, result) || ...);
        return result;
    }, keywordsTuple);
}

template<PrimitiveNumericOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(const CSSToLengthConversionData&, const CSSKeywordValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(value, std::forward<Rest>(rest)...);
}

template<LengthPercentageOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(BuilderState& state, const CSSKeywordValue& value, Rest&&... rest) -> StyleType
{
    using namespace CSS::Literals;

    auto result = convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(value, std::forward<Rest>(rest)...);
    if (result)
        return *result;

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return 0_css_px;
}

// MARK: <length-percentage> conversion

template<LengthPercentageOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    using CSSSpecified = typename StyleType::Specified::CSS;
    using CSSRaw = typename CSSSpecified::Raw;
    using CSSDimensionRaw = typename CSSRaw::Dimension;
    using CSSPercentageRaw = typename CSSRaw::Percentage;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc& calc) -> std::optional<StyleType> {
            return StyleType { toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, conversionData, std::forward<Rest>(rest)...) };
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> std::optional<StyleType> {
            if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                return StyleType { toStyle(CSSPercentageRaw(*unit, raw.value), conversionData, std::forward<Rest>(rest)...) };
            if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                return StyleType { toStyle(CSSDimensionRaw(*unit, raw.value), conversionData, std::forward<Rest>(rest)...), *unit == CSS::LengthUnit::QuirkyEm };

            return std::nullopt;
        }
    );
}

template<LengthPercentageOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
{
    using CSSSpecified = typename StyleType::Specified::CSS;
    using CSSRaw = typename CSSSpecified::Raw;
    using CSSDimensionRaw = typename CSSRaw::Dimension;
    using CSSPercentageRaw = typename CSSRaw::Percentage;

    using namespace CSS::Literals;

    return WTF::switchOn(value,
        [&](const CSSPrimitiveValue::Calc& calc) -> StyleType {
            return StyleType { toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, state, std::forward<Rest>(rest)...) };
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> StyleType {
            if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                return StyleType { toStyle(CSSPercentageRaw(*unit, raw.value), state, std::forward<Rest>(rest)...) };
            if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                return StyleType { toStyle(CSSDimensionRaw(*unit, raw.value), state, std::forward<Rest>(rest)...), *unit == CSS::LengthUnit::QuirkyEm };

            ASSERT_NOT_REACHED();
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        }
    );
}

// MARK: <length-percentage> + keyword conversion

template<LengthPercentageOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(const CSSToLengthConversionData& conversionData, const CSSValue& value, Rest&&... rest) -> std::optional<StyleType>
{
    using namespace CSS::Literals;

    if constexpr (!StyleType::Keywords::count) {
        RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
        if (!primitiveValue)
            return std::nullopt;
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, *primitiveValue, std::forward<Rest>(rest)...);
    } else {
        if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
            return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, *primitiveValue, std::forward<Rest>(rest)...);

        RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value);
        if (!keywordValue)
            return std::nullopt;
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, *keywordValue, std::forward<Rest>(rest)...);
    }
}

template<LengthPercentageOrKeywordDerived StyleType, typename... Rest>
auto convertPrimitiveNumericOrKeywordFromCSSValue(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
{
    using namespace CSS::Literals;

    if constexpr (!StyleType::Keywords::count) {
        RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
        if (!primitiveValue)
            return 0_css_px;
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, *primitiveValue, std::forward<Rest>(rest)...);
    } else {
        if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
            return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, *primitiveValue, std::forward<Rest>(rest)...);

        RefPtr keywordValue = requiredDowncast<CSSKeywordValue>(state, value);
        if (!keywordValue)
            return 0_css_px;
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, *keywordValue, std::forward<Rest>(rest)...);
    }
}

template<LengthPercentageOrKeywordDerived StyleType> struct CSSValueConversion<StyleType> {
    template<typename... Rest> auto operator()(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        using namespace CSS::Literals;

        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, value, std::forward<Rest>(rest)...).value_or(StyleType { 0_css_px });
    }
    template<typename... Rest> auto operator()(const CSSToLengthConversionData& conversionData, const CSSKeywordValue& value, Rest&&... rest) -> StyleType
    {
        using namespace CSS::Literals;

        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, value, std::forward<Rest>(rest)...).value_or(StyleType { 0_css_px });
    }
    template<typename... Rest> auto operator()(const CSSToLengthConversionData& conversionData, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        using namespace CSS::Literals;

        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(conversionData, value, std::forward<Rest>(rest)...).value_or(StyleType { 0_css_px });
    }

    template<typename... Rest> auto operator()(BuilderState& state, const CSSPrimitiveValue& value, Rest&&... rest) -> StyleType
    {
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(BuilderState& state, const CSSKeywordValue& value, Rest&&... rest) -> StyleType
    {
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
    template<typename... Rest> auto operator()(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        return convertPrimitiveNumericOrKeywordFromCSSValue<StyleType>(state, value, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
