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

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "LayoutUnit.h"
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// MARK: - Number

template<auto R, typename V> struct Evaluation<Number<R, V>> {
    constexpr double operator()(const Number<R, V>& number)
    {
        return number.value;
    }

    constexpr double operator()(const Number<R, V>& number, double)
    {
        return number.value;
    }

    constexpr float operator()(const Number<R, V>& number, float)
    {
        return static_cast<float>(number.value);
    }

    constexpr LayoutUnit operator()(const Number<R, V>& number, LayoutUnit)
    {
        return LayoutUnit(number.value);
    }
};

// MARK: - Percentage

template<auto R, typename V> struct Evaluation<Percentage<R, V>> {
    constexpr double operator()(const Percentage<R, V>& percentage)
    {
        return percentage.value / 100.0;
    }

    constexpr double operator()(const Percentage<R, V>& percentage, double referenceLength)
    {
        return percentage.value / 100.0 * referenceLength;
    }

    constexpr float operator()(const Percentage<R, V>& percentage, float referenceLength)
    {
        return static_cast<float>(percentage.value) / 100.0f * referenceLength;
    }

    constexpr LayoutUnit operator()(const Percentage<R, V>& percentage, LayoutUnit referenceLength)
    {
        // Don't remove the extra cast to float. It is needed for rounding on 32-bit Intel machines that use the FPU stack.
        return LayoutUnit(static_cast<float>(percentage.value / 100.0 * referenceLength));
    }
};

// MARK: - Numeric

template<NonCompositeNumeric StyleType> struct Evaluation<StyleType> {
    constexpr double operator()(const StyleType& value)
    {
        return value.value;
    }

    constexpr double operator()(const StyleType& value, double)
    {
        return value.value;
    }

    constexpr float operator()(const StyleType& value, float)
    {
        return value.value;
    }

    constexpr LayoutUnit operator()(const StyleType& value, LayoutUnit)
    {
        return LayoutUnit(value.value);
    }
};

// MARK: - Calculation

template<> struct Evaluation<Ref<CalculationValue>> {
    inline double operator()(Ref<CalculationValue> calculation, double referenceLength)
    {
        return calculation->evaluate(referenceLength);
    }

    inline float operator()(Ref<CalculationValue> calculation, float referenceLength)
    {
        return calculation->evaluate(referenceLength);
    }

    inline LayoutUnit operator()(Ref<CalculationValue> calculation, LayoutUnit referenceLength)
    {
        return LayoutUnit(calculation->evaluate(referenceLength));
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

// MARK: - Calculated Evaluations

// Convert to `calc(100% - value)`.
template<auto R, typename V> LengthPercentage<R, V> reflect(const LengthPercentage<R, V>& value)
{
    return WTF::switchOn(value,
        [&](const Length<R, V>& value) -> LengthPercentage<R, V> {
            // If `value` is 0, we can avoid the `calc` altogether.
            if (value.value == 0)
                return { Percentage<R, V> { 100 } };

            // Turn this into a calc expression: `calc(100% - value)`.
            return { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
        },
        [&](const Percentage<R, V>& value) -> LengthPercentage<R, V> {
            // If `value` is a percentage, we can avoid the `calc` altogether.
            return { Percentage<R, V> { 100 - value.value } };
        },
        [&](const typename LengthPercentage<R, V>::Calc& value) -> LengthPercentage<> {
            // Turn this into a calc expression: `calc(100% - value)`.
            return { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
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

    bool aIsZero = a.isZero();
    bool bIsZero = b.isZero();

    // If both `a` and `b` are 0, turn this into a calc expression: `calc(100% - (0 + 0))` aka `100%`.
    if (aIsZero && bIsZero)
        return { Percentage<resultR, V> { 100 } };

    // If just `a` is 0, we can just consider the case of `calc(100% - b)`.
    if (aIsZero) {
        return WTF::switchOn(b,
            [&](const Percentage<bR, V>& b) -> LengthPercentage<resultR, V> {
                // And if `b` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR, V> { 100 - b.value } };
            },
            [&](const auto& b) -> LengthPercentage<resultR, V> {
                // Otherwise, turn this into a calc expression: `calc(100% - b)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(b)) };
            }
        );
    }

    // If just `b` is 0, we can just consider the case of `calc(100% - a)`.
    if (bIsZero) {
        return WTF::switchOn(a,
            [&](const Percentage<aR, V>& a) -> LengthPercentage<resultR, V> {
                // And if `a` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR, V> { 100 - a.value } };
            },
            [&](const auto& a) -> LengthPercentage<resultR, V> {
                // Otherwise, turn this into a calc expression: `calc(100% - a)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(a)) };
            }
        );
    }

    // If both and `a` and `b` are percentages, we can avoid the `calc` altogether.
    if (WTF::holdsAlternative<Percentage<aR, V>>(a) && WTF::holdsAlternative<Percentage<bR, V>>(b))
        return { Percentage<resultR, V> { 100 - (get<Percentage<aR, V>>(a).value + get<Percentage<bR, V>>(b).value) } };

    // Otherwise, turn this into a calc expression: `calc(100% - (a + b))`.
    return { Calculation::subtract(Calculation::percentage(100), Calculation::add(copyCalculation(a), copyCalculation(b))) };
}

} // namespace Style
} // namespace WebCore
