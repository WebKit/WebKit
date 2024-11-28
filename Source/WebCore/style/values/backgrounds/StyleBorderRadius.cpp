/*
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

#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

auto ToCSS<BorderRadius>::operator()(const BorderRadius& value, const RenderStyle& style) -> CSS::BorderRadius
{
    return {
        .horizontal {
            toCSS(value.topLeft.width(), style),
            toCSS(value.topRight.width(), style),
            toCSS(value.bottomRight.width(), style),
            toCSS(value.bottomLeft.width(), style),
        },
        .vertical {
            toCSS(value.topLeft.height(), style),
            toCSS(value.topRight.height(), style),
            toCSS(value.bottomRight.height(), style),
            toCSS(value.bottomLeft.height(), style),
        },
    };
}

auto ToStyle<CSS::BorderRadius>::operator()(const CSS::BorderRadius& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> BorderRadius
{
    return {
        .topLeft { toStyle(value.topLeft(), state, symbolTable) },
        .topRight { toStyle(value.topRight(), state, symbolTable) },
        .bottomRight { toStyle(value.bottomRight(), state, symbolTable) },
        .bottomLeft { toStyle(value.bottomLeft(), state, symbolTable) },
    };
}

FloatRoundedRect::Radii evaluate(const BorderRadius& value, FloatSize referenceBox)
{
    return {
        evaluate(value.topLeft, referenceBox),
        evaluate(value.topRight, referenceBox),
        evaluate(value.bottomLeft, referenceBox),
        evaluate(value.bottomRight, referenceBox)
    };
}

} // namespace Style
} // namespace WebCore
