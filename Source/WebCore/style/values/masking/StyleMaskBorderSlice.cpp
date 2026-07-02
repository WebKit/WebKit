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
#include "StyleMaskBorderSlice.h"

#include "AnimationUtilities.h"
#include "CSSMaskBorderSliceValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<MaskBorderSlice>::operator()(const MaskBorderSlice& value, const Style::ComputedStyle& style) -> CSS::MaskBorderSlice
{
    return { toCSS(value.values, style), value.fill };
}

auto ToStyle<CSS::MaskBorderSlice>::operator()(const CSS::MaskBorderSlice& value, const BuilderState& state) -> MaskBorderSlice
{
    return { toStyle(value.values, state), value.fill };
}

auto CSSValueConversion<MaskBorderSlice>::operator()(BuilderState& state, const CSSValue& value) -> MaskBorderSlice
{
    if (RefPtr sliceValue = dynamicDowncast<CSSMaskBorderSliceValue>(value))
        return toStyle(sliceValue->slices(), state);

    // Values coming from CSS Typed OM may not have been converted to a CSSMaskBorderSliceValue.
    return toStyleFromCSSValue<MaskBorderSlice::Value>(state, value);
}

auto CSSValueCreation<MaskBorderSlice>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const MaskBorderSlice& value) -> Ref<CSSValue>
{
    return CSSMaskBorderSliceValue::create(toCSS(value, style));
}

// MARK: - Blending

auto Blending<MaskBorderSlice>::canBlend(const MaskBorderSlice& a, const MaskBorderSlice& b) -> bool
{
    if (a.fill != b.fill)
        return false;

    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderSlice>::requiresInterpolationForAccumulativeIteration(const MaskBorderSlice& a, const MaskBorderSlice& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderSlice>::blend(const MaskBorderSlice& a, const MaskBorderSlice& b, const BlendingContext& context) -> MaskBorderSlice
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return MaskBorderSlice {
        Style::blend(a.values.top(),     b.values.top(), context),
        Style::blend(a.values.right(),   b.values.right(), context),
        Style::blend(a.values.bottom(),  b.values.bottom(), context),
        Style::blend(a.values.left(),    b.values.left(), context),
        (!context.progress || !context.isDiscrete ? a : b).fill,
    };
}

} // namespace Style
} // namespace WebCore
