/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleLineHeight.h"

#include "AnimationUtilities.h"

#include "CSSKeywordValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericOrKeyword+Blending.h"
#include "StylePrimitiveNumericOrKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<LineHeight>::operator()(BuilderState& state, const CSSValue& value, float multiplier) -> LineHeight
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        if (auto valueID = keywordValue->valueID(); valueID == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
            return CSS::Keyword::Normal { };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    auto conversionData = state
        .cssToLengthConversionData()
        .copyForLineHeight(state.zoomWithTextZoomFactor());

    // If EvaluationTimeZoom is not enabled then we will scale the lengths in the
    // calc values when we create the CalculationValue below by using the zoom from conversionData.
    // To avoid double zooming when we evaluate the calc expression we need to make sure
    // we have a ZoomFactor of 1.0. Otherwise, we defer to whatever is on the conversionData
    // since EvaluationTimeZoom will set the appropriate value.
    auto zoomFactor = [&] {
        if (!state.style().evaluationTimeZoomEnabled())
            return Style::ZoomFactor { 1.0f };
        return Style::ZoomFactor { conversionData.zoom() };
    };

    auto percentageBasis = [&] {
        return state.style().fontDescription().computedSizeForRangeZoomOption(conversionData.rangeZoomOption());
    };

    using StyleSpecified = typename LineHeight::Specified;
    using CSSRaw = typename StyleSpecified::CSS::Raw;
    using CSSDimensionRaw = typename CSSRaw::Dimension;
    using CSSPercentageRaw = typename CSSRaw::Percentage;

    using StyleNumber = Number<CSS::Nonnegative>;
    using CSSNumberRaw = typename StyleNumber::CSS::Raw;;

    auto handleFixed = [&](const StyleSpecified::Dimension& fixed) {
        return LineHeight::Fixed { CSS::clampToRangeOf<LineHeight::Fixed>(fixed.unresolvedValue() * multiplier) };
    };

    auto handlePercentage = [&](const StyleSpecified::Percentage& percentage) {
        // Line-height percentages need to inherit as if they were pixel values. In the example:
        // <div style="font-size: 10px; line-height: 150%;"><div style="font-size: 100px;"></div></div>
        // the inner element should have line-height of 15px. However, in this example:
        // <div style="font-size: 10px; line-height: 1.5;"><div style="font-size: 100px;"></div></div>
        // the inner element should have a line-height of 150px. Therefore, we map percentages to Fixed
        // values and raw numbers to percentages.

        // FIXME: percentage should not be restricted to an integer here.
        auto percentageValue = static_cast<int>(percentage.value);

        return LineHeight::Fixed { CSS::clampToRangeOf<LineHeight::Fixed>((percentageValue * percentageBasis() * zoomFactor().value) / 100.0) };
    };

    auto handleCalc = [&](const StyleSpecified::Calc& calc) {
        return LineHeight::Fixed { CSS::clampToRangeOf<LineHeight::Fixed>(calc.evaluate(percentageBasis(), zoomFactor()) * multiplier) };
    };

    auto handleNumber = [&](const StyleNumber& number) {
        return LineHeight::Percentage { CSS::clampToRangeOf<LineHeight::Percentage>(number.value * 100.0) };
    };

    return WTF::switchOn(*primitiveValue,
        [&](const CSSPrimitiveValue::Calc& calc) -> LineHeight {
            if (calc.runtimeCategory() == CSS::Category::Number || calc.runtimeCategory() == CSS::Category::Integer)
                return handleNumber(toStyle(CSS::UnevaluatedCalc<CSSNumberRaw> { calc }, conversionData));

            ASSERT(calc.runtimeCategory() == CSS::Category::Length || calc.runtimeCategory() == CSS::Category::Percentage || calc.runtimeCategory() == CSS::Category::LengthPercentage);

            // <length-percentage> calc() can become a raw <length> or <percentage>, or can stay a calc() when converting,
            // so we have to handle all those cases here.

            auto convertedCalc = toStyle(CSS::UnevaluatedCalc<CSSRaw> { calc }, conversionData);
            return WTF::switchOn(convertedCalc,
                [&](const StyleSpecified::Dimension& fixed) {
                    return handleFixed(fixed);
                },
                [&](const StyleSpecified::Percentage& percentage) {
                    return handlePercentage(percentage);
                },
                [&](const StyleSpecified::Calc& calc) {
                    return handleCalc(calc);
                }
            );
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> LineHeight {
            if (auto unit = CSSNumberRaw::UnitTraits::validate(raw.unit))
                return handleNumber(toStyle(CSSNumberRaw(*unit, raw.value), conversionData));

            if (auto unit = CSSPercentageRaw::UnitTraits::validate(raw.unit))
                return handlePercentage(toStyle(CSSPercentageRaw(*unit, raw.value), conversionData));

            if (auto unit = CSSDimensionRaw::UnitTraits::validate(raw.unit))
                return handleFixed(toStyle(CSSDimensionRaw(*unit, raw.value), conversionData));

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    );
}

// MARK: - Blending

auto Blending<LineHeight>::canBlend(const LineHeight& a, const LineHeight& b) -> bool
{
    return a.hasSameType(b) || (a.isCalculated() && b.isNumeric()) || (b.isCalculated() && a.isNumeric());
}

auto Blending<LineHeight>::requiresInterpolationForAccumulativeIteration(const LineHeight& a, const LineHeight& b) -> bool
{
    return !a.hasSameType(b) || a.isCalculated() || b.isCalculated();
}

auto Blending<LineHeight>::blend(const LineHeight& a, const LineHeight& b, const BlendingContext& context) -> LineHeight
{
    if (!a.isNumeric() || !b.isNumeric())
        return context.progress < 0.5 ? a : b;

    return Style::blend(get<LineHeight::Numeric>(a), get<LineHeight::Numeric>(b), context);
}

// MARK: - Evaluation

auto Evaluation<LineHeight, float>::operator()(
    const LineHeight& lineHeight, LineHeightEvaluationContext context, ZoomFactor zoom) -> float
{
    return WTF::switchOn(lineHeight,
        [&](const LineHeight::Fixed& fixed) {
            return evaluate<LayoutUnit>(fixed, zoom).toFloat();
        },
        [&](const LineHeight::Percentage& percentage) {
            return evaluate<LayoutUnit>(percentage, LayoutUnit { context.computedFontSize }).toFloat();
        },
        [&](const LineHeight::Calc& calc) {
            return evaluate<LayoutUnit>(calc, LayoutUnit { context.computedFontSize }, zoom).toFloat();
        },
        [&](const CSS::Keyword::Normal&) {
            return context.lineSpacing;
        }
    );
}

} // namespace Style
} // namespace WebCore
