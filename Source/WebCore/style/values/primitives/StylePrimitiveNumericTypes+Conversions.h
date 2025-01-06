/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes+Canonicalization.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSSymbol.h"
#include "CSSToLengthConversionData.h"
#include "FloatConversion.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// MARK: Conversion Data specialization

template<typename T> struct ConversionDataSpecializer {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        return state.cssToLengthConversionData();
    }
};

template<auto R> struct ConversionDataSpecializer<CSS::LengthRaw<R>> {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        return state.useSVGZoomRulesForLength()
             ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
             : state.cssToLengthConversionData();
    }
};

template<typename T> CSSToLengthConversionData conversionData(const BuilderState& state)
{
    return ConversionDataSpecializer<T>{}(state);
}

// MARK: - Type maps

// MARK: Raw -> CSS

template<typename> struct RawToCSSMapping;
template<auto R, typename T> struct RawToCSSMapping<CSS::IntegerRaw<R, T>> { using type = CSS::Integer<R, T>; };
template<auto R> struct RawToCSSMapping<CSS::NumberRaw<R>>                 { using type = CSS::Number<R>; };
template<auto R> struct RawToCSSMapping<CSS::PercentageRaw<R>>             { using type = CSS::Percentage<R>; };
template<auto R> struct RawToCSSMapping<CSS::AngleRaw<R>>                  { using type = CSS::Angle<R>; };
template<auto R> struct RawToCSSMapping<CSS::LengthRaw<R>>                 { using type = CSS::Length<R>; };
template<auto R> struct RawToCSSMapping<CSS::TimeRaw<R>>                   { using type = CSS::Time<R>; };
template<auto R> struct RawToCSSMapping<CSS::FrequencyRaw<R>>              { using type = CSS::Frequency<R>; };
template<auto R> struct RawToCSSMapping<CSS::ResolutionRaw<R>>             { using type = CSS::Resolution<R>; };
template<auto R> struct RawToCSSMapping<CSS::FlexRaw<R>>                   { using type = CSS::Flex<R>; };
template<auto R> struct RawToCSSMapping<CSS::AnglePercentageRaw<R>>        { using type = CSS::AnglePercentage<R>; };
template<auto R> struct RawToCSSMapping<CSS::LengthPercentageRaw<R>>       { using type = CSS::LengthPercentage<R>; };

// MARK: CSS -> Raw

template<CSS::Numeric T> struct CSSToRawMapping {
    using type = typename T::Raw;
};

// MARK: - Raw canonicalization

// MARK: Length

double canonicalizeLength(double, CSS::LengthUnit, NoConversionDataRequiredToken);
double canonicalizeLength(double, CSS::LengthUnit, const CSSToLengthConversionData&);
float clampLengthToAllowedLimits(double);
float canonicalizeAndClampLength(double, CSS::LengthUnit, NoConversionDataRequiredToken);
float canonicalizeAndClampLength(double, CSS::LengthUnit, const CSSToLengthConversionData&);

template<auto R, typename V, typename... Rest> constexpr Integer<R, V> canonicalize(const CSS::IntegerRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { roundForImpreciseConversion<V>(raw.value) };
}

template<auto R, typename V, typename... Rest> constexpr Integer<R, V> canonicalize(const CSS::IntegerRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> constexpr Number<R> canonicalize(const CSS::NumberRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { raw.value };
}

template<auto R, typename... Rest> constexpr Number<R> canonicalize(const CSS::NumberRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> constexpr Percentage<R> canonicalize(const CSS::PercentageRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { raw.value };
}

template<auto R, typename... Rest> constexpr Percentage<R> canonicalize(const CSS::PercentageRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> Angle<R> canonicalize(const CSS::AngleRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { CSS::canonicalize(raw) };
}

template<auto R, typename... Rest> Angle<R> canonicalize(const CSS::AngleRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> Length<R> canonicalize(const CSS::LengthRaw<R>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    ASSERT(!requiresConversionData(raw));

    return { canonicalizeAndClampLength(raw.value, raw.unit, token, std::forward<Rest>(rest)...) };
}

template<auto R, typename... Rest> Length<R> canonicalize(const CSS::LengthRaw<R>& raw, const CSSToLengthConversionData& conversionData, Rest&&...)
{
    ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    return { canonicalizeAndClampLength(raw.value, raw.unit, conversionData) };
}

template<auto R, typename... Rest> Time<R> canonicalize(const CSS::TimeRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { CSS::canonicalize(raw) };
}

template<auto R, typename... Rest> Time<R> canonicalize(const CSS::TimeRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> Frequency<R> canonicalize(const CSS::FrequencyRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { CSS::canonicalize(raw) };
}

template<auto R, typename... Rest> Frequency<R> canonicalize(const CSS::FrequencyRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> Resolution<R> canonicalize(const CSS::ResolutionRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { CSS::canonicalize(raw) };
}

template<auto R, typename... Rest> Resolution<R> canonicalize(const CSS::ResolutionRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> constexpr Flex<R> canonicalize(const CSS::FlexRaw<R>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { raw.value };
}

template<auto R, typename... Rest> constexpr Flex<R> canonicalize(const CSS::FlexRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> AnglePercentage<R> canonicalize(const CSS::AnglePercentageRaw<R>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> AnglePercentage<R> {
            return { canonicalize(CSS::PercentageRaw<R> { raw.value }, token, std::forward<Rest>(rest)...) };
        },
        [&](CSS::AngleUnit angleUnit) -> AnglePercentage<R> {
            return { canonicalize(CSS::AngleRaw<R> { angleUnit, raw.value }, token, std::forward<Rest>(rest)...) };
        }
    );
}

template<auto R, typename... Rest> AnglePercentage<R> canonicalize(const CSS::AnglePercentageRaw<R>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename... Rest> LengthPercentage<R> canonicalize(const CSS::LengthPercentageRaw<R>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> LengthPercentage<R> {
            return canonicalize(CSS::PercentageRaw<R> { raw.value }, token, std::forward<Rest>(rest)...);
        },
        [&](CSS::LengthUnit lengthUnit) -> LengthPercentage<R> {
            // NOTE: This uses the non-clamping version length canonicalization to match the behavior of CSSPrimitiveValue::convertToLength().
            return Length<R> { narrowPrecisionToFloat(canonicalizeLength(raw.value, lengthUnit, token)) };
        }
    );
}

template<auto R, typename... Rest> LengthPercentage<R> canonicalize(const CSS::LengthPercentageRaw<R>& raw, const CSSToLengthConversionData& conversionData, Rest&&... rest)
{
    ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> LengthPercentage<R> {
            return canonicalize(CSS::PercentageRaw<R> { raw.value }, conversionData, std::forward<Rest>(rest)...);
        },
        [&](CSS::LengthUnit lengthUnit) -> LengthPercentage<R> {
            // NOTE: This uses the non-clamping version length canonicalization to match the behavior of CSSPrimitiveValue::convertToLength().
            return Length<R> { narrowPrecisionToFloat(canonicalizeLength(raw.value, lengthUnit, conversionData)) };
        }
    );
}

// MARK: - Conversion from "Style to "CSS"

// Out of line to avoid inclusion of CSSCalcValue.h
Ref<CSSCalcValue> makeCalc(Ref<CalculationValue>, const RenderStyle&);
// Out of line to avoid inclusion of RenderStyleInlines.h
float adjustForZoom(float, const RenderStyle&);

// Length requires a specialized implementation due to zoom adjustment.
template<auto R> struct ToCSS<Length<R>> {
    auto operator()(const Length<R>& value, const RenderStyle& style) -> CSS::Length<R>
    {
        return CSS::LengthRaw<R> { value.unit, adjustForZoom(value.value, style) };
    }
};

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<auto R> struct ToCSS<AnglePercentage<R>> {
    auto operator()(const AnglePercentage<R>& value, const RenderStyle& style) -> CSS::AnglePercentage<R>
    {
        return WTF::switchOn(value,
            [&](const Angle<R>& angle) -> CSS::AnglePercentage<R> {
                return typename CSS::AnglePercentage<R>::Raw { angle.unit, angle.value };
            },
            [&](const Percentage<R>& percentage) -> CSS::AnglePercentage<R> {
                return typename CSS::AnglePercentage<R>::Raw { percentage.unit, percentage.value };
            },
            [&](const typename AnglePercentage<R>::Calc& calculation) -> CSS::AnglePercentage<R> {
                return typename CSS::AnglePercentage<R>::Calc { makeCalc(calculation.protectedCalculation(), style) };
            }
        );
    }
};

template<auto R> struct ToCSS<LengthPercentage<R>> {
    auto operator()(const LengthPercentage<R>& value, const RenderStyle& style) -> CSS::LengthPercentage<R>
    {
        return WTF::switchOn(value,
            [&](const Length<R>& length) -> CSS::LengthPercentage<R> {
                return typename CSS::LengthPercentage<R>::Raw { length.unit, adjustForZoom(length.value, style) };
            },
            [&](const Percentage<R>& percentage) -> CSS::LengthPercentage<R> {
                return typename CSS::LengthPercentage<R>::Raw { percentage.unit, percentage.value };
            },
            [&](const typename LengthPercentage<R>::Calc& calculation) -> CSS::LengthPercentage<R> {
                return typename CSS::LengthPercentage<R>::Calc { makeCalc(calculation.protectedCalculation(), style) };
            }
        );
    }
};

// Partial specialization for remaining numeric types.
template<Numeric StyleType> struct ToCSS<StyleType> {
    auto operator()(const StyleType& value, const RenderStyle&) -> typename StyleType::CSS
    {
        return { value.unit, value.value };
    }
};

// NumberOrPercentageResolvedToNumber requires specialization due to asymmetric representations.
template<auto nR, auto pR> struct ToCSS<NumberOrPercentageResolvedToNumber<nR, pR>> {
    auto operator()(const NumberOrPercentageResolvedToNumber<nR, pR>& value, const RenderStyle& style) -> CSS::NumberOrPercentageResolvedToNumber<nR, pR>
    {
        return { toCSS(value.value, style) };
    }
};

// MARK: - Conversion from CSS -> Style

// Integer, Length, AnglePercentage and LengthPercentage require specialized implementations for their calc canonicalization.

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::IntegerRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::IntegerRaw<R, V>>;
    using To = Integer<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return { roundForImpreciseConversion<V>(CSS::unevaluatedCalcEvaluate(value.protectedCalc(), From::category, std::forward<Rest>(rest)...)) };
    }
};

template<auto R> struct ToStyle<CSS::UnevaluatedCalc<CSS::LengthRaw<R>>> {
    using From = CSS::UnevaluatedCalc<CSS::LengthRaw<R>>;
    using To = Length<R>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return { clampLengthToAllowedLimits(CSS::unevaluatedCalcEvaluate(value.protectedCalc(), From::category, std::forward<Rest>(rest)...)) };
    }
};

template<auto R> struct ToStyle<CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R>>> {
    using From = CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R>>;
    using To = AnglePercentage<R>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Angle<R> { calc->doubleValue(std::forward<Rest>(rest)...) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValue(std::forward<Rest>(rest)...) } };
        return { calc->createCalculationValue(std::forward<Rest>(rest)...) };
    }
};

template<auto R> struct ToStyle<CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R>>> {
    using From = CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R>>;
    using To = LengthPercentage<R>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        Ref calc = value.protectedCalc();

        ASSERT(calc->tree().category == From::category);

        if (!calc->tree().type.percentHint)
            return { Style::Length<R> { clampLengthToAllowedLimits(calc->doubleValue(std::forward<Rest>(rest)...)) } };
        if (std::holds_alternative<CSSCalc::Percentage>(calc->tree().root))
            return { Style::Percentage<R> { calc->doubleValue(std::forward<Rest>(rest)...) } };
        return { calc->createCalculationValue(std::forward<Rest>(rest)...) };
    }
};

// Partial specialization for remaining numeric types.

template<CSS::NumericRaw RawType> struct ToStyle<RawType> {
    using From = RawType;
    using To = typename ToStyleMapping<typename RawToCSSMapping<RawType>::type>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return { canonicalize(value, std::forward<Rest>(rest)...) };
    }
};

template<CSS::NumericRaw RawType> struct ToStyle<CSS::UnevaluatedCalc<RawType>> {
    using From = CSS::UnevaluatedCalc<RawType>;
    using To = typename ToStyleMapping<typename RawToCSSMapping<RawType>::type>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return { CSS::unevaluatedCalcEvaluate(value.protectedCalc(), From::category, std::forward<Rest>(rest)...) };
    }
};

template<CSS::Numeric NumericType> struct ToStyle<NumericType> {
    using From = NumericType;
    using To = typename ToStyleMapping<NumericType>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) { return toStyle(value, std::forward<Rest>(rest)...); });
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<typename From::Raw>(state), std::forward<Rest>(rest)...);
    }
};

// NumberOrPercentageResolvedToNumber, as the name implies, resolves its percentage to a number.
template<auto nR, auto pR> struct ToStyle<CSS::NumberOrPercentageResolvedToNumber<nR, pR>> {
    template<typename... Rest> auto operator()(const CSS::NumberOrPercentageResolvedToNumber<nR, pR>& value, Rest&&... rest) -> NumberOrPercentageResolvedToNumber<nR, pR>
    {
        return WTF::switchOn(value,
            [&](CSS::Number<nR> number) -> NumberOrPercentageResolvedToNumber<nR, pR> {
                return { toStyle(number, std::forward<Rest>(rest)...) };
            },
            [&](CSS::Percentage<pR> percentage) -> NumberOrPercentageResolvedToNumber<nR, pR> {
                return { toStyle(percentage, std::forward<Rest>(rest)...).value / 100.0 };
            }
        );
    }
};

} // namespace Style
} // namespace WebCore
