



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

#include <WebCore/StyleValueTypes.h>
#include <WebCore/TextFlags.h>

namespace WebCore {
namespace Style {

// <'font-variant-east-asian'> = normal | [ <east-asian-variant-values> || <east-asian-width-values> || ruby ]
// https://drafts.csswg.org/css-fonts-4/#propdef-font-variant-east-asian
struct FontVariantEastAsian {
    using Platform = WebCore::FontVariantEastAsianValues;

    constexpr FontVariantEastAsian(CSS::Keyword::Normal)
        : m_platform { }
    {
    }

    constexpr FontVariantEastAsian(Platform value)
        : m_platform { value }
    {
    }

    constexpr Platform platform() const { return m_platform; }

    constexpr bool isNormal() const
    {
        return m_platform.variant == FontVariantEastAsianVariant::Normal
            && m_platform.width == FontVariantEastAsianWidth::Normal
            && m_platform.ruby == FontVariantEastAsianRuby::Normal;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNormal())
            return visitor(CSS::Keyword::Normal { });

        auto variantValue = [](auto value) -> std::optional<Variant<CSS::Keyword::Jis78, CSS::Keyword::Jis83, CSS::Keyword::Jis90, CSS::Keyword::Jis04, CSS::Keyword::Simplified, CSS::Keyword::Traditional>> {
            switch (value) {
            case FontVariantEastAsianVariant::Normal:       return std::nullopt;
            case FontVariantEastAsianVariant::Jis78:        return CSS::Keyword::Jis78 { };
            case FontVariantEastAsianVariant::Jis83:        return CSS::Keyword::Jis83 { };
            case FontVariantEastAsianVariant::Jis90:        return CSS::Keyword::Jis90 { };
            case FontVariantEastAsianVariant::Jis04:        return CSS::Keyword::Jis04 { };
            case FontVariantEastAsianVariant::Simplified:   return CSS::Keyword::Simplified { };
            case FontVariantEastAsianVariant::Traditional:  return CSS::Keyword::Traditional { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto widthValue = [](auto value) -> std::optional<Variant<CSS::Keyword::FullWidth, CSS::Keyword::ProportionalWidth>> {
            switch (value) {
            case FontVariantEastAsianWidth::Normal:         return std::nullopt;
            case FontVariantEastAsianWidth::Full:           return CSS::Keyword::FullWidth { };
            case FontVariantEastAsianWidth::Proportional:   return CSS::Keyword::ProportionalWidth { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto rubyValue = [](auto value) -> std::optional<CSS::Keyword::Ruby> {
            switch (value) {
            case FontVariantEastAsianRuby::Normal:          return std::nullopt;
            case FontVariantEastAsianRuby::Yes:             return CSS::Keyword::Ruby { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };

        return visitor(SpaceSeparatedTuple {
            variantValue(m_platform.variant),
            widthValue(m_platform.width),
            rubyValue(m_platform.ruby),
        });
    }

    constexpr bool operator==(const FontVariantEastAsian&) const = default;

private:
    Platform m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontVariantEastAsian> { auto operator()(BuilderState&, const CSSValue&) -> FontVariantEastAsian; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontVariantEastAsian)
