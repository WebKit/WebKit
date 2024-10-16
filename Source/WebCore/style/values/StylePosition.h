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

#pragma once

#include "CSSPosition.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

using Left   = CSS::Left;
using Right  = CSS::Right;
using Top    = CSS::Top;
using Bottom = CSS::Bottom;
using Center = CSS::Center;

struct Position  {
    Position(LengthPercentage<>&& horizontal, LengthPercentage<>&& vertical)
        : value { WTFMove(horizontal), WTFMove(vertical) }
    {
    }

    Position(SpaceSeparatedArray<LengthPercentage<>, 2>&& array)
        : value { WTFMove(array) }
    {
    }

    bool operator==(const Position&) const = default;

    SpaceSeparatedArray<LengthPercentage<>, 2> value;
};

template<size_t I> const auto& get(const Position& position)
{
    return get<I>(position.value);
}

template<> struct ToCSS<Position> { auto operator()(const Position&, const RenderStyle&) -> CSS::Position; };
template<> struct ToStyle<CSS::Position> { auto operator()(const CSS::Position&, const BuilderState&, const CSSCalcSymbolTable&) -> Position; };

} // namespace Style
} // namespace WebCore

STYLE_TUPLE_LIKE_CONFORMANCE(Position, 2)
