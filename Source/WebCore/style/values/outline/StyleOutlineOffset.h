/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutUnit.h>
#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore::Style {

// <'outline-offset'> = <length> | inset
struct OutlineOffset : ValueOrKeyword<Length<>, CSS::Keyword::Inset> {
    using Base::Base;

    OutlineOffset(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : Base(Length<> { literal }) { }

    bool isInset() const { return isKeyword(); }
    std::optional<Length<>> tryLength() const { return tryValue(); }
};

// The used value of `outline-offset`: either the specified <length> or, for the `inset` keyword,
// the negated used `outline-width`. Like a line width, it is snapped to an integer number of
// device pixels when evaluated, but unlike a line width it may be negative.
struct UsedOutlineOffset {
    Length<> value;

    constexpr UsedOutlineOffset(Length<> length) : value { length } { }
    constexpr UsedOutlineOffset(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }

    constexpr auto unresolvedValue() const { return value.unresolvedValue(); }

    constexpr bool operator==(const UsedOutlineOffset&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueConversion<OutlineOffset> { OutlineOffset operator()(BuilderState&, const CSSValue&); };

// MARK: - Blending

template<> struct Blending<OutlineOffset> {
    auto canBlend(const OutlineOffset&, const OutlineOffset&) -> bool;
    auto blend(const OutlineOffset&, const OutlineOffset&, const Style::ComputedStyle&, const Style::ComputedStyle&, const Interpolation::Context&) -> OutlineOffset;
};

// MARK: - Evaluation

template<> struct Evaluation<UsedOutlineOffset, float> {
    WEBCORE_EXPORT auto operator()(const UsedOutlineOffset&, ZoomFactor, float deviceScaleFactor) -> float;
};
template<> struct Evaluation<UsedOutlineOffset, LayoutUnit> {
    auto operator()(const UsedOutlineOffset&, ZoomFactor, float deviceScaleFactor) -> LayoutUnit;
};

} // namespace WebCore::Style

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OutlineOffset)
