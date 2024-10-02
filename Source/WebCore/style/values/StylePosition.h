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

class CSSCalcSymbolTable;

namespace Style {

struct Position  {
    Position(LengthPercentage&& horizontal, LengthPercentage&& vertical)
        : value { std::make_tuple(WTFMove(horizontal), WTFMove(vertical)) }
    {
    }

    Position(const LengthPercentage& horizontal, const LengthPercentage& vertical)
        : value { std::make_tuple(horizontal, vertical) }
    {
    }

    Position(SpaceSeparatedTuple<LengthPercentage, LengthPercentage>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    bool operator==(const Position&) const = default;

    SpaceSeparatedTuple<LengthPercentage, LengthPercentage> value;
};

// MARK: Support for treating Position types as Tuple-like

template<size_t I> decltype(auto) get(const Position& position)
{
    return get<I>(position.value);
}

// Custom `toCSS()` needed to implement simplification rules.
CSS::Position toCSS(const Position&, const RenderStyle&);

} // namespace Style

namespace CSS {

// Custom `toStyle()` needed to implement simplification rules.
Style::Position toStyle(const Position&, Style::BuilderState&, const CSSCalcSymbolTable&);

}
} // namespace WebCore

namespace std {

template<> class tuple_size<WebCore::Style::Position> : public std::integral_constant<size_t, 2> { };
template<size_t I> class tuple_element<I, WebCore::Style::Position> {
public:
    using type = WebCore::Style::LengthPercentage;
};

}
