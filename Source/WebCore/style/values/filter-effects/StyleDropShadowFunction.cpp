/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StyleDropShadowFunction.h"

#include "CSSDropShadowFunction.h"
#include "FEDropShadow.h"
#include "FilterOperation.h"
#include "RenderStyle.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleColor.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

DropShadow DropShadow::passthroughForInterpolation()
{
    using Descriptor = CSSFilterFunctionDescriptor<CSSValueDropShadow>;

    return {
        .color = Descriptor::initialColorValueForInterpolation,
        .location = {
            Descriptor::initialLengthValueForInterpolation,
            Descriptor::initialLengthValueForInterpolation,
        },
        .stdDeviation = Descriptor::initialLengthValueForInterpolation,
    };
}

IntOutsets DropShadow::calculateOutsets(ZoomFactor zoom) const
{
    // FIXME: Style::roundForImpreciseConversion<int> only being used to match FilterOperation behavior.
    auto x = Style::roundForImpreciseConversion<int>(evaluate<float>(this->location.x(), zoom));
    auto y = Style::roundForImpreciseConversion<int>(evaluate<float>(this->location.y(), zoom));
    auto stdDeviation = Style::roundForImpreciseConversion<int>(evaluate<float>(this->stdDeviation, zoom));

    return FEDropShadow::calculateOutsets(FloatSize(x, y), FloatSize(stdDeviation, stdDeviation));
}

// MARK: - Conversion

auto ToCSS<DropShadow>::operator()(const DropShadow& value, const RenderStyle& style) -> CSS::DropShadow
{
    return {
        .color = toCSS(value.color, style),
        .location = toCSS(value.location, style),
        .stdDeviation = toCSS(value.stdDeviation, style),
    };
}

auto ToStyle<CSS::DropShadow>::operator()(const CSS::DropShadow& value, const BuilderState& state) -> DropShadow
{
    using Descriptor = CSSFilterFunctionDescriptor<CSSValueDropShadow>;

    return {
        .color = value.color ? toStyle(*value.color, state, ForVisitedLink::No) : Style::Color { Descriptor::defaultColorValue },
        .location = toStyle(value.location, state),
        .stdDeviation = value.stdDeviation ? toStyle(*value.stdDeviation, state) : Length<CSS::NonnegativeUnzoomed> { Descriptor::defaultStdDeviationValue },
    };
}

// MARK: - Evaluation

auto Evaluation<DropShadow, Ref<FilterEffect>>::operator()(const DropShadow& value, const RenderStyle& style) -> Ref<FilterEffect>
{
    auto zoom = style.usedZoomForLength();

    // FIXME: Style::roundForImpreciseConversion<int> only being used to match FilterOperation behavior.
    auto x = Style::roundForImpreciseConversion<int>(evaluate<float>(value.location.x(), zoom));
    auto y = Style::roundForImpreciseConversion<int>(evaluate<float>(value.location.y(), zoom));
    auto stdDeviation = Style::roundForImpreciseConversion<int>(evaluate<float>(value.stdDeviation, zoom));

    return FEDropShadow::create(stdDeviation, stdDeviation, x, y, value.color.resolveColor(style.color()), 1);
}

// MARK: - Platform

auto ToPlatform<DropShadow>::operator()(const DropShadow& value, const RenderStyle& style) -> Ref<FilterOperation>
{
    auto zoom = style.usedZoomForLength();

    return DropShadowFilterOperation::create(
        value.color.resolveColor(style.color()),
        IntPoint {
            Style::roundForImpreciseConversion<int>(evaluate<float>(value.location.x(), zoom)),
            Style::roundForImpreciseConversion<int>(evaluate<float>(value.location.y(), zoom)),
        },
        Style::roundForImpreciseConversion<int>(evaluate<float>(value.stdDeviation, zoom))
    );
}

} // namespace Style
} // namespace WebCore
