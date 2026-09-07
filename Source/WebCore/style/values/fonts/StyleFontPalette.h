/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/FontPalette.h>
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace CSS {
struct FontPalette;
}

namespace Style {

// <'font-palette'> = normal | light | dark | <palette-identifier> | <palette-mix()>
// https://drafts.csswg.org/css-fonts/#propdef-font-palette
struct FontPalette {
    FontPalette(CSS::Keyword::Normal)
        : m_platform { WebCore::FontPalette::Keyword::Normal }
    {
    }

    FontPalette(CSS::Keyword::Light)
        : m_platform { WebCore::FontPalette::Keyword::Light }
    {
    }

    FontPalette(CSS::Keyword::Dark)
        : m_platform { WebCore::FontPalette::Keyword::Dark }
    {
    }

    FontPalette(CustomIdent&& identifier)
        : m_platform { WTF::move(identifier.value) }
    {
    }

    FontPalette(WebCore::FontPalette&& platform)
        : m_platform { WTF::move(platform) }
    {
    }

    FontPalette(const WebCore::FontPalette& platform)
        : m_platform { platform }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    const WebCore::FontPalette& platform() const LIFETIME_BOUND { return m_platform; }
    WebCore::FontPalette takePlatform() { return WTF::move(m_platform); }

    bool operator==(const FontPalette&) const = default;

private:
    WebCore::FontPalette m_platform;
};

// MARK: - Conversion

template<> struct ToCSS<FontPalette> { auto operator()(const FontPalette&, const ComputedStyle&) -> CSS::FontPalette; };
template<> struct ToStyle<CSS::FontPalette> { auto operator()(const CSS::FontPalette&, const BuilderState&) -> FontPalette; };

template<> struct CSSValueConversion<FontPalette> { auto operator()(BuilderState&, const CSSValue&) -> FontPalette; };
template<> struct CSSValueCreation<FontPalette> { Ref<CSSValue> operator()(CSSValuePool&, const ComputedStyle&, const FontPalette&); };

// MARK: - Blending

template<> struct Blending<FontPalette> {
    constexpr bool canBlend(const FontPalette&, const FontPalette&) { return true; }
    auto blend(const FontPalette&, const FontPalette&, const BlendingContext&) -> FontPalette;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontPalette)
