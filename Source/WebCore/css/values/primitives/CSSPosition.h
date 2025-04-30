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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {

class WritingMode;

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

struct TwoComponentPositionBlock {
    Variant<Keyword::BlockStart, Keyword::Center, Keyword::BlockEnd> offset;

    bool operator==(const TwoComponentPositionBlock&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionBlock, offset);

struct TwoComponentPositionInline {
    Variant<Keyword::InlineStart, Keyword::Center, Keyword::InlineEnd> offset;

    bool operator==(const TwoComponentPositionInline&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionInline, offset);

struct TwoComponentPositionLogical {
    Variant<Keyword::Start, Keyword::End, Keyword::Center, LengthPercentage<>> offset;

    bool operator==(const TwoComponentPositionLogical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(TwoComponentPositionLogical, offset);

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

struct FourComponentPositionBlock {
    SpaceSeparatedTuple<Variant<Keyword::BlockStart, Keyword::BlockEnd>, LengthPercentage<>> offset;

    bool operator==(const FourComponentPositionBlock&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(FourComponentPositionBlock, offset);

struct FourComponentPositionInline {
    SpaceSeparatedTuple<Variant<Keyword::InlineStart, Keyword::InlineEnd>, LengthPercentage<>> offset;

    bool operator==(const FourComponentPositionInline&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(FourComponentPositionInline, offset);

struct FourComponentPositionLogical {
    SpaceSeparatedTuple<Variant<Keyword::Start, Keyword::End>, LengthPercentage<>> offset;

    bool operator==(const FourComponentPositionLogical&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(FourComponentPositionLogical, offset);

using TwoComponentPositionHorizontalVertical               = SpaceSeparatedTuple<TwoComponentPositionHorizontal, TwoComponentPositionVertical>;
using TwoComponentPositionBlockInline                      = SpaceSeparatedTuple<TwoComponentPositionBlock, TwoComponentPositionInline>;
using TwoComponentPositionStartEnd                         = SpaceSeparatedTuple<TwoComponentPositionLogical, TwoComponentPositionLogical>;

using ThreeComponentPositionHorizontalVerticalLengthFirst  = SpaceSeparatedTuple<FourComponentPositionHorizontal, ThreeComponentPositionVertical>;
using ThreeComponentPositionHorizontalVerticalLengthSecond = SpaceSeparatedTuple<ThreeComponentPositionHorizontal, FourComponentPositionVertical>;
using ThreeComponentPositionBlockInlineLengthFirst         = SpaceSeparatedTuple<FourComponentPositionBlock, TwoComponentPositionInline>;
using ThreeComponentPositionBlockInlineLengthSecond        = SpaceSeparatedTuple<TwoComponentPositionBlock, FourComponentPositionInline>;
using ThreeComponentPositionStartEndLengthFirst            = SpaceSeparatedTuple<FourComponentPositionLogical, TwoComponentPositionLogical>;
using ThreeComponentPositionStartEndLengthSecond           = SpaceSeparatedTuple<TwoComponentPositionLogical, FourComponentPositionLogical>;

using FourComponentPositionHorizontalVertical              = SpaceSeparatedTuple<FourComponentPositionHorizontal, FourComponentPositionVertical>;
using FourComponentPositionBlockInline                     = SpaceSeparatedTuple<FourComponentPositionBlock, FourComponentPositionInline>;
using FourComponentPositionStartEnd                        = SpaceSeparatedTuple<FourComponentPositionLogical, FourComponentPositionLogical>;

struct Position {
    using Kind = Variant<
        TwoComponentPositionHorizontalVertical,
        TwoComponentPositionBlockInline,
        TwoComponentPositionStartEnd,

        ThreeComponentPositionHorizontalVerticalLengthFirst,
        ThreeComponentPositionHorizontalVerticalLengthSecond,
        ThreeComponentPositionBlockInlineLengthFirst,
        ThreeComponentPositionBlockInlineLengthSecond,
        ThreeComponentPositionStartEndLengthFirst,
        ThreeComponentPositionStartEndLengthSecond,

        FourComponentPositionHorizontalVertical,
        FourComponentPositionBlockInline,
        FourComponentPositionStartEnd
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

struct PositionXY {
    using Kind = Variant<
        TwoComponentPositionHorizontalVertical,
        ThreeComponentPositionHorizontalVerticalLengthFirst,
        ThreeComponentPositionHorizontalVerticalLengthSecond,
        FourComponentPositionHorizontalVertical
    >;

    template<typename T>
    PositionXY(T&& value)
        : value { std::forward<T>(value) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const PositionXY&) const = default;

    Kind value;
};
DEFINE_TYPE_WRAPPER_GET(PositionXY, value);

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

struct PositionLogical {
    using Kind = Variant<
        TwoComponentPositionLogical,
        FourComponentPositionLogical
    >;

    template<typename T>
    PositionLogical(T&& value)
        : value { std::forward<T>(value) }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const PositionLogical&) const = default;

    Kind value;
};
DEFINE_TYPE_WRAPPER_GET(PositionLogical, value);

bool isCenterPosition(const Position&);

// Resolves a logical position to a physical position (used by style builder).
PositionX resolveToPhysicalX(const PositionLogical&, WritingMode);
PositionY resolveToPhysicalY(const PositionLogical&, WritingMode);

// Splits a position-xy into x and y components (used by shorthand parsers).
std::pair<CSS::PositionX, CSS::PositionY> split(CSS::PositionXY&&);

// MARK: Concept predicates.

template<typename T> concept IsBlockInlineComponents =
       std::same_as<T, CSS::TwoComponentPositionBlockInline>
    || std::same_as<T, CSS::ThreeComponentPositionBlockInlineLengthFirst>
    || std::same_as<T, CSS::ThreeComponentPositionBlockInlineLengthSecond>
    || std::same_as<T, CSS::FourComponentPositionBlockInline>;

template<typename T> concept IsStartEndComponents =
       std::same_as<T, CSS::TwoComponentPositionStartEnd>
    || std::same_as<T, CSS::ThreeComponentPositionStartEndLengthFirst>
    || std::same_as<T, CSS::ThreeComponentPositionStartEndLengthSecond>
    || std::same_as<T, CSS::FourComponentPositionStartEnd>;

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionBlock, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionInline, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::TwoComponentPositionLogical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ThreeComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ThreeComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionHorizontal, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionVertical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionBlock, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionInline, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FourComponentPositionLogical, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::Position, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionXY, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionX, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionY, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PositionLogical, 1)
