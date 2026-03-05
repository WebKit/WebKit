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

#include "CSSPropertyParserConsumer+Font.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<LineHeight>::operator()(BuilderState& state, const CSSValue& value, float multiplier) -> LineHeight
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    return operator()(state, *primitiveValue, multiplier);
}

auto CSSValueConversion<LineHeight>::operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue, float multiplier) -> LineHeight
{
    auto valueID = primitiveValue.valueID();
    if (valueID == CSSValueNormal || CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
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

    if (primitiveValue.isLength() || primitiveValue.isCalculatedPercentageWithLength()) {
        double fixedValue = 0;
        if (primitiveValue.isLength())
            fixedValue = primitiveValue.resolveAsLength(conversionData);
        else
            fixedValue = protect(primitiveValue.cssCalcValue())->createCalculationValue(conversionData, CSSCalcSymbolTable { })->evaluate(state.style().fontDescription().computedSizeForRangeZoomOption(conversionData.rangeZoomOption()), zoomFactor());

        if (multiplier != 1.0f)
            fixedValue *= multiplier;

        return LineHeight::Fixed {
            CSS::clampToRange<LineHeight::Fixed::range, float>(fixedValue, minValueForCssLength, maxValueForCssLength)
        };
    }

    // Line-height percentages need to inherit as if they were Fixed pixel values. In the example:
    // <div style="font-size: 10px; line-height: 150%;"><div style="font-size: 100px;"></div></div>
    // the inner element should have line-height of 15px. However, in this example:
    // <div style="font-size: 10px; line-height: 1.5;"><div style="font-size: 100px;"></div></div>
    // the inner element should have a line-height of 150px. Therefore, we map percentages to Fixed
    // values and raw numbers to percentages.
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        auto textZoom = evaluationTimeZoomEnabled(state) ? conversionData.zoom() : 1.0f;
        return LineHeight::Fixed {
            CSS::clampToRange<LineHeight::Fixed::range, float>((state.style().fontDescription().computedSizeForRangeZoomOption(conversionData.rangeZoomOption()) * primitiveValue.resolveAsPercentage<int>(conversionData) * textZoom) / 100.0, minValueForCssLength, maxValueForCssLength)
        };
    }

    if (primitiveValue.isNumber()) {
        return LineHeight::Percentage {
            CSS::clampToRange<LineHeight::Percentage::range, float>(primitiveValue.resolveAsNumber(conversionData) * 100.0)
        };
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

// MARK: - Blending

auto Blending<LineHeight>::canBlend(const LineHeight& a, const LineHeight& b) -> bool
{
    return a.hasSameType(b) || (a.isCalculated() && b.isSpecified()) || (b.isCalculated() && a.isSpecified());
}

auto Blending<LineHeight>::requiresInterpolationForAccumulativeIteration(const LineHeight& a, const LineHeight& b) -> bool
{
    return !a.hasSameType(b) || a.isCalculated() || b.isCalculated();
}

auto Blending<LineHeight>::blend(const LineHeight& a, const LineHeight& b, const BlendingContext& context) -> LineHeight
{
    if (!a.isSpecified() || !b.isSpecified())
        return context.progress < 0.5 ? a : b;

    if (a.isCalculated() || b.isCalculated() || !a.hasSameType(b))
        return LengthWrapperBlendingSupport<LineHeight>::blendMixedSpecifiedTypes(a, b, context);

    if (!context.progress && context.isReplace())
        return a;

    if (context.progress == 1 && context.isReplace())
        return b;

    auto resultType = b.m_value.type();

    ASSERT(resultType == LineHeight::indexForPercentage || resultType == LineHeight::indexForFixed);

    if (resultType == LineHeight::indexForPercentage) {
        return Style::blend(
            LineHeight::Percentage { a.m_value.value() },
            LineHeight::Percentage { b.m_value.value() },
            context
        );
    } else {
        return Style::blend(
            LineHeight::Fixed { a.m_value.value() },
            LineHeight::Fixed { b.m_value.value() },
            context
        );
    }
}

} // namespace Style
} // namespace WebCore
