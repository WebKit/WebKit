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
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

CSS::Position toCSS(const Position& position, const RenderStyle& style)
{
    return CSS::TwoComponentPosition { Style::toCSS(get<0>(position), style), Style::toCSS(get<1>(position), style) };
}

} // namespace Style

namespace CSS {

static Style::LengthPercentage makeLengthPercentageForRoot(Calculation::Child&& root)
{
    return Style::LengthPercentage {
        CalculationValue::create(
            Calculation::Tree {
                .root = WTFMove(root),
                .category = Calculation::Category::LengthPercentage,
                .range = ValueRange::All
            }
        )
    };
}

Style::Position toStyle(const CSS::Position& position, Style::BuilderState& state, const CSSCalcSymbolTable&)
{
    auto convertTo100PercentMinus = [](Style::LengthPercentage value) -> Style::LengthPercentage {
        return value.value.switchOn(
            [](const Style::Length& length) {
                if (!length.value)
                    return Style::LengthPercentage { Style::Percentage { 100 } };
                return makeLengthPercentageForRoot(Calculation::subtract(Calculation::percentage(100), Calculation::dimension(length.value)));
            },
            [](const Style::Percentage& percentage) {
                return Style::LengthPercentage { Style::Percentage { 100 - percentage.value } };
            },
            [](const CalculationValue& calculation) {
                return makeLengthPercentageForRoot(Calculation::subtract(Calculation::percentage(100), calculation.copyRoot()));
            }
        );
    };

    return WTF::switchOn(position,
        [&](const CSS::TwoComponentPosition& twoComponent) {
            auto horizontal = WTF::switchOn(get<0>(twoComponent),
                [&](CSS::Left) {
                    return Style::LengthPercentage { Style::Percentage { 0 } };
                },
                [&](CSS::Right)  {
                    return Style::LengthPercentage { Style::Percentage { 100 } };
                },
                [&](CSS::Center)  {
                    return Style::LengthPercentage { Style::Percentage { 50 } };
                },
                [&](const CSS::LengthPercentage& value) {
                    return toStyle(value, state, CSSCalcSymbolTable { });
                }
            );
            auto vertical = WTF::switchOn(get<1>(twoComponent),
                [&](CSS::Top) {
                    return Style::LengthPercentage { Style::Percentage { 0 } };
                },
                [&](CSS::Bottom) {
                    return Style::LengthPercentage { Style::Percentage { 100 } };
                },
                [&](CSS::Center) {
                    return Style::LengthPercentage { Style::Percentage { 50 } };
                },
                [&](const CSS::LengthPercentage& value) {
                    return toStyle(value, state, CSSCalcSymbolTable { });
                }
            );
            return Style::Position { WTFMove(horizontal), WTFMove(vertical) };
        },
        [&](const CSS::FourComponentPosition& fourComponent) {
            auto horizontal = WTF::switchOn(get<0>(get<0>(fourComponent)),
                [&](CSS::Left) {
                    return toStyle(get<1>(get<0>(fourComponent)), state, CSSCalcSymbolTable { });
                },
                [&](CSS::Right) {
                    return convertTo100PercentMinus(toStyle(get<1>(get<0>(fourComponent)), state, CSSCalcSymbolTable { }));
                }
            );
            auto vertical = WTF::switchOn(get<0>(get<1>(fourComponent)),
                [&](CSS::Top) {
                    return toStyle(get<1>(get<1>(fourComponent)), state, CSSCalcSymbolTable { });
                },
                [&](CSS::Bottom) {
                    return convertTo100PercentMinus(toStyle(get<1>(get<1>(fourComponent)), state, CSSCalcSymbolTable { }));
                }
            );
            return Style::Position { WTFMove(horizontal), WTFMove(vertical) };
        }
    );
}

} // namespace CSS
} // namespace WebCore
