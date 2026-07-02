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
namespace Style {

// <`-webkit-text-stroke-width`> = <length [0,∞]> | thin | medium | thick
// NOTE: There is no standard associated with this property.
struct WebkitTextStrokeWidth {
    using Length = Style::Length<CSS::NonnegativeUnzoomed>;

    Length value;

    constexpr WebkitTextStrokeWidth(Length length) : value { length } { }
    constexpr WebkitTextStrokeWidth(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }

    constexpr bool isZero() const { return value.isZero(); }
    constexpr bool isPositive() const { return value.isPositive(); }

    constexpr bool operator==(const WebkitTextStrokeWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(WebkitTextStrokeWidth, value);

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitTextStrokeWidth> { auto operator()(BuilderState&, const CSSValue&) -> WebkitTextStrokeWidth; };

// MARK: - Evaluate

template<> struct Evaluation<WebkitTextStrokeWidth, float> {
    constexpr auto operator()(const WebkitTextStrokeWidth& value, ZoomFactor zoom) -> float
    {
        return value.value.resolveZoom(zoom);
    }
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::WebkitTextStrokeWidth)
