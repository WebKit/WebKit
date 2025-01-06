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
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// MARK: - Number

template<auto R> constexpr float evaluate(const Number<R>& number, float)
{
    return narrowPrecisionToFloat(number.value);
}

template<auto R> constexpr double evaluate(const Number<R>& number, double)
{
    return number.value;
}

// MARK: - Percentage

template<auto R> constexpr float evaluate(const Percentage<R>& percentage, float referenceLength)
{
    return narrowPrecisionToFloat(percentage.value) / 100.0f * referenceLength;
}

template<auto R> constexpr double evaluate(const Percentage<R>& percentage, double referenceLength)
{
    return percentage.value / 100.0 * referenceLength;
}

// MARK: - Numeric

constexpr float evaluate(Numeric auto const& value, float)
{
    return value.value;
}

constexpr double evaluate(Numeric auto const& value, double)
{
    return value.value;
}

inline float evaluate(const CalculationValue& calculation, float referenceValue)
{
    return calculation.evaluate(referenceValue);
}

inline double evaluate(const CalculationValue& calculation, double referenceValue)
{
    return calculation.evaluate(referenceValue);
}

inline float evaluate(Calc auto const& calculation, float referenceValue)
{
    return evaluate(calculation.protectedCalculation(), referenceValue);
}

inline double evaluate(Calc auto const& calculation, double referenceValue)
{
    return evaluate(calculation.protectedCalculation(), referenceValue);
}

// MARK: - DimensionPercentageNumeric (e.g. AnglePercentage/LengthPercentage)

inline float evaluate(DimensionPercentageNumeric auto const& value, float referenceValue)
{
    return WTF::switchOn(value, [&referenceValue](const auto& value) -> float { return evaluate(value, referenceValue); });
}

inline double evaluate(DimensionPercentageNumeric auto const& value, double referenceValue)
{
    return WTF::switchOn(value, [&referenceValue](const auto& value) -> double { return evaluate(value, referenceValue); });
}

// MARK: - NumberOrPercentage

template<auto nR, auto pR> double evaluate(const NumberOrPercentage<nR, pR>& value)
{
    return WTF::switchOn(value,
        [](Number<nR> number) -> double { return number.value; },
        [](Percentage<pR> percentage) -> double { return percentage.value / 100.0; }
    );
}

// MARK: - SpaceSeparatedPoint

template<typename T> FloatPoint evaluate(const SpaceSeparatedPoint<T>& value, FloatSize referenceBox)
{
    return {
        evaluate(value.x(), referenceBox.width()),
        evaluate(value.y(), referenceBox.height())
    };
}

// MARK: - SpaceSeparatedSize

template<typename T> FloatSize evaluate(const SpaceSeparatedSize<T>& value, FloatSize referenceBox)
{
    return {
        evaluate(value.width(), referenceBox.width()),
        evaluate(value.height(), referenceBox.height())
    };
}

// MARK: - Calculated Evaluations

// Convert to `calc(100% - value)`.
template<auto R> LengthPercentage<R> reflect(const LengthPercentage<R>& value)
{
    return WTF::switchOn(value,
        [&](const Length<R>& value) -> LengthPercentage<R> {
            // If `value` is 0, we can avoid the `calc` altogether.
            if (value.value == 0)
                return { Percentage<R> { 100 } };

            // Turn this into a calc expression: `calc(100% - value)`.
            return { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
        },
        [&](const Percentage<R>& value) -> LengthPercentage<R> {
            // If `value` is a percentage, we can avoid the `calc` altogether.
            return { Percentage<R> { 100 - value.value } };
        },
        [&](const typename LengthPercentage<R>::Calc& value) -> LengthPercentage<> {
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
template<auto aR, auto bR> auto reflectSum(const LengthPercentage<aR>& a, const LengthPercentage<bR>& b) -> LengthPercentage<mergeRanges(aR, bR)>
{
    constexpr auto resultR = mergeRanges(aR, bR);

    bool aIsZero = a.isZero();
    bool bIsZero = b.isZero();

    // If both `a` and `b` are 0, turn this into a calc expression: `calc(100% - (0 + 0))` aka `100%`.
    if (aIsZero && bIsZero)
        return { Percentage<resultR> { 100 } };

    // If just `a` is 0, we can just consider the case of `calc(100% - b)`.
    if (aIsZero) {
        return WTF::switchOn(b,
            [&](const Percentage<bR>& b) -> LengthPercentage<resultR> {
                // And if `b` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR> { 100 - b.value } };
            },
            [&](const auto& b) -> LengthPercentage<resultR> {
                // Otherwise, turn this into a calc expression: `calc(100% - b)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(b)) };
            }
        );
    }

    // If just `b` is 0, we can just consider the case of `calc(100% - a)`.
    if (bIsZero) {
        return WTF::switchOn(a,
            [&](const Percentage<aR>& a) -> LengthPercentage<resultR> {
                // And if `a` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR> { 100 - a.value } };
            },
            [&](const auto& a) -> LengthPercentage<resultR> {
                // Otherwise, turn this into a calc expression: `calc(100% - a)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(a)) };
            }
        );
    }

    // If both and `a` and `b` are percentages, we can avoid the `calc` altogether.
    if (WTF::holdsAlternative<Percentage<aR>>(a) && WTF::holdsAlternative<Percentage<bR>>(b))
        return { Percentage<resultR> { 100 - (get<Percentage<aR>>(a).value + get<Percentage<bR>>(b).value) } };

    // Otherwise, turn this into a calc expression: `calc(100% - (a + b))`.
    return { Calculation::subtract(Calculation::percentage(100), Calculation::add(copyCalculation(a), copyCalculation(b))) };
}

} // namespace Style
} // namespace WebCore
