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

#include <WebCore/FontPalette.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'font-palette'> = normal | light | dark | <palette-identifier> | <palette-mix()>
// FIXME: <palette-mix()> is not yet supported.
// https://drafts.csswg.org/css-fonts/#propdef-font-palette
struct FontPalette {
    FontPalette(CSS::Keyword::Normal)
        : m_platform { .type = WebCore::FontPalette::Type::Normal, .identifier = nullAtom() }
    {
    }

    FontPalette(CSS::Keyword::Light)
        : m_platform { .type = WebCore::FontPalette::Type::Light, .identifier = nullAtom() }
    {
    }

    FontPalette(CSS::Keyword::Dark)
        : m_platform { .type = WebCore::FontPalette::Type::Dark, .identifier = nullAtom() }
    {
    }

    FontPalette(CustomIdentifier&& identifier)
        : m_platform { .type = WebCore::FontPalette::Type::Custom, .identifier = WTFMove(identifier.value) }
    {
    }

    FontPalette(WebCore::FontPalette&& platform)
        : m_platform { WTFMove(platform) }
    {
    }

    FontPalette(const WebCore::FontPalette& platform)
        : m_platform { platform }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_platform.type) {
        case WebCore::FontPalette::Type::Normal:
            return visitor(CSS::Keyword::Normal { });
        case WebCore::FontPalette::Type::Light:
            return visitor(CSS::Keyword::Light { });
        case WebCore::FontPalette::Type::Dark:
            return visitor(CSS::Keyword::Dark { });
        case WebCore::FontPalette::Type::Custom:
            return visitor(CustomIdentifier { m_platform.identifier });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    const WebCore::FontPalette& platform() const { return m_platform; }

    bool operator==(const FontPalette&) const = default;

private:
    WebCore::FontPalette m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontPalette> { auto operator()(BuilderState&, const CSSValue&) -> FontPalette; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontPalette)
