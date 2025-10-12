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

#include <WebCore/AcceleratedEffectOpacity.h>
#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <opacity-value> = <number [0,1]@(allow-both-at-parse)> | <percentage [0,100]@(allow-both-at-parse, converted-to-number)>
// https://drafts.csswg.org/css-color-4/#typedef-opacity-opacity-value
struct Opacity {
    using Number = Style::Number<CSS::ClosedUnitRange, float>;

    Number value;

    constexpr Opacity(Number number)
        : value { number }
    {
    }

    constexpr Opacity(float literal)
        : value { literal }
    {
    }

    constexpr Opacity(CSS::ValueLiteral<CSS::NumberUnit::Number> literal)
        : value { literal }
    {
    }

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    constexpr explicit Opacity(AcceleratedEffectOpacity opacity)
        : value { opacity.value }
    {
    }
#endif

    constexpr bool isZero() const { return value == 0; }

    constexpr bool isTransparent() const { return value == 0; }
    constexpr bool isOpaque() const { return value == 1; }

    constexpr bool operator==(const Opacity&) const = default;
    constexpr auto operator<=>(const Opacity&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(Opacity, value);

// MARK: - Conversion

template<> struct CSSValueConversion<Opacity> { auto operator()(BuilderState&, const CSSValue&) -> Opacity; };

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

template<> struct Evaluation<Opacity, AcceleratedEffectOpacity> {
    auto operator()(const Opacity& value) -> AcceleratedEffectOpacity
    {
        return { .value = value.value.value };
    }
};

#endif

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::Opacity)
