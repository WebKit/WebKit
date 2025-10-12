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

#include <WebCore/FloatConversion.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/LayoutUnit.h>
#include <WebCore/StylePrimitiveNumericTypes+Calculation.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Length

template<auto R, typename V, typename Result> struct Evaluation<Length<R, V>, Result> {
    constexpr auto operator()(const Length<R, V>& value, ZoomNeeded token) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return Result(value.resolveZoom(token));
    }
    constexpr auto operator()(const Length<R, V>& value, ZoomFactor zoom) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return Result(value.resolveZoom(zoom));
    }

    constexpr auto operator()(const Length<R, V>& value, Result, ZoomNeeded token) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return Result(value.resolveZoom(token));
    }
    constexpr auto operator()(const Length<R, V>& value, Result, ZoomFactor zoom) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return Result(value.resolveZoom(zoom));
    }
};

// MARK: - Percentage

template<auto R, typename V, typename Result> struct Evaluation<Percentage<R, V>, Result> {
    constexpr auto operator()(const Percentage<R, V>& percentage) -> Result
    {
        return Result(percentage.value / 100.0);
    }
    constexpr auto operator()(const Percentage<R, V>& percentage, Result referenceLength) -> Result
    {
        return Result(percentage.value / 100.0 * referenceLength);
    }
};

// MARK: - Numeric

template<NonCompositeNumeric StyleType, typename Result> struct Evaluation<StyleType, Result> {
    constexpr auto operator()(const StyleType& value) -> Result
    {
        return Result(value.value);
    }
    constexpr auto operator()(const StyleType& value, Result) -> Result
    {
        return Result(value.value);
    }
};

// MARK: - Calculation

template<typename Result> struct Evaluation<Ref<CalculationValue>, Result> {
    auto operator()(Ref<CalculationValue> calculation, Result referenceLength) -> Result
    {
        return Result(calculation->evaluate(referenceLength));
    }
};

template<Calc Calculation, typename Result> struct Evaluation<Calculation, Result> {
    template<typename... Rest> auto operator()(const Calculation& calculation, Result referenceLength, Rest&&... rest) -> Result
    {
        return evaluate<Result>(calculation.protectedCalculation(), referenceLength, std::forward<Rest>(rest)...);
    }
};

// MARK: - LengthPercentage

template<auto R, typename V, typename Result> struct Evaluation<LengthPercentage<R, V>, Result> {
    constexpr auto operator()(const LengthPercentage<R, V>& lengthPercentage, Result referenceLength, ZoomNeeded token) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return WTF::switchOn(lengthPercentage,
            [&](const typename LengthPercentage<R, V>::Dimension& length) -> Result {
                return evaluate<Result>(length, token);
            },
            [&](const typename LengthPercentage<R, V>::Percentage& percentage) -> Result {
                return evaluate<Result>(percentage, referenceLength);
            },
            [&](const typename LengthPercentage<R, V>::Calc& calculation) -> Result {
                return evaluate<Result>(calculation, referenceLength);
            }
        );
    }
    constexpr auto operator()(const LengthPercentage<R, V>& lengthPercentage, Result referenceLength, ZoomFactor zoom) -> Result
        requires (R.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return WTF::switchOn(lengthPercentage,
            [&](const typename LengthPercentage<R, V>::Dimension& length) -> Result {
                return evaluate<Result>(length, zoom);
            },
            [&](const typename LengthPercentage<R, V>::Percentage& percentage) -> Result {
                return evaluate<Result>(percentage, referenceLength);
            },
            [&](const typename LengthPercentage<R, V>::Calc& calculation) -> Result {
                return evaluate<Result>(calculation, referenceLength);
            }
        );
    }
};

// MARK: - SpaceSeparatedPoint

template<typename T> struct Evaluation<SpaceSeparatedPoint<T>, FloatPoint> {
    auto operator()(const SpaceSeparatedPoint<T>& value, FloatSize referenceBox) -> FloatPoint
        requires HasTwoParameterEvaluate<T, float, float>
    {
        return {
            evaluate<float>(value.x(), referenceBox.width()),
            evaluate<float>(value.y(), referenceBox.height())
        };
    }
    auto operator()(const SpaceSeparatedPoint<T>& value, FloatSize referenceBox, ZoomNeeded token) -> FloatPoint
        requires HasThreeParameterEvaluate<T, float, float, ZoomNeeded>
    {
        return {
            evaluate<float>(value.x(), referenceBox.width(), token),
            evaluate<float>(value.y(), referenceBox.height(), token)
        };
    }
    auto operator()(const SpaceSeparatedPoint<T>& value, FloatSize referenceBox, ZoomFactor zoom) -> FloatPoint
        requires HasThreeParameterEvaluate<T, float, float, ZoomFactor>
    {
        return {
            evaluate<float>(value.x(), referenceBox.width(), zoom),
            evaluate<float>(value.y(), referenceBox.height(), zoom)
        };
    }
};
template<typename T> struct Evaluation<SpaceSeparatedPoint<T>, LayoutPoint> {
    auto operator()(const SpaceSeparatedPoint<T>& value, LayoutSize referenceBox) -> LayoutPoint
        requires HasTwoParameterEvaluate<T, LayoutUnit, LayoutUnit>
    {
        return {
            evaluate<LayoutUnit>(value.x(), referenceBox.width()),
            evaluate<LayoutUnit>(value.y(), referenceBox.height())
        };
    }
    auto operator()(const SpaceSeparatedPoint<T>& value, LayoutSize referenceBox, ZoomNeeded token) -> LayoutPoint
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, ZoomNeeded>
    {
        return {
            evaluate<LayoutUnit>(value.x(), referenceBox.width(), token),
            evaluate<LayoutUnit>(value.y(), referenceBox.height(), token)
        };
    }
    auto operator()(const SpaceSeparatedPoint<T>& value, LayoutSize referenceBox, ZoomFactor zoom) -> LayoutPoint
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, ZoomFactor>
    {
        return {
            evaluate<LayoutUnit>(value.x(), referenceBox.width(), zoom),
            evaluate<LayoutUnit>(value.y(), referenceBox.height(), zoom)
        };
    }
};

// MARK: - SpaceSeparatedSize

template<typename T> struct Evaluation<SpaceSeparatedSize<T>, FloatSize> {
    auto operator()(const SpaceSeparatedSize<T>& value, FloatSize referenceBox) -> FloatSize
        requires HasTwoParameterEvaluate<T, float, float>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width()),
            evaluate<float>(value.height(), referenceBox.height())
        };
    }
    auto operator()(const SpaceSeparatedSize<T>& value, FloatSize referenceBox, ZoomNeeded token) -> FloatSize
        requires HasThreeParameterEvaluate<T, float, float, ZoomNeeded>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width(), token),
            evaluate<float>(value.height(), referenceBox.height(), token)
        };
    }
    auto operator()(const SpaceSeparatedSize<T>& value, FloatSize referenceBox, ZoomFactor zoom) -> FloatSize
        requires HasThreeParameterEvaluate<T, float, float, float>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width(), zoom),
            evaluate<float>(value.height(), referenceBox.height(), zoom)
        };
    }
};
template<typename T> struct Evaluation<SpaceSeparatedSize<T>, LayoutSize> {
    auto operator()(const SpaceSeparatedSize<T>& value, LayoutSize referenceBox) -> LayoutSize
        requires HasTwoParameterEvaluate<T, LayoutUnit, LayoutUnit>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width()),
            evaluate<LayoutUnit>(value.height(), referenceBox.height())
        };
    }
    auto operator()(const SpaceSeparatedSize<T>& value, LayoutSize referenceBox, ZoomNeeded token) -> LayoutSize
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, ZoomNeeded>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width(), token),
            evaluate<LayoutUnit>(value.height(), referenceBox.height(), token)
        };
    }
    auto operator()(const SpaceSeparatedSize<T>& value, LayoutSize referenceBox, ZoomFactor zoom) -> LayoutSize
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, float>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width(), zoom),
            evaluate<LayoutUnit>(value.height(), referenceBox.height(), zoom)
        };
    }
};

// MARK: - MinimallySerializingSpaceSeparatedSize

template<typename T> struct Evaluation<MinimallySerializingSpaceSeparatedSize<T>, FloatSize> {
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, FloatSize referenceBox) -> FloatSize
        requires HasTwoParameterEvaluate<T, float, float>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width()),
            evaluate<float>(value.height(), referenceBox.height())
        };
    }
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, FloatSize referenceBox, ZoomNeeded token) -> FloatSize
        requires HasThreeParameterEvaluate<T, float, float, ZoomNeeded>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width(), token),
            evaluate<float>(value.height(), referenceBox.height(), token)
        };
    }
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, FloatSize referenceBox, ZoomFactor zoom) -> FloatSize
        requires HasThreeParameterEvaluate<T, float, float, ZoomFactor>
    {
        return {
            evaluate<float>(value.width(), referenceBox.width(), zoom),
            evaluate<float>(value.height(), referenceBox.height(), zoom)
        };
    }
};
template<typename T> struct Evaluation<MinimallySerializingSpaceSeparatedSize<T>, LayoutSize> {
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, LayoutSize referenceBox) -> LayoutSize
        requires HasTwoParameterEvaluate<T, LayoutUnit, LayoutUnit>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width()),
            evaluate<LayoutUnit>(value.height(), referenceBox.height())
        };
    }
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, LayoutSize referenceBox, ZoomNeeded token) -> LayoutSize
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, ZoomNeeded>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width(), token),
            evaluate<LayoutUnit>(value.height(), referenceBox.height(), token)
        };
    }
    auto operator()(const MinimallySerializingSpaceSeparatedSize<T>& value, LayoutSize referenceBox, ZoomFactor zoom) -> LayoutSize
        requires HasThreeParameterEvaluate<T, LayoutUnit, LayoutUnit, ZoomFactor>
    {
        return {
            evaluate<LayoutUnit>(value.width(), referenceBox.width(), zoom),
            evaluate<LayoutUnit>(value.height(), referenceBox.height(), zoom)
        };
    }
};

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

    bool aIsZero = a.isKnownZero();
    bool bIsZero = b.isKnownZero();

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
