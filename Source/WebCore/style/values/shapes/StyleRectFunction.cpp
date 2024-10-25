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
#include "StyleRectFunction.h"

#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToStyle<CSS::Rect>::operator()(const CSS::Rect& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Inset
{
    // "An auto value makes the edge of the box coincide with the corresponding edge of the
    //  reference box: it’s equivalent to 0% as the first (top) or fourth (left) value, and
    //  equivalent to 100% as the second (right) or third (bottom) value."
    //      (https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-rect)

    // Conversion applies reflection to the trailing (right/bottom) edges to convert from rect()
    // form to inset() form. This means that all the `auto` values are converted to 0%.

    auto convertLeadingEdge = [&](const std::variant<CSS::LengthPercentage<>, CSS::Auto>& edge) -> LengthPercentage<> {
        return WTF::switchOn(edge,
            [&](const CSS::LengthPercentage<>& value) -> LengthPercentage<> {
                return toStyle(value, state, symbolTable);
            },
            [&](const CSS::Auto&) -> LengthPercentage<> {
                return { Percentage<> { 0 } };
            }
        );
    };

    auto convertTrailingEdge = [&](const std::variant<CSS::LengthPercentage<>, CSS::Auto>& edge) -> LengthPercentage<> {
        return WTF::switchOn(edge,
            [&](const CSS::LengthPercentage<>& value) -> LengthPercentage<> {
                return reflect(toStyle(value, state, symbolTable));
            },
            [&](const CSS::Auto&) -> LengthPercentage<> {
                return { Percentage<> { 0 } };
            }
        );
    };

    return {
        .insets = {
            convertLeadingEdge(value.edges.top()),
            convertTrailingEdge(value.edges.right()),
            convertTrailingEdge(value.edges.bottom()),
            convertLeadingEdge(value.edges.left()),
        },
        .radii = toStyle(value.radii, state, symbolTable)
    };
}

auto ToStyle<CSS::RectFunction>::operator()(const CSS::RectFunction& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> InsetFunction
{
    return { toStyle(value.parameters, state, symbolTable) };
}

} // namespace Style
} // namespace WebCore
