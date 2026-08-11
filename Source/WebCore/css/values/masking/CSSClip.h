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

#include "CSSPrimitiveNumeric.h"

namespace WebCore {
namespace CSS {

// <clip-edge> = <length> | auto
struct ClipEdge : ValueOrKeyword<Length<>, Keyword::Auto> {
    using Base::Base;
    using Length = typename Base::Value;

    bool isAuto() const { return isKeyword(); }
    bool isLength() const { return isValue(); }
    std::optional<Length> tryLength() const { return tryValue(); }
};

// <rect()> = rect( <clip-edge> , <clip-edge> , <clip-edge> , <clip-edge> )
struct ClipRect {
    FunctionNotation<CSSValueRect, CommaSeparatedRectEdges<ClipEdge>> value;

    ClipRect(FunctionNotation<CSSValueRect, CommaSeparatedRectEdges<ClipEdge>> value)
        : value { WTF::move(value) }
    {
    }

    ClipRect(Keyword::Auto keyword)
        : value { CommaSeparatedRectEdges<ClipEdge> { keyword } }
    {
    }

    template<typename T> ClipRect(T top, T right, T bottom, T left)
        : value { CommaSeparatedRectEdges<ClipEdge> { WTF::move(top), WTF::move(right), WTF::move(bottom), WTF::move(left) } }
    {
    }

    bool isAllAuto() const { return value->allOf([](auto& side) { return side.isAuto(); }); }
    bool isAnyAuto() const { return value->anyOf([](auto& side) { return side.isAuto(); }); }

    bool operator==(const ClipRect&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ClipRect, value);

// <'clip'> = <rect()> | auto
// https://drafts.csswg.org/css-masking/#propdef-clip
struct Clip {
    Clip(Keyword::Auto)
        : value { }
    {
    }

    Clip(const ClipRect& rect)
        : value { rect }
    {
    }

    Clip(ClipRect&& rect)
        : value { WTF::move(rect) }
    {
    }

    bool isAuto() const { return !value; }
    bool isRect() const { return !isAuto(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto())
            return visitor(Keyword::Auto { });
        return visitor(*value);
    }

    bool operator==(const Clip&) const = default;

private:
    std::optional<ClipRect> value;
};

// MARK: - DeprecatedCSSOMValue Creation

// Specialized to return a `DeprecatedCSSOMPrimitiveValue`.
template<> struct DeprecatedCSSOMValueCreation<ClipRect> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const ClipRect&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::ClipRect);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::ClipEdge);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::Clip);
