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

#include "StylePrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

class CSSValue;
class LayoutRect;
class LayoutUnit;
using LayoutBoxExtent = RectEdges<LayoutUnit>;

namespace CSS {
struct ScrollPaddingEdge;
struct ScrollPadding;
}

namespace Style {

class BuilderState;

// <'scroll-padding-*'> = auto | <length-percentage [0,∞]>
// https://drafts.csswg.org/css-scroll-snap-1/#padding-longhands-physical
using ScrollPaddingEdgeValue = PrimitiveNumericOrKeyword<LengthPercentage<CSS::Nonnegative>, CSS::Keyword::Auto>;
DEFINE_TYPE_WRAPPER(ScrollPaddingEdge, ScrollPaddingEdgeValue);

// <'scroll-padding'> = [ auto | <length-percentage [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-padding
DEFINE_TYPE_EXTENDER(ScrollPadding, SpaceSeparatedRectEdges<ScrollPaddingEdge>);

DEFINE_TYPE_MAPPING(CSS::ScrollPaddingEdge, ScrollPaddingEdge)
DEFINE_TYPE_MAPPING(CSS::ScrollPadding, ScrollPadding)

// MARK: - Conversion

template<> struct CSSValueConversions<ScrollPaddingEdge> {
    ScrollPaddingEdge operator()(const CSSValue&, const BuilderState&);
};

// MARK: - Evaluation

template<> struct Evaluation<ScrollPaddingEdge> {
    double operator()(const ScrollPaddingEdge&, double);
    float operator()(const ScrollPaddingEdge&, float);
    LayoutUnit operator()(const ScrollPaddingEdge&, LayoutUnit);
};

LayoutBoxExtent extentForRect(const ScrollPadding&, const LayoutRect&);

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::ScrollPaddingEdge)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_EXTENDER(WebCore::Style::ScrollPadding)
