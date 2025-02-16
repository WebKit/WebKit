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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

struct TwoComponentPositionHorizontal {
    std::variant<Keyword::Left, Keyword::Right, Keyword::Center, LengthPercentage<>> offset;

    bool operator==(const TwoComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionHorizontal, offset);

struct TwoComponentPositionVertical {
    std::variant<Keyword::Top, Keyword::Bottom, Keyword::Center, LengthPercentage<>> offset;

    bool operator==(const TwoComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionVertical, offset);

using TwoComponentPosition              = SpaceSeparatedTuple<TwoComponentPositionHorizontal, TwoComponentPositionVertical>;

using FourComponentPositionHorizontal   = SpaceSeparatedTuple<std::variant<Keyword::Left, Keyword::Right>, LengthPercentage<>>;
using FourComponentPositionVertical     = SpaceSeparatedTuple<std::variant<Keyword::Top, Keyword::Bottom>, LengthPercentage<>>;
using FourComponentPosition             = SpaceSeparatedTuple<FourComponentPositionHorizontal, FourComponentPositionVertical>;

struct Position {
    Position(TwoComponentPosition&& twoComponent)
        : value { WTFMove(twoComponent) }
    {
    }

    Position(const TwoComponentPosition& twoComponent)
        : value { twoComponent }
    {
    }

    Position(FourComponentPosition&& fourComponent)
        : value { WTFMove(fourComponent) }
    {
    }

    Position(const FourComponentPosition& fourComponent)
        : value { fourComponent }
    {
    }

    Position(std::variant<TwoComponentPosition, FourComponentPosition>&& variant)
        : value { WTFMove(variant) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const Position&) const = default;

    std::variant<TwoComponentPosition, FourComponentPosition> value;
};
DEFINE_TYPE_WRAPPER_GET(Position, value);

bool isCenterPosition(const Position&);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::Position, 1)
