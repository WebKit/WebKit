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
        .copyForLineHeight();

    using StyleLengthPercentage = LengthPercentage<CSS::Nonnegative>;
    using CSSRaw = typename StyleLengthPercentage::CSS::Raw;
    using CSSDimensionRaw = typename CSSRaw::Dimension;
    using CSSPercentageRaw = typename CSSRaw::Percentage;

    using StyleNumber = Number<CSS::Nonnegative>;
    using CSSNumberRaw = typename StyleNumber::CSS::Raw;;

    auto handleLength = [&](const auto& length) {
        return CSS::clampingToRangeOf<LineHeight::Length>(
            length.unresolvedValue() * multiplier
        );
    };

    auto handlePercentage = [&](const auto& percentage) {
        auto textZoomFactor = ZoomFactor { state.zoomWithTextZoomFactor() };
        auto percentageBasis = state.style().fontDescription().unzoomedComputedSize();

        // FIXME: percentage should not be restricted to an integer here.
        auto percentageValue = static_cast<int>(percentage.value);

        return CSS::clampingToRangeOf<LineHeight::Length>(
            (percentageValue * percentageBasis * textZoomFactor.value) / 100.0
        );
    };

    auto handleCalc = [&](const auto& calc) {
        auto textZoomFactor = ZoomFactor { state.zoomWithTextZoomFactor() };
        auto percentageBasis = state.style().fontDescription().unzoomedComputedSize();

        return CSS::clampingToRangeOf<LineHeight::Length>(
            evaluate<float>(calc, percentageBasis, textZoomFactor) * multiplier
        );
    };

    auto handleNumber = [&](const auto& number) {
        return CSS::clampingToRangeOf<LineHeight::Number>(
            number.value
        );
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
                [&](const StyleLengthPercentage::Dimension& length) {
                    return handleLength(length);
                },
                [&](const StyleLengthPercentage::Percentage& percentage) {
                    return handlePercentage(percentage);
                },
                [&](const StyleLengthPercentage::Calc& calc) {
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
                return handleLength(toStyle(CSSDimensionRaw(*unit, raw.value), conversionData));

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    );
}

// MARK: - Blending

auto Blending<LineHeight>::canBlend(const LineHeight& a, const LineHeight& b) -> bool
{
    return a.hasSameType(b) && a.isNumeric() && b.isNumeric();
}

auto Blending<LineHeight>::requiresInterpolationForAccumulativeIteration(const LineHeight& a, const LineHeight& b) -> bool
{
    return !a.hasSameType(b);
}

auto Blending<LineHeight>::blend(const LineHeight& a, const LineHeight& b, const BlendingContext& context) -> LineHeight
{
    if (!a.hasSameType(b) || !a.isNumeric() || !b.isNumeric())
        return context.progress < 0.5 ? a : b;

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> LineHeight {
            return LineHeight { WebCore::Style::blend(a, b, context) };
        },
        [](const auto&, const auto&) -> LineHeight {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

// MARK: - Evaluation

auto Evaluation<LineHeight, float>::operator()(
    const LineHeight& lineHeight, LineHeightEvaluationContext context, ZoomFactor zoom) -> float
{
    return WTF::switchOn(lineHeight,
        [&](const CSS::Keyword::Normal&) {
            return context.lineSpacing;
        },
        [&](const LineHeight::Length& length) {
            return evaluate<LayoutUnit>(length, zoom).toFloat();
        },
        [&](const LineHeight::Number& number) {
            return LayoutUnit { number.value * LayoutUnit { context.computedFontSize } }.toFloat();
        }
    );
}

} // namespace Style
} // namespace WebCore
