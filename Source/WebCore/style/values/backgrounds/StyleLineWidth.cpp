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
#include "StyleLineWidth.h"

#include "CSSKeywordValue.h"
#include "Document.h"
#include "Settings.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "StyleSnapLengthAsBorderWidth.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<LineWidth>::operator()(BuilderState& state, const CSSValue& value) -> LineWidth
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueThin:
            return CSS::Keyword::Thin { };
        case CSSValueMedium:
            return CSS::Keyword::Medium { };
        case CSSValueThick:
            return CSS::Keyword::Thick { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Medium { };
        }
    }

    return snapLengthAsBorderWidth(toStyleFromCSSValue<LineWidth::Length>(state, value), state.style().deviceScaleFactor());
}

// MARK: - Blending

auto Blending<LineWidth>::blend(const LineWidth& a, const LineWidth& b, const Style::ComputedStyle& aStyle, const Style::ComputedStyle& bStyle, const Interpolation::Context& context) -> LineWidth
{
    auto blendedValue = Style::blend(a.value, b.value, aStyle, bStyle, context);
    if (RefPtr document = context.client.document())
        return snapLengthAsBorderWidth(blendedValue, document->deviceScaleFactor());
    return blendedValue;
}

// MARK: - Evaluation

auto Evaluation<LineWidth, float>::operator()(const LineWidth& value, ZoomFactor zoom, float deviceScaleFactor) -> float
{
    return snapLengthAsBorderWidth(evaluate<float>(value.value, zoom), deviceScaleFactor);
}

auto Evaluation<LineWidth, LayoutUnit>::operator()(const LineWidth& value, ZoomFactor zoom, float deviceScaleFactor) -> LayoutUnit
{
    // NOTE: Using `evaluate<float>`, not `evaluate<LayoutUnit>`, as snapLengthAsBorderWidth takes a `float`.
    return LayoutUnit { snapLengthAsBorderWidth(evaluate<float>(value.value, zoom), deviceScaleFactor) };
}

auto Evaluation<LineWidthBox, FloatBoxExtent>::operator()(const LineWidthBox& value, ZoomFactor zoom, float deviceScaleFactor) -> FloatBoxExtent
{
    return {
        evaluate<float>(value.top(), zoom, deviceScaleFactor),
        evaluate<float>(value.right(), zoom, deviceScaleFactor),
        evaluate<float>(value.bottom(), zoom, deviceScaleFactor),
        evaluate<float>(value.left(), zoom, deviceScaleFactor),
    };
}

auto Evaluation<LineWidthBox, LayoutBoxExtent>::operator()(const LineWidthBox& value, ZoomFactor zoom, float deviceScaleFactor) -> LayoutBoxExtent
{
    return {
        evaluate<LayoutUnit>(value.top(), zoom, deviceScaleFactor),
        evaluate<LayoutUnit>(value.right(), zoom, deviceScaleFactor),
        evaluate<LayoutUnit>(value.bottom(), zoom, deviceScaleFactor),
        evaluate<LayoutUnit>(value.left(), zoom, deviceScaleFactor),
    };
}

} // namespace Style
} // namespace WebCore
