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

#include <WebCore/BoxExtents.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class LayoutRect;
class LayoutUnit;

namespace Style {

// <'scroll-margin-*'> = <length>
// https://drafts.csswg.org/css-scroll-snap-1/#margin-longhands-physical
struct ScrollMarginEdge {
    using Fixed = Length<>;

    ScrollMarginEdge(Fixed&& fixed) : m_value(fixed) { }
    ScrollMarginEdge(const Fixed& fixed) : m_value(fixed) { }

    ScrollMarginEdge(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(literal) { }

    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        return visitor(m_value);
    }

    bool operator==(const ScrollMarginEdge&) const = default;

private:
    friend struct Evaluation<ScrollMarginEdge, LayoutUnit>;
    friend struct Evaluation<ScrollMarginEdge, float>;

    Length<> m_value;
};

// <'scroll-margin'> = <length>{1,4}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin
using ScrollMarginBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollMarginEdge>;

// MARK: - Conversion

template<> struct CSSValueConversion<ScrollMarginEdge> { auto operator()(BuilderState&, const CSSValue&) -> ScrollMarginEdge; };

// MARK: - Evaluation

template<> struct Evaluation<ScrollMarginEdge, LayoutUnit> {
    auto operator()(const ScrollMarginEdge&, LayoutUnit referenceLength, ZoomNeeded) -> LayoutUnit;
    auto operator()(const ScrollMarginEdge&, ZoomNeeded) -> LayoutUnit;
};
template<> struct Evaluation<ScrollMarginEdge, float> {
    auto operator()(const ScrollMarginEdge&, float referenceLength, ZoomNeeded) -> float;
    auto operator()(const ScrollMarginEdge&, ZoomNeeded) -> float;
};

// MARK: - Extent

LayoutBoxExtent extentForRect(const ScrollMarginBox&, const LayoutRect&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ScrollMarginEdge)
