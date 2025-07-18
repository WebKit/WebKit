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

#include "config.h"
#include "StyleBorderRadius.h"

#include "CSSPrimitiveNumericUnits.h"
#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto ToCSS<BorderRadius>::operator()(const BorderRadius& value, const RenderStyle& style) -> CSS::BorderRadius
{
    return {
        .horizontal {
            toCSS(value.topLeft().width(), style),
            toCSS(value.topRight().width(), style),
            toCSS(value.bottomRight().width(), style),
            toCSS(value.bottomLeft().width(), style),
        },
        .vertical {
            toCSS(value.topLeft().height(), style),
            toCSS(value.topRight().height(), style),
            toCSS(value.bottomRight().height(), style),
            toCSS(value.bottomLeft().height(), style),
        },
    };
}

auto ToStyle<CSS::BorderRadius>::operator()(const CSS::BorderRadius& value, const BuilderState& state) -> BorderRadius
{
    return {
        toStyle(value.topLeft(), state),
        toStyle(value.topRight(), state),
        toStyle(value.bottomLeft(), state),
        toStyle(value.bottomRight(), state),
    };
}

auto CSSValueConversion<BorderRadiusValue>::operator()(BuilderState& state, const CSSValue& value) -> BorderRadiusValue
{
    if (!value.isPair())
        return { 0_css_px, 0_css_px };

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return { 0_css_px, 0_css_px };

    return {
        toStyleFromCSSValue<LengthPercentage<CSS::Nonnegative>>(state, pair->first),
        toStyleFromCSSValue<LengthPercentage<CSS::Nonnegative>>(state, pair->second)
    };
}

// MARK: - Evaluation

auto Evaluation<BorderRadius>::operator()(const BorderRadius& value, FloatSize referenceSize) -> FloatRoundedRect::Radii
{
    return {
        evaluate(value.topLeft(), referenceSize),
        evaluate(value.topRight(), referenceSize),
        evaluate(value.bottomLeft(), referenceSize),
        evaluate(value.bottomRight(), referenceSize),
    };
}

auto Evaluation<BorderRadius>::operator()(const BorderRadius& value, LayoutSize referenceSize) -> LayoutRoundedRect::Radii
{
    return {
        evaluate(value.topLeft(), referenceSize),
        evaluate(value.topRight(), referenceSize),
        evaluate(value.bottomLeft(), referenceSize),
        evaluate(value.bottomRight(), referenceSize),
    };
}

} // namespace Style
} // namespace WebCore
