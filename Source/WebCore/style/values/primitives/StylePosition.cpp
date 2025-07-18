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
#include "StylePosition.h"

#include "CSSPositionValue.h"
#include "CalculationCategory.h"
#include "CalculationTree.h"
#include "LengthPoint.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Platform.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

Position::Position(const WebCore::LengthPoint& point)
    : x { point.x }
    , y { point.y }
{
}

// MARK: Core Keyword Resolution

static auto resolveKeyword(CSS::Keyword::Top, const BuilderState&) -> LengthPercentage<>
{
    return 0_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Top, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return toStyle(length, state);
}

static auto resolveKeyword(CSS::Keyword::Right, const BuilderState&) -> LengthPercentage<>
{
    return 100_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Right, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return reflect(toStyle(length, state));
}

static auto resolveKeyword(CSS::Keyword::Bottom, const BuilderState&) -> LengthPercentage<>
{
    return 100_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Bottom, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return reflect(toStyle(length, state));
}

static auto resolveKeyword(CSS::Keyword::Left, const BuilderState&) -> LengthPercentage<>
{
    return 0_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Left, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return toStyle(length, state);
}

static auto resolveKeyword(CSS::Keyword::Center, const BuilderState&) -> LengthPercentage<>
{
    return 50_css_percentage;
}

// MARK: Mapped value resolution

template<typename... Args> static auto resolveKeyword(CSS::Keyword::XStart, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyLeftToRight()
        ? resolveKeyword(CSS::Keyword::Left { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Right { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::XEnd, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyLeftToRight()
        ? resolveKeyword(CSS::Keyword::Right { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Left { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::YStart, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyTopToBottom()
        ? resolveKeyword(CSS::Keyword::Top { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Bottom { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::YEnd, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyTopToBottom()
        ? resolveKeyword(CSS::Keyword::Bottom { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Top { }, state, std::forward<Args>(args)...);
}

// MARK: Horizontal/Vertical

static auto resolve(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        },
        [&](const CSS::LengthPercentage<>& value) {
            return toStyle(value, state);
        }
    );
}

static auto resolve(const CSS::TwoComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        },
        [&](const CSS::LengthPercentage<>& value) {
            return toStyle(value, state);
        }
    );
}

static auto resolve(const CSS::ThreeComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        }
    );
}

static auto resolve(const CSS::ThreeComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        }
    );
}

static auto resolve(const CSS::FourComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(get<0>(value.offset),
        [&](auto keyword) {
            return resolveKeyword(keyword, state, get<1>(value.offset));
        }
    );
}

static auto resolve(const CSS::FourComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(get<0>(value.offset),
        [&](auto keyword) {
            return resolveKeyword(keyword, state, get<1>(value.offset));
        }
    );
}

auto ToCSS<TwoComponentPositionHorizontal>::operator()(const TwoComponentPositionHorizontal& value, const RenderStyle& style) -> CSS::TwoComponentPositionHorizontal
{
    return WTF::switchOn(value.offset,
        [&](const auto& value) {
            return CSS::TwoComponentPositionHorizontal { toCSS(LengthPercentage<> { value }, style) };
        }
    );
}

auto ToStyle<CSS::TwoComponentPositionHorizontal>::operator()(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state) -> TwoComponentPositionHorizontal
{
    return { PositionX { resolve(value, state) } };
}

auto ToCSS<TwoComponentPositionVertical>::operator()(const TwoComponentPositionVertical& value, const RenderStyle& style) -> CSS::TwoComponentPositionVertical
{
    return WTF::switchOn(value.offset,
        [&](const auto& value) {
            return CSS::TwoComponentPositionVertical { toCSS(LengthPercentage<> { value }, style) };
        }
    );
}

auto ToStyle<CSS::TwoComponentPositionVertical>::operator()(const CSS::TwoComponentPositionVertical& value, const BuilderState& state) -> TwoComponentPositionVertical
{
    return { PositionY { resolve(value, state) } };
}

// MARK: <position> conversion

auto ToCSS<Position>::operator()(const Position& value, const RenderStyle& style) -> CSS::Position
{
    return CSS::TwoComponentPositionHorizontalVertical {
        WTF::switchOn(value.x,
            [&](const auto& value) {
                return CSS::TwoComponentPositionHorizontal { toCSS(LengthPercentage<> { value }, style) };
            }
        ),
        WTF::switchOn(value.y,
            [&](const auto& value) {
                return CSS::TwoComponentPositionVertical { toCSS(LengthPercentage<> { value }, style) };
            }
        )
    };
}

auto ToStyle<CSS::Position>::operator()(const CSS::Position& position, const BuilderState& state) -> Position
{
    return WTF::switchOn(position,
        [&](const auto& components) {
            return Position {
                PositionX { resolve(get<0>(components), state) },
                PositionY { resolve(get<1>(components), state) },
            };
        }
    );
}

// MARK: <position-x> conversion

auto ToCSS<PositionX>::operator()(const PositionX& value, const RenderStyle& style) -> CSS::PositionX
{
    return WTF::switchOn(value,
        [&](const auto& value) {
            return CSS::TwoComponentPositionHorizontal { toCSS(LengthPercentage<> { value }, style) };
        }
    );
}

auto ToStyle<CSS::PositionX>::operator()(const CSS::PositionX& positionX, const BuilderState& state) -> PositionX
{
    return WTF::switchOn(positionX,
        [&](const auto& value) {
            return PositionX { resolve(value, state) };
        }
    );
}

// MARK: <position-y> conversion

auto ToCSS<PositionY>::operator()(const PositionY& value, const RenderStyle& style) -> CSS::PositionY
{
    return WTF::switchOn(value,
        [&](const auto& value) {
            return CSS::TwoComponentPositionVertical { toCSS(LengthPercentage<> { value }, style) };
        }
    );
}

auto ToStyle<CSS::PositionY>::operator()(const CSS::PositionY& positionY, const BuilderState& state) -> PositionY
{
    return WTF::switchOn(positionY,
        [&](const auto& value) {
            return PositionY { resolve(value, state) };
        }
    );
}

auto CSSValueConversion<Position>::operator()(BuilderState& state, const CSSValue& value) -> Position
{
    RefPtr positionValue = requiredDowncast<CSSPositionValue>(state, value);
    if (!positionValue)
        return { 0_css_px, 0_css_px };
    return toStyle(positionValue->position(), state);
}

auto CSSValueConversion<PositionX>::operator()(BuilderState& state, const CSSValue& value) -> PositionX
{
    RefPtr positionXValue = requiredDowncast<CSSPositionXValue>(state, value);
    if (!positionXValue)
        return 0_css_px;
    return toStyle(positionXValue->position(), state);
}

auto CSSValueConversion<PositionY>::operator()(BuilderState& state, const CSSValue& value) -> PositionY
{
    RefPtr positionYValue = requiredDowncast<CSSPositionYValue>(state, value);
    if (!positionYValue)
        return 0_css_px;
    return toStyle(positionYValue->position(), state);
}

// MARK: - Evaluation

auto Evaluation<Position>::operator()(const Position& position, FloatSize referenceBox) -> FloatPoint
{
    return {
        evaluate(position.x, referenceBox.width()),
        evaluate(position.y, referenceBox.height())
    };
}

// MARK: - Platform

auto ToPlatform<Position>::operator()(const Position& position) -> WebCore::LengthPoint
{
    return { toPlatform(position.x), toPlatform(position.y) };
}

} // namespace Style
} // namespace WebCore
