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

#include "StyleLengthWrapper.h"

namespace WebCore {
namespace Style {

struct StrokeWidthLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>> {
    using Base::Base;
};

// <'stroke-width'> = <length-percentage [0,∞]> | <number [0,∞]>@(converted-to-px)
// FIXME: The current spec has the grammar as <'stroke-width'> = <length-percentage [0,∞]>#.
// https://drafts.fxtf.org/fill-stroke-3/#propdef-stroke-width
struct StrokeWidth {
    using Fixed = StrokeWidthLength::Fixed;
    using Percentage = StrokeWidthLength::Percentage;
    using Calc = StrokeWidthLength::Calc;

    StrokeWidthLength value;

    StrokeWidth(StrokeWidthLength&& length) : value { WTFMove(length) } { }
    StrokeWidth(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }
    StrokeWidth(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : value { literal } { }

    bool isZero() const { return value.isZero(); }
    bool isPositive() const { return value.isPositive(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const StrokeWidth&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueConversion<StrokeWidth> { auto operator()(BuilderState&, const CSSValue&) -> StrokeWidth; };

// MARK: - Conversion

template<> struct Blending<StrokeWidth> {
    auto canBlend(const StrokeWidth&, const StrokeWidth&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const StrokeWidth&, const StrokeWidth&) -> bool;
    auto blend(const StrokeWidth&, const StrokeWidth&, const BlendingContext&) -> StrokeWidth;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::StrokeWidthLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::StrokeWidth)
