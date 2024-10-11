/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleGradient.h"

#include "ComputedStyleExtractor.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion: Style -> CSS

static RefPtr<CSSPrimitiveValue> toCSSColorStopColor(const std::optional<StyleColor>& color, const RenderStyle& style)
{
    if (!color)
        return nullptr;
    return ComputedStyleExtractor::currentColorOrValidColor(style, *color);
}

static auto toCSSColorStopPosition(const GradientLinearColorStop::Position& position, const RenderStyle& style) -> CSS::GradientLinearColorStopPosition
{
    return toCSS(position, style);
}

static auto toCSSColorStopPosition(const GradientAngularColorStop::Position& position, const RenderStyle& style) -> CSS::GradientAngularColorStopPosition
{
    return toCSS(position, style);
}

static auto toCSSColorStopPosition(const GradientDeprecatedColorStop::Position& position, const RenderStyle&) -> CSS::GradientDeprecatedColorStopPosition
{
    return CSS::NumberRaw { position.value };
}

template<typename CSSStop, typename StyleStop> static auto toCSSColorStop(const StyleStop& stop, const RenderStyle& style) -> CSSStop
{
    return CSSStop {
        toCSSColorStopColor(stop.color, style),
        toCSSColorStopPosition(stop.position, style)
    };
}

auto ToCSS<GradientAngularColorStop>::operator()(const GradientAngularColorStop& stop, const RenderStyle& style) -> CSS::GradientAngularColorStop
{
    return toCSSColorStop<CSS::GradientAngularColorStop>(stop, style);
}

auto ToCSS<GradientLinearColorStop>::operator()(const GradientLinearColorStop& stop, const RenderStyle& style) -> CSS::GradientLinearColorStop
{
    return toCSSColorStop<CSS::GradientLinearColorStop>(stop, style);
}

auto ToCSS<GradientDeprecatedColorStop>::operator()(const GradientDeprecatedColorStop& stop, const RenderStyle& style) -> CSS::GradientDeprecatedColorStop
{
    return toCSSColorStop<CSS::GradientDeprecatedColorStop>(stop, style);
}

// MARK: - Conversion: CSS -> Style

static auto toStyleStopColor(const RefPtr<CSSPrimitiveValue>& color, BuilderState& state, const CSSCalcSymbolTable&) -> std::optional<StyleColor>
{
    if (!color)
        return std::nullopt;
    return state.colorFromPrimitiveValue(*color);
}

static auto toStyleStopPosition(const CSS::GradientLinearColorStopPosition& position, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientLinearColorStopPosition
{
    return toStyle(position, state, symbolTable);
}

static auto toStyleStopPosition(const CSS::GradientAngularColorStopPosition& position, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientAngularColorStopPosition
{
    return toStyle(position, state, symbolTable);
}

static auto toStyleStopPosition(const CSS::GradientDeprecatedColorStopPosition& position, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientDeprecatedColorStopPosition
{
    return WTF::switchOn(position,
        [&](const CSS::Number& number) {
            return toStyle(number, state, symbolTable);
        },
        [&](const CSS::Percentage& percentage) {
            return Number { toStyle(percentage, state, symbolTable).value / 100.0f };
        }
    );
}

template<typename T> decltype(auto) toStyleColorStop(const T& stop, BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return GradientColorStop {
        toStyleStopColor(stop.color, state, symbolTable),
        toStyleStopPosition(stop.position, state, symbolTable)
    };
}

auto ToStyle<CSS::GradientAngularColorStop>::operator()(const CSS::GradientAngularColorStop& stop, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientAngularColorStop
{
    return toStyleColorStop(stop, state, symbolTable);
}

auto ToStyle<CSS::GradientLinearColorStop>::operator()(const CSS::GradientLinearColorStop& stop, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientLinearColorStop
{
    return toStyleColorStop(stop, state, symbolTable);
}

auto ToStyle<CSS::GradientDeprecatedColorStop>::operator()(const CSS::GradientDeprecatedColorStop& stop, BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> GradientDeprecatedColorStop
{
    return toStyleColorStop(stop, state, symbolTable);
}

} // namespace Style
} // namespace WebCore
