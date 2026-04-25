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

// <'font-width'> = normal | <percentage [0,∞]> | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded
// NOTE: Computed value is always resolved to a <percentage [0,∞]>.
// https://drafts.csswg.org/css-fonts-4/#propdef-font-width
struct FontWidth {
    using Percentage = Style::Percentage<CSS::Nonnegative>;

    FontWidth(CSS::Keyword::Normal) : m_platform { normalWidthValue() } { }
    FontWidth(CSS::Keyword::UltraCondensed) : m_platform { ultraCondensedWidthValue() } { }
    FontWidth(CSS::Keyword::ExtraCondensed) : m_platform { extraCondensedWidthValue() } { }
    FontWidth(CSS::Keyword::Condensed) : m_platform { condensedWidthValue() } { }
    FontWidth(CSS::Keyword::SemiCondensed) : m_platform { semiCondensedWidthValue() } { }
    FontWidth(CSS::Keyword::SemiExpanded) : m_platform { semiExpandedWidthValue() } { }
    FontWidth(CSS::Keyword::Expanded) : m_platform { expandedWidthValue() } { }
    FontWidth(CSS::Keyword::ExtraExpanded) : m_platform { extraExpandedWidthValue() } { }
    FontWidth(CSS::Keyword::UltraExpanded) : m_platform { ultraExpandedWidthValue() } { }
    FontWidth(Percentage percentage) : m_platform { FontSelectionValue::clampFloat(percentage.value) } { }

    FontWidth(FontSelectionValue platform) : m_platform { platform } { }

    Percentage percentage() const { return Percentage { static_cast<float>(m_platform) }; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        return visitor(percentage());
    }

    FontSelectionValue platform() const { return m_platform; }

    bool operator==(const FontWidth&) const = default;

private:
    FontSelectionValue m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontWidth> { auto operator()(BuilderState&, const CSSValue&) -> FontWidth; };

// MARK: - Blending

template<> struct Blending<FontWidth> {
    auto blend(const FontWidth&, const FontWidth&, const BlendingContext&) -> FontWidth;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontWidth)
