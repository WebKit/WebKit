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

#include <WebCore/CSSPrimitiveNumericTypes.h>

namespace WebCore {
namespace CSS {

// MARK: Two Component Types

struct TwoComponentPositionHorizontal {
    Variant<Keyword::Left, Keyword::Right, Keyword::Center, Keyword::XStart, Keyword::XEnd, LengthPercentage<>> offset;

    bool operator==(const TwoComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionHorizontal, offset);

struct TwoComponentPositionVertical {
    Variant<Keyword::Top, Keyword::Bottom, Keyword::Center, Keyword::YStart, Keyword::YEnd, LengthPercentage<>> offset;

    bool operator==(const TwoComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionVertical, offset);

// MARK: Three Component Types

struct ThreeComponentPositionHorizontal {
    Variant<Keyword::Left, Keyword::Right, Keyword::Center, Keyword::XStart, Keyword::XEnd> offset;

    bool operator==(const ThreeComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ThreeComponentPositionHorizontal, offset);

struct ThreeComponentPositionVertical {
    Variant<Keyword::Top, Keyword::Bottom, Keyword::Center, Keyword::YStart, Keyword::YEnd> offset;

    bool operator==(const ThreeComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ThreeComponentPositionVertical, offset);

// MARK: Four Component Types
struct FourComponentPositionHorizontal {
    SpaceSeparatedTuple<Variant<Keyword::Left, Keyword::Right, Keyword::XStart, Keyword::XEnd>, LengthPercentage<>> offset;

    bool operator==(const FourComponentPositionHorizontal&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(FourComponentPositionHorizontal, offset);

struct FourComponentPositionVertical {
    SpaceSeparatedTuple<Variant<Keyword::Top, Keyword::Bottom, Keyword::YStart, Keyword::YEnd>, LengthPercentage<>> offset;

    bool operator==(const FourComponentPositionVertical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(FourComponentPositionVertical, offset);

using TwoComponentPositionHorizontalVertical               = SpaceSeparatedTuple<TwoComponentPositionHorizontal, TwoComponentPositionVertical>;

using ThreeComponentPositionHorizontalVerticalLengthFirst  = SpaceSeparatedTuple<FourComponentPositionHorizontal, ThreeComponentPositionVertical>;
using ThreeComponentPositionHorizontalVerticalLengthSecond = SpaceSeparatedTuple<ThreeComponentPositionHorizontal, FourComponentPositionVertical>;

using FourComponentPositionHorizontalVertical              = SpaceSeparatedTuple<FourComponentPositionHorizontal, FourComponentPositionVertical>;

struct Position {
    using Kind = Variant<
        TwoComponentPositionHorizontalVertical,
        ThreeComponentPositionHorizontalVerticalLengthFirst,
        ThreeComponentPositionHorizontalVerticalLengthSecond,
        FourComponentPositionHorizontalVertical
    >;

    template<typename T>
    Position(T&& value)
        : value { std::forward<T>(value) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const Position&) const = default;

    Kind value;
};
DEFINE_TYPE_WRAPPER_GET(Position, value);

struct PositionX {
    using Kind = Variant<
        TwoComponentPositionHorizontal,
        FourComponentPositionHorizontal
    >;

    template<typename T>
    PositionX(T&& value)
        : value { std::forward<T>(value) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const PositionX&) const = default;

    Kind value;
};
DEFINE_TYPE_WRAPPER_GET(PositionX, value);

struct PositionY {
    using Kind = Variant<
        TwoComponentPositionVertical,
        FourComponentPositionVertical
    >;

    template<typename T>
    PositionY(T&& value)
        : value { std::forward<T>(value) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const PositionY&) const = default;

    Kind value;
};
DEFINE_TYPE_WRAPPER_GET(PositionY, value);

bool isCenterPosition(const Position&);

std::pair<CSS::PositionX, CSS::PositionY> split(CSS::Position&&);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ThreeComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ThreeComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::Position, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionX, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionY, 1)
