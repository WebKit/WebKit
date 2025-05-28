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

#include "CSSCalcValue.h"
#include "CSSPrimitiveNumericTypes+Canonicalization.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSSymbol.h"
#include "CSSToLengthConversionData.h"
#include "CalculationValue.h"
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

template<auto R, typename V> struct ConversionDataSpecializer<CSS::LengthRaw<R, V>> {
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
template<auto R, typename V> struct RawToCSSMapping<CSS::IntegerRaw<R, V>>          { using type = CSS::Integer<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::NumberRaw<R, V>>           { using type = CSS::Number<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::PercentageRaw<R, V>>       { using type = CSS::Percentage<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::AngleRaw<R, V>>            { using type = CSS::Angle<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::LengthRaw<R, V>>           { using type = CSS::Length<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::TimeRaw<R, V>>             { using type = CSS::Time<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::FrequencyRaw<R, V>>        { using type = CSS::Frequency<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::ResolutionRaw<R, V>>       { using type = CSS::Resolution<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::FlexRaw<R, V>>             { using type = CSS::Flex<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::AnglePercentageRaw<R, V>>  { using type = CSS::AnglePercentage<R, V>; };
template<auto R, typename V> struct RawToCSSMapping<CSS::LengthPercentageRaw<R, V>> { using type = CSS::LengthPercentage<R, V>; };

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

template<auto R, typename V, typename... Rest> constexpr Number<R, V> canonicalize(const CSS::NumberRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { raw.value };
}

template<auto R, typename V, typename... Rest> constexpr Number<R, V> canonicalize(const CSS::NumberRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> constexpr Percentage<R, V> canonicalize(const CSS::PercentageRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(raw.value) };
}

template<auto R, typename V, typename... Rest> constexpr Percentage<R, V> canonicalize(const CSS::PercentageRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> Angle<R, V> canonicalize(const CSS::AngleRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Angle<R, V> canonicalize(const CSS::AngleRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> Length<R, V> canonicalize(const CSS::LengthRaw<R, V>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    ASSERT(!requiresConversionData(raw));

    return { canonicalizeAndClampLength(raw.value, raw.unit, token, std::forward<Rest>(rest)...) };
}

template<auto R, typename V, typename... Rest> Length<R, V> canonicalize(const CSS::LengthRaw<R, V>& raw, const CSSToLengthConversionData& conversionData, Rest&&...)
{
    ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    return { canonicalizeAndClampLength(raw.value, raw.unit, conversionData) };
}

template<auto R, typename V, typename... Rest> Time<R, V> canonicalize(const CSS::TimeRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Time<R, V> canonicalize(const CSS::TimeRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> Frequency<R, V> canonicalize(const CSS::FrequencyRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Frequency<R, V> canonicalize(const CSS::FrequencyRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> Resolution<R, V> canonicalize(const CSS::ResolutionRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Resolution<R, V> canonicalize(const CSS::ResolutionRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> constexpr Flex<R, V> canonicalize(const CSS::FlexRaw<R, V>& raw, NoConversionDataRequiredToken, Rest&&...)
{
    return { static_cast<V>(raw.value) };
}

template<auto R, typename V, typename... Rest> constexpr Flex<R, V> canonicalize(const CSS::FlexRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> AnglePercentage<R, V> canonicalize(const CSS::AnglePercentageRaw<R, V>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> AnglePercentage<R, V> {
            return { canonicalize(CSS::PercentageRaw<R, V> { raw.value }, token, std::forward<Rest>(rest)...) };
        },
        [&](CSS::AngleUnit angleUnit) -> AnglePercentage<R, V> {
            return { canonicalize(CSS::AngleRaw<R, V> { angleUnit, raw.value }, token, std::forward<Rest>(rest)...) };
        }
    );
}

template<auto R, typename V, typename... Rest> AnglePercentage<R, V> canonicalize(const CSS::AnglePercentageRaw<R, V>& raw, const CSSToLengthConversionData&, Rest&&... rest)
{
    return canonicalize(raw, NoConversionDataRequiredToken { }, std::forward<Rest>(rest)...);
}

template<auto R, typename V, typename... Rest> LengthPercentage<R, V> canonicalize(const CSS::LengthPercentageRaw<R, V>& raw, NoConversionDataRequiredToken token, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::PercentageRaw<R, V> { raw.value }, token, std::forward<Rest>(rest)...);
        },
        [&](CSS::LengthUnit lengthUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::LengthRaw<R, V> { lengthUnit, raw.value }, token, std::forward<Rest>(rest)...);
        }
    );
}

template<auto R, typename V, typename... Rest> LengthPercentage<R, V> canonicalize(const CSS::LengthPercentageRaw<R, V>& raw, const CSSToLengthConversionData& conversionData, Rest&&... rest)
{
    // ASSERT(CSS::collectComputedStyleDependencies(raw).canResolveDependenciesWithConversionData(conversionData));

    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::PercentageRaw<R, V> { raw.value }, conversionData, std::forward<Rest>(rest)...);
        },
        [&](CSS::LengthUnit lengthUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::LengthRaw<R, V> { lengthUnit, raw.value }, conversionData, std::forward<Rest>(rest)...);
        }
    );
}

// MARK: - Conversion from "Style to "CSS"

// Out of line to avoid inclusion of RenderStyleInlines.h
float adjustForZoom(float, const RenderStyle&);

// Length requires a specialized implementation due to zoom adjustment.
template<auto R, typename V> struct ToCSS<Length<R, V>> {
    auto operator()(const Length<R, V>& value, const RenderStyle& style) -> CSS::Length<R, V>
    {
        return CSS::LengthRaw<R, V> { value.unit, adjustForZoom(value.value, style) };
    }
};

template<auto R, typename V> struct ToCSS<UnevaluatedCalculation<CSS::AnglePercentage<R, V>>> {
    auto operator()(const UnevaluatedCalculation<CSS::AnglePercentage<R, V>>& value, const RenderStyle& style) -> typename CSS::AnglePercentage<R, V>::Calc
    {
        return typename CSS::AnglePercentage<R, V>::Calc { CSSCalcValue::create(value.protectedCalculation(), style) };
    }
};

template<auto R, typename V> struct ToCSS<UnevaluatedCalculation<CSS::LengthPercentage<R, V>>> {
    auto operator()(const UnevaluatedCalculation<CSS::LengthPercentage<R, V>>& value, const RenderStyle& style) -> typename CSS::LengthPercentage<R, V>::Calc
    {
        return typename CSS::LengthPercentage<R, V>::Calc { CSSCalcValue::create(value.protectedCalculation(), style) };
    }
};

// AnglePercentage / LengthPercentage require specialized implementations due to additional `calc` field.
template<auto R, typename V> struct ToCSS<AnglePercentage<R, V>> {
    auto operator()(const AnglePercentage<R, V>& value, const RenderStyle& style) -> CSS::AnglePercentage<R, V>
    {
        return WTF::switchOn(value,
            [&](const Angle<R, V>& angle) -> CSS::AnglePercentage<R, V> {
                return typename CSS::AnglePercentage<R, V>::Raw { angle.unit, angle.value };
            },
            [&](const Percentage<R, V>& percentage) -> CSS::AnglePercentage<R, V> {
                return typename CSS::AnglePercentage<R, V>::Raw { percentage.unit, percentage.value };
            },
            [&](const typename AnglePercentage<R, V>::Calc& calculation) -> CSS::AnglePercentage<R> {
                return typename CSS::AnglePercentage<R, V>::Calc { CSSCalcValue::create(calculation.protectedCalculation(), style) };
            }
        );
    }
};

template<auto R, typename V> struct ToCSS<LengthPercentage<R, V>> {
    auto operator()(const LengthPercentage<R, V>& value, const RenderStyle& style) -> CSS::LengthPercentage<R, V>
    {
        return WTF::switchOn(value,
            [&](const typename LengthPercentage<R, V>::Dimension& length) -> CSS::LengthPercentage<R, V> {
                return typename CSS::LengthPercentage<R, V>::Raw { length.unit, adjustForZoom(length.value, style) };
            },
            [&](const typename LengthPercentage<R, V>::Percentage& percentage) -> CSS::LengthPercentage<R, V> {
                return typename CSS::LengthPercentage<R, V>::Raw { percentage.unit, percentage.value };
            },
            [&](const typename LengthPercentage<R, V>::Calc& calculation) -> CSS::LengthPercentage<R> {
                return typename CSS::LengthPercentage<R, V>::Calc { CSSCalcValue::create(calculation.protectedCalculation(), style) };
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
template<auto nR, auto pR, typename V> struct ToCSS<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    auto operator()(const NumberOrPercentageResolvedToNumber<nR, pR, V>& value, const RenderStyle& style) -> CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>
    {
        return { toCSS(value.value, style) };
    }
};

// MARK: - Conversion from CSS -> Style

// AnglePercentage and LengthPercentage require specialized implementations for their calc canonicalization.

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R, V>>;
    using To = AnglePercentage<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        // NOTE: Simplification is needed here for the case of the user using the Typed CSSOM
        // to explicitly specify a CSSMath* value for a specified value.

        Ref simplifiedCalc = value.protectedCalc()->copySimplified(rest...);

        // FIXME: This ASSERT and the following extra cases for Category::Angle and Category::Percentage
        // should go away once the typed CSSOM learns to set the correct category when creating internal
        // representations of CSSMath* types.

        ASSERT(simplifiedCalc->category() == Calculation::Category::AnglePercentage || simplifiedCalc->category() == Calculation::Category::Angle || simplifiedCalc->category() == Calculation::Category::Percentage);

        if (simplifiedCalc->category() == Calculation::Category::Angle)
            return canonicalize(CSS::AngleRaw<R, V> { To::Dimension::unit, simplifiedCalc->doubleValue(rest...) }, std::forward<Rest>(rest)...);

        if (simplifiedCalc->category() == Calculation::Category::Percentage) {
            if (WTF::holdsAlternative<CSSCalc::Percentage>(simplifiedCalc->tree().root))
                return canonicalize(CSS::PercentageRaw<R, V> { simplifiedCalc->doubleValue(std::forward<Rest>(rest)...) }, std::forward<Rest>(rest)...);
            return typename To::Calc { simplifiedCalc->createCalculationValue(std::forward<Rest>(rest)...) };
        }

        if (!simplifiedCalc->tree().type.percentHint)
            return canonicalize(CSS::AngleRaw<R, V> { To::Dimension::unit, simplifiedCalc->doubleValue(rest...) }, std::forward<Rest>(rest)...);
        if (WTF::holdsAlternative<CSSCalc::Percentage>(simplifiedCalc->tree().root))
            return canonicalize(CSS::PercentageRaw<R, V> { simplifiedCalc->doubleValue(std::forward<Rest>(rest)...) }, std::forward<Rest>(rest)...);
        return typename To::Calc { simplifiedCalc->createCalculationValue(std::forward<Rest>(rest)...) };
    }
};

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>;
    using To = LengthPercentage<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        // NOTE: Simplification is needed here for the case of the user using the Typed CSSOM
        // to explicitly specify a CSSMath* value for a specified value.

        Ref simplifiedCalc = value.protectedCalc()->copySimplified(rest...);

        // FIXME: This ASSERT and the following extra cases for Category::Length and Category::Percentage
        // should go away once the typed CSSOM learns to set the correct category when creating internal
        // representations of CSSMath* types.

        ASSERT(simplifiedCalc->category() == Calculation::Category::LengthPercentage || simplifiedCalc->category() == Calculation::Category::Length || simplifiedCalc->category() == Calculation::Category::Percentage);

        if (simplifiedCalc->category() == Calculation::Category::Length)
            return canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, simplifiedCalc->doubleValue(rest...) }, std::forward<Rest>(rest)...);

        if (simplifiedCalc->category() == Calculation::Category::Percentage) {
            if (WTF::holdsAlternative<CSSCalc::Percentage>(simplifiedCalc->tree().root))
                return canonicalize(CSS::PercentageRaw<R, V> { simplifiedCalc->doubleValue(std::forward<Rest>(rest)...) }, std::forward<Rest>(rest)...);
            return typename To::Calc { simplifiedCalc->createCalculationValue(std::forward<Rest>(rest)...) };
        }

        if (!simplifiedCalc->tree().type.percentHint)
            return canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, simplifiedCalc->doubleValue(rest...) }, std::forward<Rest>(rest)...);
        if (WTF::holdsAlternative<CSSCalc::Percentage>(simplifiedCalc->tree().root))
            return canonicalize(CSS::PercentageRaw<R, V> { simplifiedCalc->doubleValue(std::forward<Rest>(rest)...) }, std::forward<Rest>(rest)...);
        return typename To::Calc { simplifiedCalc->createCalculationValue(std::forward<Rest>(rest)...) };
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
        return { canonicalize(RawType { To::unit, value.evaluate(From::category, rest...) }, rest...) };
    }
};

template<CSS::Numeric NumericType> struct ToStyle<NumericType> {
    using From = NumericType;
    using To = typename ToStyleMapping<From>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) -> To { return toStyle(value, std::forward<Rest>(rest)...); });
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<typename From::Raw>(state), std::forward<Rest>(rest)...);
    }
};

// NumberOrPercentageResolvedToNumber, as the name implies, resolves its percentage to a number.
template<auto nR, auto pR, typename V> struct ToStyle<CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using From = CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>;
    using To = NumberOrPercentageResolvedToNumber<nR, pR, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value,
            [&](const typename From::Number& number) -> To {
                return { toStyle(number, std::forward<Rest>(rest)...) };
            },
            [&](const typename From::Percentage& percentage) -> To {
                return { toStyle(percentage, std::forward<Rest>(rest)...).value / 100.0 };
            }
        );
    }
};

} // namespace Style
} // namespace WebCore
