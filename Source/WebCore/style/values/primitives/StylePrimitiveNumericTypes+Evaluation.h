/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "LayoutUnit.h"
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Percentage

template<auto R, typename V> struct Evaluation<Percentage<R, V>> {
    constexpr double operator()(const Percentage<R, V>& percentage)
    {
        return static_cast<double>(percentage.value) / 100.0;
    }

    template<typename Reference> constexpr auto operator()(const Percentage<R, V>& percentage, Reference referenceLength) -> Reference
    {
        return static_cast<Reference>(percentage.value) / 100.0 * referenceLength;
    }
};

template<auto R, typename V> constexpr LayoutUnit evaluate(const Percentage<R, V>& percentage, LayoutUnit referenceLength)
{
    // Don't remove the extra cast to float. It is needed for rounding on 32-bit Intel machines that use the FPU stack.
    return LayoutUnit(static_cast<float>(percentage.value / 100.0 * referenceLength));
}

// MARK: - Numeric

template<NonCompositeNumeric StyleType> struct Evaluation<StyleType> {
    constexpr double operator()(const StyleType& value)
    {
        return static_cast<double>(value.value);
    }

    template<typename Reference> constexpr auto operator()(const StyleType& value, Reference) -> Reference
    {
        return static_cast<Reference>(value.value);
    }
};

// MARK: - Calculation

template<> struct Evaluation<Ref<CalculationValue>> {
    template<typename Reference> auto operator()(Ref<CalculationValue> calculation, Reference referenceLength)
    {
        return static_cast<Reference>(calculation->evaluate(referenceLength));
    }
};

template<Calc Calculation> struct Evaluation<Calculation> {
    template<typename... Rest> decltype(auto) operator()(const Calculation& calculation, Rest&&... rest)
    {
        return evaluate(calculation.protectedCalculation(), std::forward<Rest>(rest)...);
    }
};

// MARK: - SpaceSeparatedPoint

template<typename T> struct Evaluation<SpaceSeparatedPoint<T>> {
    FloatPoint operator()(const SpaceSeparatedPoint<T>& value, FloatSize referenceBox)
    {
        return {
            evaluate(value.x(), referenceBox.width()),
            evaluate(value.y(), referenceBox.height())
        };
    }
};

// MARK: - SpaceSeparatedSize

template<typename T> struct Evaluation<SpaceSeparatedSize<T>> {
    FloatSize operator()(const SpaceSeparatedSize<T>& value, FloatSize referenceBox)
    {
        return {
            evaluate(value.width(), referenceBox.width()),
            evaluate(value.height(), referenceBox.height())
        };
    }
};

// MARK: - MinimallySerializingSpaceSeparatedSize

template<typename T> struct Evaluation<MinimallySerializingSpaceSeparatedSize<T>> {
    FloatSize operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, FloatSize referenceBox)
    {
        return {
            evaluate(value.width(), referenceBox.width()),
            evaluate(value.height(), referenceBox.height())
        };
    }
};

// MARK: - VariantLike

template<VariantLike CSSType, typename... Rest> decltype(auto) evaluate(const CSSType& value, Rest&& ...rest)
{
    return WTF::switchOn(value, [&](const auto& alternative) { return evaluate(alternative, std::forward<Rest>(rest)...); });
}

// MARK: - TupleLike

template<TupleLike CSSType, typename... Rest> requires (std::tuple_size_v<CSSType> == 1) decltype(auto) evaluate(const CSSType& value, Rest&& ...rest)
{
    return evaluate(get<0>(value), std::forward<Rest>(rest)...);
}

// MARK: - Calculated Evaluations

// Convert to `calc(100% - value)`.
template<auto R, typename V> auto reflect(const LengthPercentage<R, V>& value) -> LengthPercentage<R, V>
{
    using Result = LengthPercentage<R, V>;
    using Dimension = typename Result::Dimension;
    using Percentage = typename Result::Percentage;
    using Calc = typename Result::Calc;

    return WTF::switchOn(value,
        [&](const Dimension& value) -> Result {
            // If `value` is 0, we can avoid the `calc` altogether.
            if (value == 0_css_px)
                return 100_css_percentage;

            // Turn this into a calc expression: `calc(100% - value)`.
            return Calc { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
        },
        [&](const Percentage& value) -> Result {
            // If `value` is a percentage, we can avoid the `calc` altogether.
            return 100_css_percentage - value.value;
        },
        [&](const Calc& value) -> Result {
            // Turn this into a calc expression: `calc(100% - value)`.
            return Calc { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
        }
    );
}

// Merges the two ranges, `aR` and `bR`, creating a union of their ranges.
consteval CSS::Range mergeRanges(CSS::Range aR, CSS::Range bR)
{
    return CSS::Range { std::min(aR.min, bR.min), std::max(aR.max, bR.max) };
}

// Convert to `calc(100% - (a + b))`.
//
// Returns a LengthPercentage with range, `resultR`, equal to union of the two input ranges `aR` and `bR`.
template<auto aR, auto bR, typename V> auto reflectSum(const LengthPercentage<aR, V>& a, const LengthPercentage<bR, V>& b) -> LengthPercentage<mergeRanges(aR, bR), V>
{
    constexpr auto resultR = mergeRanges(aR, bR);

    using Result = LengthPercentage<resultR, V>;
    using CalcResult = typename Result::Calc;
    using PercentageA = typename LengthPercentage<aR, V>::Percentage;
    using PercentageB = typename LengthPercentage<bR, V>::Percentage;

    bool aIsZero = a.isZero();
    bool bIsZero = b.isZero();

    // If both `a` and `b` are 0, turn this into a calc expression: `calc(100% - (0 + 0))` aka `100%`.
    if (aIsZero && bIsZero)
        return 100_css_percentage;

    // If just `a` is 0, we can just consider the case of `calc(100% - b)`.
    if (aIsZero) {
        return WTF::switchOn(b,
            [&](const PercentageB& b) -> Result {
                // And if `b` is a percent, we can avoid the `calc` altogether.
                return 100_css_percentage - b.value;
            },
            [&](const auto& b) -> Result {
                // Otherwise, turn this into a calc expression: `calc(100% - b)`.
                return CalcResult { Calculation::subtract(Calculation::percentage(100), copyCalculation(b)) };
            }
        );
    }

    // If just `b` is 0, we can just consider the case of `calc(100% - a)`.
    if (bIsZero) {
        return WTF::switchOn(a,
            [&](const PercentageA& a) -> Result {
                // And if `a` is a percent, we can avoid the `calc` altogether.
                return 100_css_percentage - a.value;
            },
            [&](const auto& a) -> Result {
                // Otherwise, turn this into a calc expression: `calc(100% - a)`.
                return CalcResult { Calculation::subtract(Calculation::percentage(100), copyCalculation(a)) };
            }
        );
    }

    // If both and `a` and `b` are percentages, we can avoid the `calc` altogether.
    if (WTF::holdsAlternative<PercentageA>(a) && WTF::holdsAlternative<PercentageB>(b))
        return 100_css_percentage - (get<PercentageA>(a).value + get<PercentageB>(b).value);

    // Otherwise, turn this into a calc expression: `calc(100% - (a + b))`.
    return CalcResult { Calculation::subtract(Calculation::percentage(100), Calculation::add(copyCalculation(a), copyCalculation(b))) };
}

} // namespace Style
} // namespace WebCore
