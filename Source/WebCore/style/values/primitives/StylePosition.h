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

#pragma once

#include "CSSPosition.h"
#include "FloatPoint.h"
#include "StyleLengthWrapper.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

struct Length;
struct LengthPoint;

namespace Style {

struct PositionX : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};
struct PositionY : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

struct TwoComponentPositionHorizontal {
    PositionX offset;
    bool operator==(const TwoComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionHorizontal, offset);

struct TwoComponentPositionVertical {
    PositionY offset;
    bool operator==(const TwoComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionVertical, offset);

struct Position {
    using X = PositionX;
    using Y = PositionY;

    X x;
    Y y;

    Position(X&& x, Y&& y)
        : x { WTFMove(x) }
        , y { WTFMove(y) }
    {
    }

    Position(const X& x, const Y& y)
        : x { x }
        , y { y }
    {
    }

    Position(TwoComponentPositionHorizontal&& x, TwoComponentPositionVertical&& y)
        : x { WTFMove(x.offset) }
        , y { WTFMove(y.offset) }
    {
    }

    Position(const TwoComponentPositionHorizontal& x, const TwoComponentPositionVertical& y)
        : x { x.offset }
        , y { y.offset }
    {
    }

    Position(FloatPoint point)
        : x { PositionX::Fixed { point.x() } }
        , y { PositionY::Fixed { point.y() } }
    {
    }

    Position(CSS::ValueLiteral<CSS::LengthUnit::Px> literalX, CSS::ValueLiteral<CSS::LengthUnit::Px> literalY)
        : x { literalX }
        , y { literalY }
    {
    }

    Position(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literalX, CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literalY)
        : x { literalX }
        , y { literalY }
    {
    }

    Position(const WebCore::LengthPoint&);

    bool operator==(const Position&) const = default;
};
template<size_t I> const auto& get(const Position& position)
{
    if constexpr (!I)
        return position.x;
    else if constexpr (I == 1)
        return position.y;
}

// MARK: - Conversion

template<> struct ToCSS<TwoComponentPositionHorizontal> { auto operator()(const TwoComponentPositionHorizontal&, const RenderStyle&) -> CSS::TwoComponentPositionHorizontal; };
template<> struct ToStyle<CSS::TwoComponentPositionHorizontal> { auto operator()(const CSS::TwoComponentPositionHorizontal&, const BuilderState&) -> TwoComponentPositionHorizontal; };

template<> struct ToCSS<TwoComponentPositionVertical> { auto operator()(const TwoComponentPositionVertical&, const RenderStyle&) -> CSS::TwoComponentPositionVertical; };
template<> struct ToStyle<CSS::TwoComponentPositionVertical> { auto operator()(const CSS::TwoComponentPositionVertical&, const BuilderState&) -> TwoComponentPositionVertical; };

template<> struct ToCSS<Position> { auto operator()(const Position&, const RenderStyle&) -> CSS::Position; };
template<> struct ToStyle<CSS::Position> { auto operator()(const CSS::Position&, const BuilderState&) -> Position; };

template<> struct ToCSS<PositionX> { auto operator()(const PositionX&, const RenderStyle&) -> CSS::PositionX; };
template<> struct ToStyle<CSS::PositionX> { auto operator()(const CSS::PositionX&, const BuilderState&) -> PositionX; };

template<> struct ToCSS<PositionY> { auto operator()(const PositionY&, const RenderStyle&) -> CSS::PositionY; };
template<> struct ToStyle<CSS::PositionY> { auto operator()(const CSS::PositionY&, const BuilderState&) -> PositionY; };

template<> struct CSSValueConversion<Position> { auto operator()(BuilderState&, const CSSValue&) -> Position; };
template<> struct CSSValueConversion<PositionX> { auto operator()(BuilderState&, const CSSValue&) -> PositionX; };
template<> struct CSSValueConversion<PositionY> { auto operator()(BuilderState&, const CSSValue&) -> PositionY; };

// MARK: - Evaluation

template<> struct Evaluation<Position> { auto operator()(const Position&, FloatSize) -> FloatPoint; };

// MARK: - Platform

template<> struct ToPlatform<Position> { auto operator()(const Position&) -> WebCore::LengthPoint; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionX)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PositionY)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::TwoComponentPositionHorizontal)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::TwoComponentPositionVertical)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Position, 2)
