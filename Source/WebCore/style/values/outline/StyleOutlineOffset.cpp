/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleOutlineOffset.h"

#include "CSSKeywordValue.h"
#include "Document.h"
#include "StyleBuilderState.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleSnapLengthAsBorderWidth.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<OutlineOffset>::operator()(BuilderState& state, const CSSValue& value) -> OutlineOffset
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        if (keywordValue->valueID() == CSSValueInset)
            return CSS::Keyword::Inset { };
    }

    return snapSignedLengthAsBorderWidth(toStyleFromCSSValue<Length<>>(state, value), state.style().deviceScaleFactor());
}

// MARK: - Blending

auto Blending<OutlineOffset>::canBlend(const OutlineOffset& a, const OutlineOffset& b) -> bool
{
    if (a.isInset() || b.isInset())
        return false;
    return Style::canBlend(*a.tryLength(), *b.tryLength());
}

auto Blending<OutlineOffset>::blend(const OutlineOffset& a, const OutlineOffset& b, const Style::ComputedStyle& aStyle, const Style::ComputedStyle& bStyle, const Interpolation::Context& context) -> OutlineOffset
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    auto blendedValue = Style::blend(*a.tryLength(), *b.tryLength(), aStyle, bStyle, context);
    if (RefPtr document = context.client.document())
        return snapSignedLengthAsBorderWidth(blendedValue, document->deviceScaleFactor());
    return blendedValue;
}

// MARK: - Evaluation

auto Evaluation<UsedOutlineOffset, float>::operator()(const UsedOutlineOffset& value, ZoomFactor zoom, float deviceScaleFactor) -> float
{
    return snapSignedLengthAsBorderWidth(evaluate<float>(value.value, zoom), deviceScaleFactor);
}

auto Evaluation<UsedOutlineOffset, LayoutUnit>::operator()(const UsedOutlineOffset& value, ZoomFactor zoom, float deviceScaleFactor) -> LayoutUnit
{
    // NOTE: Using `evaluate<float>`, not `evaluate<LayoutUnit>`, as snapSignedLengthAsBorderWidth takes a `float`.
    return LayoutUnit { snapSignedLengthAsBorderWidth(evaluate<float>(value.value, zoom), deviceScaleFactor) };
}

} // namespace Style
} // namespace WebCore
