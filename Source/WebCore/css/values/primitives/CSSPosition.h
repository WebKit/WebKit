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

using Left   = Constant<CSSValueLeft>;
using Right  = Constant<CSSValueRight>;
using Top    = Constant<CSSValueTop>;
using Bottom = Constant<CSSValueBottom>;
using Center = Constant<CSSValueCenter>;

struct TwoComponentPositionHorizontal {
    std::variant<Left, Right, Center, LengthPercentage<>> offset;
    bool operator==(const TwoComponentPositionHorizontal&) const = default;
};
struct TwoComponentPositionVertical {
    std::variant<Top, Bottom, Center, LengthPercentage<>> offset;
    bool operator==(const TwoComponentPositionVertical&) const = default;
};
using TwoComponentPosition              = SpaceSeparatedTuple<TwoComponentPositionHorizontal, TwoComponentPositionVertical>;

using FourComponentPositionHorizontal   = SpaceSeparatedTuple<std::variant<Left, Right>, LengthPercentage<>>;
using FourComponentPositionVertical     = SpaceSeparatedTuple<std::variant<Top, Bottom>, LengthPercentage<>>;
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

    bool operator==(const Position&) const = default;

    std::variant<TwoComponentPosition, FourComponentPosition> value;
};

bool isCenterPosition(const Position&);

template<> struct Serialize<TwoComponentPositionHorizontal> { void operator()(StringBuilder&, const TwoComponentPositionHorizontal&); };
template<> struct ComputedStyleDependenciesCollector<TwoComponentPositionHorizontal> { void operator()(ComputedStyleDependencies&, const TwoComponentPositionHorizontal&); };
template<> struct CSSValueChildrenVisitor<TwoComponentPositionHorizontal> { IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const TwoComponentPositionHorizontal&); };

template<> struct Serialize<TwoComponentPositionVertical> { void operator()(StringBuilder&, const TwoComponentPositionVertical&); };
template<> struct ComputedStyleDependenciesCollector<TwoComponentPositionVertical> { void operator()(ComputedStyleDependencies&, const TwoComponentPositionVertical&); };
template<> struct CSSValueChildrenVisitor<TwoComponentPositionVertical> { IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const TwoComponentPositionVertical&); };

template<> struct Serialize<Position> { void operator()(StringBuilder&, const Position&); };
template<> struct ComputedStyleDependenciesCollector<Position> { void operator()(ComputedStyleDependencies&, const Position&); };
template<> struct CSSValueChildrenVisitor<Position> { IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const Position&); };

} // namespace CSS
} // namespace WebCore

namespace WTF {

// Overload WTF::switchOn to make it so CSS::Position can be used directly.

template<class... F> ALWAYS_INLINE auto switchOn(const WebCore::CSS::Position& position, F&&... f) -> decltype(switchOn(position.value, std::forward<F>(f)...))
{
    return switchOn(position.value, std::forward<F>(f)...);
}

template<class... F> ALWAYS_INLINE auto switchOn(WebCore::CSS::Position&& position, F&&... f) -> decltype(switchOn(WTFMove(position.value), std::forward<F>(f)...))
{
    return switchOn(WTFMove(position.value), std::forward<F>(f)...);
}

} // namespace WTF
