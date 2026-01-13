/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveValue.h"
#include "Document.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

static auto handleKeywordValue(BuilderState& state, CSSValueID valueID) -> LineWidth
{
    float keywordValue;
    switch (valueID) {
    case CSSValueThin:
        keywordValue = 1.0f;
        break;
    case CSSValueMedium:
        keywordValue = 3.0f;
        break;
    case CSSValueThick:
        keywordValue = 5.0f;
        break;
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        keywordValue = 3.0f; // Medium
        break;
    }

    return LineWidth::Length { floorToDevicePixel(keywordValue * state.style().usedZoom(), state.document().deviceScaleFactor()) };
}

template<typename T> static T snapLengthAsBorderWidth(T length, float deviceScaleFactor)
{
    // https://drafts.csswg.org/css-values-4/#snap-a-length-as-a-border-width

    // 1. Assert: `length` is non-negative.
    // NOTE: Not asserted, but checked in step 3.

    // 2. If `length` is an integer number of device pixels, do nothing.
    // NOTE: Handled by step 4 without explicitly checking here.

    // 3. If `length` is greater than zero, but less than 1 device pixel, round `length` up to 1 device pixel.
    if (auto singleDevicePixelLength = T { 1.0f / deviceScaleFactor }; length > T { 0 } && length < singleDevicePixelLength)
        return singleDevicePixelLength;

    // 4. If `length` is greater than 1 device pixel, round it down to the nearest integer number of device pixels.
    return T { floorToDevicePixel(length, deviceScaleFactor) };
}

auto CSSValueConversion<LineWidth>::operator()(BuilderState& state, const CSSValue& value) -> LineWidth
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return LineWidth::Length { 3.0f };

    if (!state.cssToLengthConversionData().evaluationTimeZoomEnabled()) {
        if (primitiveValue->isValueID())
            return handleKeywordValue(state, primitiveValue->valueID());

        // Any original result that was >= 1 should not be allowed to fall below 1. This keeps border lines from vanishing.

        auto result = primitiveValue->resolveAsLength<float>(state.cssToLengthConversionData());
        if (state.style().usedZoom() < 1.0f && result < 1.0f) {
            auto originalLength = primitiveValue->resolveAsLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            if (originalLength >= 1.0f)
                return LineWidth::Length { 1.0f };
        }

        return LineWidth::Length { snapLengthAsBorderWidth(result, state.document().deviceScaleFactor()) };
    }

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueThin:
            return LineWidth { 1.0f };
        case CSSValueMedium:
            return LineWidth { 3.0f };
        case CSSValueThick:
            return LineWidth { 5.0f };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return LineWidth { 3.0f };
        }
    }

    auto result = toStyleFromCSSValue<LineWidth::Length>(state, *primitiveValue);
    return LineWidth::Length { snapLengthAsBorderWidth(result.unresolvedValue(), state.document().deviceScaleFactor()) };
}

// MARK: - Blending

auto Blending<LineWidth>::blend(const LineWidth& a, const LineWidth& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const Interpolation::Context& context) -> LineWidth
{
    auto blendedValue = Style::blend(a.value, b.value, aStyle, bStyle, context);
    if (RefPtr document = context.client.document())
        return LineWidth::Length { snapLengthAsBorderWidth(blendedValue.unresolvedValue(), document->deviceScaleFactor()) };
    return blendedValue;
}

// MARK: - Evaluation

auto Evaluation<LineWidth, float>::operator()(const LineWidth& value, ZoomFactor zoom, float deviceScaleFactor) -> float
{
    return snapLengthAsBorderWidth(evaluate<float>(value.value, zoom), deviceScaleFactor);
}

auto Evaluation<LineWidth, LayoutUnit>::operator()(const LineWidth& value, ZoomFactor zoom, float deviceScaleFactor) -> LayoutUnit
{
    return snapLengthAsBorderWidth(evaluate<LayoutUnit>(value.value, zoom), deviceScaleFactor);
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
