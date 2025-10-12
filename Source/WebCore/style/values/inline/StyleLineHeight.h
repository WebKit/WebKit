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

#include <WebCore/StyleLengthWrapper.h>
#include <wtf/Hasher.h>

namespace WebCore {
namespace Style {

// <'line-height'> = normal | <number [0,∞]> | <length-percentage [0,∞]>
// NOTE: <number [0,∞]> gets converted to <length-percentage [0,∞]>.
// https://drafts.csswg.org/css-inline/#propdef-line-height
struct LineHeight : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>, CSS::Keyword::Normal> {
    using Base::Base;

    bool isNormal() const { return holdsAlternative<CSS::Keyword::Normal>(); }

    unsigned valueForHash() const
    {
        return switchOn(
            [&](const CSS::Keyword::Normal&) -> unsigned { return computeHash(0); },
            [&](const Fixed& fixed) -> unsigned { return computeHash(1, fixed.unresolvedValue()); },
            [&](const Percentage& percentage) -> unsigned { return computeHash(2, percentage.value); },
            [&](const Calc&) -> unsigned { return computeHash(3); }
        );
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<LineHeight> {
    auto operator()(BuilderState&, const CSSValue&, float multiplier = 1.0f) -> LineHeight;
    auto operator()(BuilderState&, const CSSPrimitiveValue&, float multiplier = 1.0f) -> LineHeight;
};

// MARK: - Blending

template<> struct Blending<LineHeight> {
    auto canBlend(const LineHeight&, const LineHeight&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const LineHeight&, const LineHeight&) -> bool;
    auto blend(const LineHeight&, const LineHeight&, const BlendingContext&) -> LineHeight;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::LineHeight)
