/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "RenderStyleConstants.h"
#include "StyleRatio.h"

namespace WebCore {
namespace Style {

// <clip-edge> = <length> | auto
struct ClipEdge {
    WebCore::Length length;

    ClipEdge(CSS::Keyword::Auto)
        : length { LengthType::Auto }
    {
    }

    ClipEdge(Style::Length<>&& value)
        : length { value.value, LengthType::Fixed }
    {
    }

    ClipEdge(WebCore::Length&& value)
        : length { WTFMove(value) }
    {
        ASSERT(length.isFixed() || length.isAuto());
    }

    bool isAuto() const { return length.isAuto(); }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        if (length.isAuto())
            return functor(CSS::Keyword::Auto { });
        ASSERT(length.isFixed());
        return functor(Style::Length<> { length.value() });
    }

    bool operator==(const ClipEdge&) const = default;
};

template<> struct Evaluation<ClipEdge> {
    template<typename T> auto operator()(const ClipEdge& edge, T reference) -> T
    {
        return valueForLength(edge.length, reference);
    }
};

// <rect()> = rect( <clip-edge> , <clip-edge> , <clip-edge> , <clip-edge> )
struct ClipRect {
    FunctionNotation<CSSValueRect, CommaSeparatedRectEdges<ClipEdge>> value;

    ClipRect(CSS::Keyword::Auto keyword)
        : value { CommaSeparatedRectEdges<ClipEdge> { keyword } }
    {
    }

    template<typename T> ClipRect(T top, T right, T bottom, T left)
        : value { CommaSeparatedRectEdges<ClipEdge> { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) } }
    {
    }

    bool operator==(const ClipRect&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ClipRect, value);

// <'clip'> = <rect()> | auto
// https://drafts.fxtf.org/css-masking/#propdef-clip
struct Clip {
    std::optional<ClipRect> value;

    Clip(CSS::Keyword::Auto)
        : value { std::nullopt }
    {
    }

    Clip(const ClipRect& rect)
        : value { rect }
    {
    }

    Clip(ClipRect&& rect)
        : value { WTFMove(rect) }
    {
    }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        if (!value || value->value->value.allOf([](auto& side) { return side.isAuto(); }))
            return functor(CSS::Keyword::Auto { });
        return functor(*value);
    }

    bool operator==(const Clip&) const = default;
};

// `ClipRect` is special-cased to return a `CSSValueRect`.
template<> struct CSSValueCreation<ClipRect> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const ClipRect&); };

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::ClipEdge> = true;
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::Clip> = true;
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ClipRect, 1);
