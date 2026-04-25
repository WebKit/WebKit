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

class CSSValue;
class RenderStyle;

namespace Style {

// NOTE: the keyword value "infinity" is represented as the standard double value `std::numeric_limits<double>::infinity()`.
using SuperellipseFunction = FunctionNotation<CSSValueSuperellipse, Number<CSS::Nonnegative>>;

// https://drafts.csswg.org/css-borders-4/#typedef-corner-shape-value
struct CornerShapeValue {
    SuperellipseFunction superellipse;

    constexpr CornerShapeValue(CSS::Keyword::Round) : superellipse { 2.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Scoop) : superellipse { 0.5 } { }
    constexpr CornerShapeValue(CSS::Keyword::Bevel) : superellipse { 1.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Notch) : superellipse { 0.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Straight) : superellipse { std::numeric_limits<double>::infinity() } { }
    constexpr CornerShapeValue(CSS::Keyword::Squircle) : superellipse { 4.0 } { }
    constexpr CornerShapeValue(SuperellipseFunction value) : superellipse { value } { }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        if (*this == CornerShapeValue(CSS::Keyword::Round { }))
            return functor(CSS::Keyword::Round { });
        if (*this == CornerShapeValue(CSS::Keyword::Scoop { }))
            return functor(CSS::Keyword::Scoop { });
        if (*this == CornerShapeValue(CSS::Keyword::Bevel { }))
            return functor(CSS::Keyword::Bevel { });
        if (*this == CornerShapeValue(CSS::Keyword::Notch { }))
            return functor(CSS::Keyword::Notch { });
        if (*this == CornerShapeValue(CSS::Keyword::Straight { }))
            return functor(CSS::Keyword::Straight { });
        if (*this == CornerShapeValue(CSS::Keyword::Squircle { }))
            return functor(CSS::Keyword::Squircle { });
        return functor(superellipse);
    }

    bool operator==(const CornerShapeValue&) const = default;
};

// https://drafts.csswg.org/css-borders-4/#propdef-corner-shape
using CornerShape = MinimallySerializingSpaceSeparatedRectCorners<CornerShapeValue>;

// MARK: - Conversion

template<> struct CSSValueConversion<CornerShapeValue> { auto operator()(BuilderState&, const CSSValue&) -> CornerShapeValue; };

// MARK: - Blending

template<> struct Blending<CornerShapeValue> {
    auto blend(const CornerShapeValue&, const CornerShapeValue&, const BlendingContext&) -> CornerShapeValue;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::CornerShapeValue)
