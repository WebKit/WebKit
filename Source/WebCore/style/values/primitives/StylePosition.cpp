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

using namespace CSS::Literals;

auto ToStyle<CSS::TwoComponentPositionHorizontal>::operator()(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state) -> TwoComponentPositionHorizontal
{
    return WTF::switchOn(value.offset,
        [&](CSS::Keyword::Left) {
            return TwoComponentPositionHorizontal { 0_css_percentage };
        },
        [&](CSS::Keyword::Right) {
            return TwoComponentPositionHorizontal { 100_css_percentage };
        },
        [&](CSS::Keyword::Center) {
            return TwoComponentPositionHorizontal { 50_css_percentage };
        },
        [&](const CSS::LengthPercentage<>& value) {
            return TwoComponentPositionHorizontal { toStyle(value, state) };
        }
    );
}

auto ToStyle<CSS::TwoComponentPositionVertical>::operator()(const CSS::TwoComponentPositionVertical& value, const BuilderState& state) -> TwoComponentPositionVertical
{
    return WTF::switchOn(value.offset,
        [&](CSS::Keyword::Top) {
            return TwoComponentPositionVertical { 0_css_percentage };
        },
        [&](CSS::Keyword::Bottom) {
            return TwoComponentPositionVertical { 100_css_percentage };
        },
        [&](CSS::Keyword::Center) {
            return TwoComponentPositionVertical { 50_css_percentage };
        },
        [&](const CSS::LengthPercentage<>& value) {
            return TwoComponentPositionVertical { toStyle(value, state) };
        }
    );
}

auto ToCSS<Position>::operator()(const Position& value, const RenderStyle& style) -> CSS::Position
{
    return CSS::TwoComponentPosition { { toCSS(value.x(), style) }, { toCSS(value.y(), style) } };
}

auto ToStyle<CSS::Position>::operator()(const CSS::Position& position, const BuilderState& state) -> Position
{
    return WTF::switchOn(position,
        [&](const CSS::TwoComponentPosition& twoComponent) {
            return Position {
                toStyle(get<0>(twoComponent), state),
                toStyle(get<1>(twoComponent), state)
            };
        },
        [&](const CSS::FourComponentPosition& fourComponent) {
            auto horizontal = WTF::switchOn(get<0>(get<0>(fourComponent)),
                [&](CSS::Keyword::Left) {
                    return toStyle(get<1>(get<0>(fourComponent)), state);
                },
                [&](CSS::Keyword::Right) {
                    return reflect(toStyle(get<1>(get<0>(fourComponent)), state));
                }
            );
            auto vertical = WTF::switchOn(get<0>(get<1>(fourComponent)),
                [&](CSS::Keyword::Top) {
                    return toStyle(get<1>(get<1>(fourComponent)), state);
                },
                [&](CSS::Keyword::Bottom) {
                    return reflect(toStyle(get<1>(get<1>(fourComponent)), state));
                }
            );
            return Position { WTFMove(horizontal), WTFMove(vertical) };
        }
    );
}

// MARK: - Evaluation

auto Evaluation<Position>::operator()(const Position& position, FloatSize referenceBox) -> FloatPoint
{
    return evaluate(position.value, referenceBox);
}

} // namespace CSS
} // namespace WebCore
