/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSUnevaluatedCalc.h"
#include "FloatConversion.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes.h"
#include "StylePrimitiveNumericTypes+Rounding.h"

namespace WebCore {
namespace Style {

// Out of line to avoid additional includes.
double canonicalizeLength(double, CSS::LengthUnit, NoConversionDataRequiredToken);
double canonicalizeLength(double, CSS::LengthUnit, const CSSToLengthConversionData&);
float NODELETE adjustForZoom(float, const RenderStyle&);
bool NODELETE evaluationTimeZoomEnabled(const RenderStyle&);
bool NODELETE evaluationTimeZoomEnabled(const BuilderState&);

// MARK: Conversion Data specialization

template<typename T> struct ConversionDataSpecializer {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        return state.cssToLengthConversionData();
    }
};

template<auto R, typename V> struct ConversionDataSpecializer<Style::Length<R, V>> {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Default) {
            return state.useSVGZoomRulesForLength()
                ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : state.cssToLengthConversionData();
        } else if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(state))
                return state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f, R.zoomOptions);

            return state.useSVGZoomRulesForLength()
                ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : state.cssToLengthConversionData();
        }
    }
};

template<auto R, typename V> struct ConversionDataSpecializer<Style::LengthPercentage<R, V>> {
    CSSToLengthConversionData operator()(const BuilderState& state)
    {
        if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Default) {
            return state.useSVGZoomRulesForLength()
                ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : state.cssToLengthConversionData();
        } else if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(state))
                return state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f, R.zoomOptions);

            return state.useSVGZoomRulesForLength()
                ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)
                : state.cssToLengthConversionData();
        }
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

template<auto R, typename V, typename... Rest> constexpr Integer<R, V> canonicalize(const CSS::IntegerRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Integer<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> constexpr Number<R, V> canonicalize(const CSS::NumberRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Number<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> constexpr Percentage<R, V> canonicalize(const CSS::PercentageRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Percentage<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Angle<R, V> canonicalize(const CSS::AngleRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Angle<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Time<R, V> canonicalize(const CSS::TimeRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Time<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Frequency<R, V> canonicalize(const CSS::FrequencyRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Frequency<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Resolution<R, V> canonicalize(const CSS::ResolutionRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Resolution<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> constexpr Flex<R, V> canonicalize(const CSS::FlexRaw<R, V>& raw, Rest&&...)
{
    return { CSS::clampToRangeOf<Flex<R, V>>(CSS::canonicalize(raw)) };
}

template<auto R, typename V, typename... Rest> Length<R, V> canonicalize(const CSS::LengthRaw<R, V>& raw, Rest&&... rest)
{
    return { CSS::clampToRangeOf<Length<R, V>>(canonicalizeLength(raw.value, raw.unit, std::forward<Rest>(rest)...)) };
}

template<auto R, typename V, typename... Rest> AnglePercentage<R, V> canonicalize(const CSS::AnglePercentageRaw<R, V>& raw, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> AnglePercentage<R, V> {
            return { canonicalize(CSS::PercentageRaw<R, V> { raw.value }, std::forward<Rest>(rest)...) };
        },
        [&](CSS::AngleUnit angleUnit) -> AnglePercentage<R, V> {
            return { canonicalize(CSS::AngleRaw<R, V> { angleUnit, raw.value }, std::forward<Rest>(rest)...) };
        }
    );
}

template<auto R, typename V, typename... Rest> LengthPercentage<R, V> canonicalize(const CSS::LengthPercentageRaw<R, V>& raw, Rest&&... rest)
{
    return CSS::switchOnUnitType(raw.unit,
        [&](CSS::PercentageUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::PercentageRaw<R, V> { raw.value }, std::forward<Rest>(rest)...);
        },
        [&](CSS::LengthUnit lengthUnit) -> LengthPercentage<R, V> {
            return canonicalize(CSS::LengthRaw<R, V> { lengthUnit, raw.value }, std::forward<Rest>(rest)...);
        }
    );
}

// MARK: - Conversion from "Style to "CSS"

// Length requires a specialized implementation due to zoom adjustment.
template<auto R, typename V> struct ToCSS<Length<R, V>> {
    auto operator()(const Length<R, V>& value, const RenderStyle& style) -> CSS::Length<R, V>
    {
        if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Default) {
            return CSS::LengthRaw<R, V> { value.unit, adjustForZoom(value.unresolvedValue(), style) };
        } else if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
            if (evaluationTimeZoomEnabled(style))
                return CSS::LengthRaw<R, V> { value.unit, value.unresolvedValue() };

            return CSS::LengthRaw<R, V> { value.unit, adjustForZoom(value.unresolvedValue(), style) };
        }
    }
};

template<auto R, typename V> struct ToCSS<UnevaluatedCalculation<CSS::AnglePercentage<R, V>>> {
    auto operator()(const UnevaluatedCalculation<CSS::AnglePercentage<R, V>>& value, const RenderStyle& style) -> typename CSS::AnglePercentage<R, V>::Calc
    {
        return typename CSS::AnglePercentage<R, V>::Calc { value, style };
    }
};

template<auto R, typename V> struct ToCSS<UnevaluatedCalculation<CSS::LengthPercentage<R, V>>> {
    auto operator()(const UnevaluatedCalculation<CSS::LengthPercentage<R, V>>& value, const RenderStyle& style) -> typename CSS::LengthPercentage<R, V>::Calc
    {
        return typename CSS::LengthPercentage<R, V>::Calc { value, style };
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
                return typename CSS::AnglePercentage<R, V>::Calc { calculation, style };
            }
        );
    }
};

template<auto R, typename V> struct ToCSS<LengthPercentage<R, V>> {
    auto operator()(const LengthPercentage<R, V>& value, const RenderStyle& style) -> CSS::LengthPercentage<R, V>
    {
        return WTF::switchOn(value,
            [&](const typename LengthPercentage<R, V>::Dimension& length) -> CSS::LengthPercentage<R, V> {
                if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Default) {
                    return typename CSS::LengthPercentage<R, V>::Raw { length.unit, adjustForZoom(length.unresolvedValue(), style) };
                } else if constexpr (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed) {
                    return typename CSS::LengthPercentage<R, V>::Raw { length.unit, length.unresolvedValue() };
                }
            },
            [&](const typename LengthPercentage<R, V>::Percentage& percentage) -> CSS::LengthPercentage<R, V> {
                return typename CSS::LengthPercentage<R, V>::Raw { percentage.unit, percentage.value };
            },
            [&](const typename LengthPercentage<R, V>::Calc& calculation) -> CSS::LengthPercentage<R> {
                return typename CSS::LengthPercentage<R, V>::Calc { calculation, style };
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

// Integer and Number require specialized implementations to allow for either <integer> or <number> category values.

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::IntegerRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::IntegerRaw<R, V>>;
    using To = Integer<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        ASSERT(value.runtimeCategory() == CSS::Category::Number || value.runtimeCategory() == CSS::Category::Integer);
        return { canonicalize(CSS::IntegerRaw<R, V> { To::unit, value.evaluate(rest...) }, rest...) };
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
    }
};

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::NumberRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::NumberRaw<R, V>>;
    using To = Number<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        ASSERT(value.runtimeCategory() == CSS::Category::Number || value.runtimeCategory() == CSS::Category::Integer);
        return { canonicalize(CSS::NumberRaw<R, V> { To::unit, value.evaluate(rest...) }, rest...) };
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
    }
};

// AnglePercentage and LengthPercentage require specialized implementations for their calc canonicalization.

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::AnglePercentageRaw<R, V>>;
    using To = AnglePercentage<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        // NOTE: Simplification is needed here for the case of the user using the Typed CSSOM
        // to explicitly specify a CSSMath* value for a specified value.

        auto simplifiedCalc = value.simplify(rest...);

        // FIXME: This ASSERT and the following extra cases for Category::Angle and Category::Percentage
        // should go away once the typed CSSOM learns to set the correct category when creating internal
        // representations of CSSMath* types.

        ASSERT(simplifiedCalc.runtimeCategory() == CSS::Category::AnglePercentage || simplifiedCalc.runtimeCategory() == CSS::Category::Angle || simplifiedCalc.runtimeCategory() == CSS::Category::Percentage);

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Angle) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::AngleRaw<R, V> { To::Dimension::unit, doubleValue }, std::forward<Rest>(rest)...);
        }

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Percentage) {
            if (simplifiedCalc.rootNodeIsPercentage()) {
                auto doubleValue = simplifiedCalc.evaluate(rest...);
                return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, std::forward<Rest>(rest)...);
            }
            return typename To::Calc(simplifiedCalc.createCalculationValue(std::forward<Rest>(rest)...));
        }

        auto simplifiedPrimitiveType = simplifiedCalc.primitiveType();

        if (simplifiedPrimitiveType == CSSUnitType::CSS_DEG) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::AngleRaw<R, V> { To::Dimension::unit, doubleValue }, std::forward<Rest>(rest)...);
        }
        if (simplifiedPrimitiveType == CSSUnitType::CSS_PERCENTAGE) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, std::forward<Rest>(rest)...);
        }
        return typename To::Calc(simplifiedCalc.createCalculationValue(std::forward<Rest>(rest)...));
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
    }
};

template<auto R, typename V> struct ToStyle<CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>> {
    using From = CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>>;
    using To = LengthPercentage<R, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        // NOTE: Simplification is needed here for the case of the user using the Typed CSSOM
        // to explicitly specify a CSSMath* value for a specified value.

        auto simplifiedCalc = value.simplify(rest...);

        // FIXME: This ASSERT and the following extra cases for Category::Length and Category::Percentage
        // should go away once the typed CSSOM learns to set the correct category when creating internal
        // representations of CSSMath* types.

        ASSERT(simplifiedCalc.runtimeCategory() == CSS::Category::LengthPercentage || simplifiedCalc.runtimeCategory() == CSS::Category::Length || simplifiedCalc.runtimeCategory() == CSS::Category::Percentage);

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Length) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, doubleValue }, std::forward<Rest>(rest)...);
        }

        if (simplifiedCalc.runtimeCategory() == CSS::Category::Percentage) {
            if (simplifiedCalc.rootNodeIsPercentage()) {
                auto doubleValue = simplifiedCalc.evaluate(rest...);
                return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, std::forward<Rest>(rest)...);
            }
            return typename To::Calc(simplifiedCalc.createCalculationValue(std::forward<Rest>(rest)...));
        }

        auto simplifiedPrimitiveType = simplifiedCalc.primitiveType();

        if (simplifiedPrimitiveType == CSSUnitType::CSS_PX) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::LengthRaw<R, V> { To::Dimension::unit, doubleValue }, std::forward<Rest>(rest)...);
        }
        if (simplifiedPrimitiveType == CSSUnitType::CSS_PERCENTAGE) {
            auto doubleValue = simplifiedCalc.evaluate(rest...);
            return canonicalize(CSS::PercentageRaw<R, V> { doubleValue }, std::forward<Rest>(rest)...);
        }
        return typename To::Calc(simplifiedCalc.createCalculationValue(std::forward<Rest>(rest)...));
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
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

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
    }
};

template<CSS::NumericRaw RawType> struct ToStyle<CSS::UnevaluatedCalc<RawType>> {
    using From = CSS::UnevaluatedCalc<RawType>;
    using To = typename ToStyleMapping<typename RawToCSSMapping<RawType>::type>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return { canonicalize(RawType { To::unit, value.evaluate(rest...) }, rest...) };
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<To>(state), std::forward<Rest>(rest)...);
    }
};

template<CSS::Numeric NumericType> struct ToStyle<NumericType> {
    using From = NumericType;
    using To = typename ToStyleMapping<From>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) -> To { return toStyle(value, std::forward<Rest>(rest)...); });
    }
};

// NumberOrPercentageResolvedToNumber, as the name implies, resolves its percentage to a number.
template<auto nR, auto pR, typename V> struct ToStyle<CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using From = CSS::NumberOrPercentageResolvedToNumber<nR, pR, V>;
    using To = NumberOrPercentageResolvedToNumber<nR, pR, V>;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) -> To { return toStyle(value, std::forward<Rest>(rest)...); });
    }
};

} // namespace Style
} // namespace WebCore
