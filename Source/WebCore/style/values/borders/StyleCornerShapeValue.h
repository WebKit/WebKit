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

using SuperellipseFunction = FunctionNotation<CSSValueSuperellipse, Number<>>;

// https://drafts.csswg.org/css-borders-4/#typedef-corner-shape-value
struct CornerShapeValue {
    SuperellipseFunction superellipse;

    constexpr CornerShapeValue(CSS::Keyword::Round) : superellipse { 1.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Scoop) : superellipse { -1.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Bevel) : superellipse { 0.0 } { }
    constexpr CornerShapeValue(CSS::Keyword::Notch) : superellipse { -std::numeric_limits<double>::infinity() } { }
    constexpr CornerShapeValue(CSS::Keyword::Square) : superellipse { std::numeric_limits<double>::infinity() } { }
    constexpr CornerShapeValue(CSS::Keyword::Squircle) : superellipse { 2.0 } { }
    constexpr CornerShapeValue(SuperellipseFunction value) : superellipse { value } { }

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

DEFINE_TYPE_WRAPPER_GET(CornerShapeValue, superellipse)

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::CornerShapeValue)
