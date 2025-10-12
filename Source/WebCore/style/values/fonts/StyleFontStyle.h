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

#include <WebCore/FontSelectionAlgorithm.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'font-style'> = normal | italic | oblique <angle [-90deg,90deg]>?
// https://drafts.csswg.org/css-fonts-4/#propdef-font-style
struct FontStyle {
    using Angle = Style::Angle<CSS::Range{-90, 90}>;

    FontStyle(CSS::Keyword::Normal) : m_platformSlope { std::nullopt }, m_platformAxis { FontStyleAxis::slnt } { }
    FontStyle(CSS::Keyword::Italic) : m_platformSlope { italicValue() }, m_platformAxis { FontStyleAxis::ital } { }
    FontStyle(CSS::Keyword::Oblique) : m_platformSlope { italicValue() }, m_platformAxis { FontStyleAxis::slnt } { }
    FontStyle(Angle angle) : m_platformSlope { FontSelectionValue::clampFloat(angle.value) }, m_platformAxis { FontStyleAxis::slnt } { }

    FontStyle(std::optional<FontSelectionValue> slope, FontStyleAxis axis) : m_platformSlope { slope }, m_platformAxis { axis } { }

    bool isNormal() const { return m_platformSlope == std::nullopt && m_platformAxis == FontStyleAxis::slnt; }
    bool isItalic() const { return m_platformSlope == italicValue() && m_platformAxis == FontStyleAxis::ital; }
    bool isOblique() const { return m_platformSlope != std::nullopt && m_platformAxis == FontStyleAxis::slnt; }

    std::optional<Angle> angle() const { return m_platformSlope ? std::make_optional(Angle { static_cast<float>(*m_platformSlope) }) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (!m_platformSlope || !*m_platformSlope)
            return visitor(CSS::Keyword::Normal { });
        if (*m_platformSlope == italicValue()) {
            if (m_platformAxis == FontStyleAxis::ital)
                return visitor(CSS::Keyword::Italic { });
            return visitor(CSS::Keyword::Oblique { });
        }
        return visitor(SpaceSeparatedTuple { CSS::Keyword::Oblique { }, Angle { static_cast<float>(*m_platformSlope) } });
    }

    std::optional<FontSelectionValue> platformSlope() const { return m_platformSlope; }
    FontStyleAxis platformAxis() const { return m_platformAxis; }

    // NOTE: This is not if the value would compute to the keyword `italic`, but rather a more general whether the slope is large enough to be considered "italic" (see `WebCore::italicThreshold()`).
    bool isConsideredItalic() const { return WebCore::isItalic(m_platformSlope); }

    bool operator==(const FontStyle&) const = default;

private:
    std::optional<FontSelectionValue> m_platformSlope;
    FontStyleAxis m_platformAxis;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontStyle> { auto operator()(BuilderState&, const CSSValue&) -> FontStyle; };
// `FontStyle` is special-cased to return a `CSSFontStyleWithAngleValue`.
template<> struct CSSValueCreation<FontStyle> { auto operator()(CSSValuePool&, const RenderStyle&, const FontStyle&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<FontStyle> {
    auto canBlend(const FontStyle&, const FontStyle&) -> bool;
    auto blend(const FontStyle&, const FontStyle&, const BlendingContext&) -> FontStyle;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontStyle)
