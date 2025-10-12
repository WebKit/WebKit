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

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'perspective'> = none | <length [0,∞]>
// https://drafts.csswg.org/css-transforms-2/#propdef-perspective
struct Perspective : ValueOrKeyword<Length<CSS::Nonnegative, float>, CSS::Keyword::None> {
    using Base::Base;
    using Length = typename Base::Value;

    float usedPerspective() const { return std::max(1.0f, tryValue().value_or(1.0f).resolveZoom(Style::ZoomNeeded { })); }

    bool isNone() const { return isKeyword(); }
    bool isLength() const { return isValue(); }
};
static_assert(sizeof(Perspective) == sizeof(float));

// MARK: - Conversion

template<> struct CSSValueConversion<Perspective> { auto operator()(BuilderState&, const CSSValue&) -> Perspective; };

// MARK: - Blending

template<> struct Blending<Perspective> {
    auto canBlend(const Perspective&, const Perspective&) -> bool;
    auto blend(const Perspective&, const Perspective&, const BlendingContext&) -> Perspective;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Perspective)
