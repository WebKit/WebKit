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

#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

class CSSValue;
class RenderStyle;

namespace Style {

// NOTE: the keyword value "infinity" is represented as the standard double value `std::numeric_limits<double>::infinity()`.
using SuperellipseFunction = FunctionNotation<CSSValueSuperellipse, Number<CSS::Nonnegative>>;

// https://drafts.csswg.org/css-borders-4/#typedef-corner-shape-value
struct CornerShapeValue {
    SuperellipseFunction superellipse;

    static constexpr CornerShapeValue round() { return { SuperellipseFunction { 2.0 } }; }
    static constexpr CornerShapeValue scoop() { return { SuperellipseFunction { 0.5 } }; }
    static constexpr CornerShapeValue bevel() { return { SuperellipseFunction { 1.0 } }; }
    static constexpr CornerShapeValue notch() { return { SuperellipseFunction { 0.0 } }; }
    static constexpr CornerShapeValue straight() { return { SuperellipseFunction { std::numeric_limits<double>::infinity() } }; }
    static constexpr CornerShapeValue squircle() { return { SuperellipseFunction { 4.0 } }; }

    bool operator==(const CornerShapeValue&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(CornerShapeValue, superellipse);

// MARK: - Conversion

Ref<CSSValue> toCSSValue(const CornerShapeValue&, const RenderStyle&);

// MARK: - Blending

template<> struct Blending<CornerShapeValue> {
    constexpr auto canBlend(const CornerShapeValue&, const CornerShapeValue&) -> bool { return true; }
    auto blend(const CornerShapeValue&, const CornerShapeValue&, const BlendingContext&) -> CornerShapeValue;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::CornerShapeValue)
