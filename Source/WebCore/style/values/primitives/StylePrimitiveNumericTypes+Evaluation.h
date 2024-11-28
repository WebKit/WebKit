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

#include "FloatPoint.h"
#include "FloatSize.h"
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// MARK: - Number

template<auto R> constexpr double evaluate(const Number<R>& number, double)
{
    return number.value;
}

template<auto R> constexpr float evaluate(const Number<R>& number, float)
{
    return narrowPrecisionToFloat(number.value);
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

// MARK: - StyleNumericPrimitive

template<StyleNumericPrimitive T> constexpr float evaluate(const T& value, float)
{
    return value.value;
}

template<StyleNumericPrimitive T> constexpr double evaluate(const T& value, double)
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

// MARK: - StyleDimensionPercentage (e.g. AnglePercentage/LengthPercentage)

template<StyleDimensionPercentage T> float evaluate(const T& value, float referenceValue)
{
    return WTF::switchOn(value, [&referenceValue](const auto& value) -> float { return evaluate(value, referenceValue); });
}

template<StyleDimensionPercentage T> double evaluate(const T& value, double referenceValue)
{
    return WTF::switchOn(value, [&referenceValue](const auto& value) -> double { return evaluate(value, referenceValue); });
}

// MARK: - Point

template<typename T> FloatPoint evaluate(const Point<T>& value, FloatSize referenceBox)
{
    return {
        evaluate(value.x(), referenceBox.width()),
        evaluate(value.y(), referenceBox.height())
    };
}

// MARK: - Size

template<typename T> FloatSize evaluate(const Size<T>& value, FloatSize referenceBox)
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
        [&](Length<R> value) -> LengthPercentage<R> {
            // If `value` is 0, we can avoid the `calc` altogether.
            if (value.value == 0)
                return { Percentage<R> { 100 } };

            // Turn this into a calc expression: `calc(100% - value)`.
            return { Calculation::subtract(Calculation::percentage(100), copyCalculation(value)) };
        },
        [&](Percentage<R> value) -> LengthPercentage<R> {
            // If `value` is a percentage, we can avoid the `calc` altogether.
            return { Percentage<R> { 100 - value.value } };
        },
        [&](Ref<CalculationValue> value) -> LengthPercentage<> {
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

    bool aIsZero = a.value.switchOn(
        [](const Ref<CalculationValue>&) { return false; },
        [](const auto& a) { return a.value == 0; }
    );
    bool bIsZero = b.value.switchOn(
        [](const Ref<CalculationValue>&) { return false; },
        [](const auto& b) { return b.value == 0; }
    );

    // If both `a` and `b` are 0, turn this into a calc expression: `calc(100% - (0 + 0))` aka `100%`.
    if (aIsZero && bIsZero)
        return { Percentage<resultR> { 100 } };

    // If just `a` is 0, we can just consider the case of `calc(100% - b)`.
    if (aIsZero) {
        return b.value.switchOn(
            [&](Percentage<bR> b) -> LengthPercentage<resultR> {
                // And if `b` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR> { 100 - b.value } };
            },
            [&](auto b) -> LengthPercentage<resultR> {
                // Otherwise, turn this into a calc expression: `calc(100% - b)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(b)) };
            }
        );
    }

    // If just `b` is 0, we can just consider the case of `calc(100% - a)`.
    if (bIsZero) {
        return a.value.switchOn(
            [&](Percentage<aR> a) -> LengthPercentage<resultR> {
                // And if `a` is a percent, we can avoid the `calc` altogether.
                return { Percentage<resultR> { 100 - a.value } };
            },
            [&](auto a) -> LengthPercentage<resultR> {
                // Otherwise, turn this into a calc expression: `calc(100% - a)`.
                return { Calculation::subtract(Calculation::percentage(100), copyCalculation(a)) };
            }
        );
    }

    // If both and `a` and `b` are percentages, we can avoid the `calc` altogether.
    if (a.value.isPercentage() && b.value.isPercentage())
        return { Percentage<resultR> { 100 - (a.value.asPercentage().value + b.value.asPercentage().value) } };

    // Otherwise, turn this into a calc expression: `calc(100% - (a + b))`.
    return { Calculation::subtract(Calculation::percentage(100), Calculation::add(copyCalculation(a), copyCalculation(b))) };
}

} // namespace Style
} // namespace WebCore
