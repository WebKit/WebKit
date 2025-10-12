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

// <'glyph-orientation-horizontal'> = <angle>
// NOTE: The value of the angle is restricted to 0, 90, 180, and 270 degrees.
// https://www.w3.org/TR/SVG11/text.html#GlyphOrientationHorizontalProperty
enum class SVGGlyphOrientationHorizontal : uint8_t {
    Degrees0,
    Degrees90,
    Degrees180,
    Degrees270
};

// MARK: - Value Representation

template<> struct ValueRepresentation<SVGGlyphOrientationHorizontal> {
    template<typename... F> decltype(auto) operator()(SVGGlyphOrientationHorizontal value, F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        switch (value) {
        case SVGGlyphOrientationHorizontal::Degrees0:
            return visitor(Angle<> { 0 });
        case SVGGlyphOrientationHorizontal::Degrees90:
            return visitor(Angle<> { 90 });
        case SVGGlyphOrientationHorizontal::Degrees180:
            return visitor(Angle<> { 180 });
        case SVGGlyphOrientationHorizontal::Degrees270:
            return visitor(Angle<> { 270 });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<SVGGlyphOrientationHorizontal> { auto operator()(BuilderState&, const CSSValue&) -> SVGGlyphOrientationHorizontal; };

} // namespace Style
} // namespace WebCore
