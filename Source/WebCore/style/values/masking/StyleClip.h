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

#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/Markable.h>

namespace WebCore {
namespace Style {

// <clip-edge> = <length> | auto
struct ClipEdge : ValueOrKeyword<Length<>, CSS::Keyword::Auto> {
    using Base::Base;
    using Length = typename Base::Value;

    bool isAuto() const { return isKeyword(); }
    bool isLength() const { return isValue(); }
    std::optional<Length> tryLength() const { return tryValue(); }
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

    bool isAllAuto() const { return value->allOf([](auto& side) { return side.isAuto(); }); }
    bool isAnyAuto() const { return value->anyOf([](auto& side) { return side.isAuto(); }); }

    bool operator==(const ClipRect&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ClipRect, value);

// <'clip'> = <rect()> | auto
// https://drafts.fxtf.org/css-masking/#propdef-clip
struct Clip {
    Clip(CSS::Keyword::Auto)
        : value { }
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

    bool isAuto() const { return !value; }
    bool isRect() const { return !isAuto(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto() || value->isAllAuto())
            return visitor(CSS::Keyword::Auto { });
        return visitor(*value);
    }

    bool operator==(const Clip&) const = default;

private:
    friend struct Blending<Clip>;

    std::optional<ClipRect> value;
};

// MARK: Conversion

template<> struct CSSValueConversion<Clip> { auto operator()(BuilderState&, const CSSValue&) -> Clip; };

// `ClipRect` is special-cased to return a `CSSValueRect`.
template<> struct CSSValueCreation<ClipRect> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const ClipRect&); };

// MARK: - Blending

template<> struct Blending<Clip> {
    auto canBlend(const Clip&, const Clip&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const Clip&, const Clip&) -> bool;
    auto blend(const Clip&, const Clip&, const BlendingContext&) -> Clip;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::ClipRect);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ClipEdge);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Clip);
