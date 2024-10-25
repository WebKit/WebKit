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
#include "StylePosition.h"

#include "CalculationCategory.h"
#include "CalculationTree.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

auto ToCSS<TwoComponentPositionHorizontal>::operator()(const TwoComponentPositionHorizontal& value, const RenderStyle& style) -> CSS::TwoComponentPositionHorizontal
{
    return { .offset = toCSS(value.offset, style) };
}

auto ToStyle<CSS::TwoComponentPositionHorizontal>::operator()(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> TwoComponentPositionHorizontal
{
    return WTF::switchOn(value.offset,
        [&](CSS::Left) {
            return TwoComponentPositionHorizontal { .offset = LengthPercentage<> { Percentage<> { 0 } } };
        },
        [&](CSS::Right)  {
            return TwoComponentPositionHorizontal { .offset = LengthPercentage<> { Percentage<> { 100 } } };
        },
        [&](CSS::Center)  {
            return TwoComponentPositionHorizontal { .offset = LengthPercentage<> { Percentage<> { 50 } } };
        },
        [&](const CSS::LengthPercentage<>& value) {
            return TwoComponentPositionHorizontal { .offset = toStyle(value, state, symbolTable) };
        }
    );
}

auto ToCSS<TwoComponentPositionVertical>::operator()(const TwoComponentPositionVertical& value, const RenderStyle& style) -> CSS::TwoComponentPositionVertical
{
    return { .offset = toCSS(value.offset, style) };
}

auto ToStyle<CSS::TwoComponentPositionVertical>::operator()(const CSS::TwoComponentPositionVertical& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> TwoComponentPositionVertical
{
    return WTF::switchOn(value.offset,
        [&](CSS::Top) {
            return TwoComponentPositionVertical { .offset = LengthPercentage<> { Percentage<> { 0 } } };
        },
        [&](CSS::Bottom) {
            return TwoComponentPositionVertical { .offset = LengthPercentage<> { Percentage<> { 100 } } };
        },
        [&](CSS::Center) {
            return TwoComponentPositionVertical { .offset = LengthPercentage<> { Percentage<> { 50 } } };
        },
        [&](const CSS::LengthPercentage<>& value) {
            return TwoComponentPositionVertical { .offset = toStyle(value, state, symbolTable) };
        }
    );
}

auto ToCSS<Position>::operator()(const Position& value, const RenderStyle& style) -> CSS::Position
{
    return CSS::TwoComponentPosition { { toCSS(value.x(), style) }, { toCSS(value.y(), style) } };
}

auto ToStyle<CSS::Position>::operator()(const CSS::Position& position, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Position
{
    return WTF::switchOn(position,
        [&](const CSS::TwoComponentPosition& twoComponent) {
            return Position {
                toStyle(get<0>(twoComponent), state, symbolTable),
                toStyle(get<1>(twoComponent), state, symbolTable)
            };
        },
        [&](const CSS::FourComponentPosition& fourComponent) {
            auto horizontal = WTF::switchOn(get<0>(get<0>(fourComponent)),
                [&](CSS::Left) {
                    return toStyle(get<1>(get<0>(fourComponent)), state, symbolTable);
                },
                [&](CSS::Right) {
                    return reflect(toStyle(get<1>(get<0>(fourComponent)), state, symbolTable));
                }
            );
            auto vertical = WTF::switchOn(get<0>(get<1>(fourComponent)),
                [&](CSS::Top) {
                    return toStyle(get<1>(get<1>(fourComponent)), state, symbolTable);
                },
                [&](CSS::Bottom) {
                    return reflect(toStyle(get<1>(get<1>(fourComponent)), state, symbolTable));
                }
            );
            return Position { WTFMove(horizontal), WTFMove(vertical) };
        }
    );
}

// MARK: - Evaluation

FloatPoint evaluate(const Position& position, FloatSize referenceBox)
{
    return evaluate(position.value, referenceBox);
}

float evaluate(const TwoComponentPositionHorizontal& component, float referenceWidth)
{
    return evaluate(component.offset, referenceWidth);
}

float evaluate(const TwoComponentPositionVertical& component, float referenceHeight)
{
    return evaluate(component.offset, referenceHeight);
}

} // namespace CSS
} // namespace WebCore
