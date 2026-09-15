/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "CSSCalcSizeValue.h"
#include "CSSCalcTree+Copy.h"
#include "CSSCalcValue.h"
#include "StyleCalcSizeValue.h"
#include "StylePrimitiveNumericOrKeyword+CSSValueConversion.h"
#include "StyleSizeOrKeyword.h"
#include "StyleUnevaluatedCalcSize.h"

namespace WebCore {
namespace Style {

// MARK: - <calc-size()> conversion

template<typename CSSRaw>
static auto asUnevaluatedCalc(const CSS::CalcSizeCalculation& calculation) -> CSS::UnevaluatedCalc<CSSRaw>
{
    return CSS::UnevaluatedCalc<CSSRaw> { CSSCalc::Value::create(CSS::Category::LengthPercentage, CSS::All, CSSCalc::copy(calculation)) };
}

// Simplification at computed value time happens here, as it does for any math function. The `size`
// keyword survives it as a Calculation::Size leaf, to be resolved at used value time.
template<typename CSSRaw, typename ConversionState>
static auto toStyleCalculation(ConversionState& conversionState, const CSS::CalcSizeCalculation& calculation) -> Calculation::Tree
{
    return protect(asUnevaluatedCalc<CSSRaw>(calculation).createCalculationValue(conversionState).calculation())->copyTree();
}

template<typename CSSRaw, typename ConversionState>
static auto toStyleCalcSizeValue(ConversionState& conversionState, const CSS::CalcSizeParameters& parameters) -> RefPtr<CalcSizeValue>
{
    // Parsing guarantees both arguments match <length-percentage>, so failing to match <length>
    // identifies a percentage.
    auto basisHasPercentage = false;
    auto basis = WTF::switchOn(parameters.basis,
        [&](const CSS::Keyword::Any& keyword) -> std::optional<CalcSizeValue::Basis> {
            return keyword;
        },
        [&](const CSS::CalcSizeCalculation& basis) -> std::optional<CalcSizeValue::Basis> {
            basisHasPercentage = !basis.type.matches(CSS::Category::Length);
            return toStyleCalculation<CSSRaw>(conversionState, basis);
        },
        [&](const UniqueRef<CSS::CalcSizeFunction>& nested) -> std::optional<CalcSizeValue::Basis> {
            RefPtr inner = toStyleCalcSizeValue<CSSRaw>(conversionState, nested->value.parameters);
            if (!inner)
                return { };
            basisHasPercentage = inner->hasPercentage();
            return Ref<CalcSizeValue> { inner.releaseNonNull() };
        },
        [&]<CSSValueID Id>(const Constant<Id>&) -> std::optional<CalcSizeValue::Basis> {
            // The prefixed spellings normalize here, so only canonical keywords are stored.
            if constexpr (Id == CSSValueWebkitMinContent)
                return CSS::Keyword::MinContent { };
            else if constexpr (Id == CSSValueWebkitMaxContent)
                return CSS::Keyword::MaxContent { };
            else if constexpr (Id == CSSValueWebkitFitContent)
                return CSS::Keyword::FitContent { };
            else
                return Constant<Id> { };
        }
    );

    if (!basis)
        return { };

    auto calculationHasPercentage = !parameters.calculation.type.matches(CSS::Category::Length);
    return CalcSizeValue::create(WTF::move(*basis), toStyleCalculation<CSSRaw>(conversionState, parameters.calculation), basisHasPercentage, basisHasPercentage || calculationHasPercentage);
}

template<SizeOrKeywordDerived StyleType, typename ConversionState, typename... Rest>
auto convertCalcSizeForCSSValueConversion(ConversionState& conversionState, const CSS::CalcSizeParameters& parameters, Rest&&...) -> std::optional<StyleType>
{
    using CSSRaw = typename StyleType::Specified::CSS::Raw;

    if (RefPtr calcSize = toStyleCalcSizeValue<CSSRaw>(conversionState, parameters))
        return StyleType { typename StyleType::CalcSize { calcSize.releaseNonNull() } };

    return convertKeywordIDForCSSValueConversion<StyleType>(parameters.basisKeyword());
}

// MARK: - Conversion

// Only the CSSValue overloads need calc-size() handling.
template<SizeOrKeywordDerived StyleType> struct CSSValueConversion<StyleType> : NumericOrKeywordCSSValueConversion<StyleType> {
    using Base = NumericOrKeywordCSSValueConversion<StyleType>;
    using Base::operator();
    using Base::invalidValue;

    template<typename... Rest> auto operator()(const CSSToLengthConversionData& conversionData, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        if (RefPtr calcSizeValue = dynamicDowncast<CSSCalcSizeValue>(value))
            return convertCalcSizeForCSSValueConversion<StyleType>(conversionData, calcSizeValue->calcSize()->parameters, std::forward<Rest>(rest)...).value_or(invalidValue());

        return Base::operator()(conversionData, value, std::forward<Rest>(rest)...);
    }

    template<typename... Rest> auto operator()(BuilderState& state, const CSSValue& value, Rest&&... rest) -> StyleType
    {
        if (RefPtr calcSizeValue = dynamicDowncast<CSSCalcSizeValue>(value)) {
            if (auto result = convertCalcSizeForCSSValueConversion<StyleType>(state, calcSizeValue->calcSize()->parameters, std::forward<Rest>(rest)...))
                return *result;
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return invalidValue();
        }

        return Base::operator()(state, value, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
