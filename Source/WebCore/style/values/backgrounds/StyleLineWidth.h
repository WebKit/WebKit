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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {

class LayoutUnit;

using FloatBoxExtent = RectEdges<float>;
using LayoutBoxExtent = RectEdges<LayoutUnit>;

namespace Style {

// <line-width> = <length [0,∞]> | thin | medium | thick
// https://drafts.csswg.org/css-backgrounds/#typedef-line-width
struct LineWidth {
    using Length = Style::Length<CSS::Nonnegative>;

    Length value;

    constexpr LineWidth(Length length) : value { length } { }
    constexpr LineWidth(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }

    constexpr LineWidth(CSS::Keyword::Thin) : value { 1 } { }
    constexpr LineWidth(CSS::Keyword::Medium) : value { 3 } { }
    constexpr LineWidth(CSS::Keyword::Thick) : value { 5 } { }

    constexpr bool isZero() const { return value.isZero(); }
    constexpr bool isPositive() const { return value.isPositive(); }

    constexpr explicit operator bool() const { return !isZero(); }

    constexpr bool operator==(const LineWidth&) const = default;
    constexpr auto operator<=>(const LineWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(LineWidth, value);

using LineWidthBox = MinimallySerializingSpaceSeparatedRectEdges<Style::LineWidth>;

// MARK: - Conversion

template<> struct CSSValueConversion<LineWidth> { auto operator()(BuilderState&, const CSSValue&) -> LineWidth; };

// MARK: - Evaluate

template<typename Result> struct Evaluation<LineWidth, Result> {
    constexpr auto operator()(const LineWidth& value, ZoomNeeded token) -> Result
    {
        return Result(value.value.resolveZoom(token));
    }
};

template<> struct Evaluation<LineWidthBox, FloatBoxExtent> {
    auto operator()(const LineWidthBox&, ZoomNeeded) -> FloatBoxExtent;
};
template<> struct Evaluation<LineWidthBox, LayoutBoxExtent> {
    auto operator()(const LineWidthBox&, ZoomNeeded) -> LayoutBoxExtent;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::LineWidth)
