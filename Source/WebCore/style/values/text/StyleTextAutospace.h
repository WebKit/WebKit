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

#include <WebCore/StyleValueTypes.h>
#include <WebCore/TextSpacing.h>

namespace WebCore {
namespace Style {

// <'text-autospace'> = normal | auto | no-autospace | [ ideograph-alpha || ideograph-numeric || insert ]
// FIXME: Current spec is `normal | auto | no-autospace | [ [ ideograph-alpha || ideograph-numeric || punctuation ] || [ insert | replace ] ]`
// https://drafts.csswg.org/css-text-4/#propdef-text-autospace
struct TextAutospace {
    constexpr TextAutospace(CSS::Keyword::Normal) : m_value { WebCore::TextAutospace::Type::Normal } { }
    constexpr TextAutospace(CSS::Keyword::Auto) : m_value { WebCore::TextAutospace::Type::Auto } { }
    constexpr TextAutospace(CSS::Keyword::NoAutospace) : m_value { } { }
    constexpr TextAutospace(CSS::Keyword::IdeographAlpha) : m_value { WebCore::TextAutospace::Type::IdeographAlpha } { }
    constexpr TextAutospace(CSS::Keyword::IdeographNumeric) : m_value { WebCore::TextAutospace::Type::IdeographNumeric } { }
    constexpr TextAutospace(CSS::Keyword::IdeographAlpha, CSS::Keyword::IdeographNumeric) : m_value { { WebCore::TextAutospace::Type::IdeographAlpha, WebCore::TextAutospace::Type::IdeographNumeric } } { }

    constexpr TextAutospace(WebCore::TextAutospace value) : m_value { value } { }

    constexpr bool isNormal() const { return m_value.isNormal(); }
    constexpr bool isAuto() const { return m_value.isAuto(); }
    constexpr bool isNoAutospace() const { return m_value.isNoAutospace(); }
    constexpr bool hasIdeographAlpha() const { return m_value.hasIdeographAlpha(); }
    constexpr bool hasIdeographNumeric() const { return m_value.hasIdeographNumeric(); }
    constexpr bool hasInsert() const { return m_value.hasInsert(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNormal())
            return visitor(CSS::Keyword::Normal { });
        if (isAuto())
            return visitor(CSS::Keyword::Auto { });
        if (isNoAutospace())
            return visitor(CSS::Keyword::NoAutospace { });

        auto ideographAlphaValue = [&]() -> std::optional<CSS::Keyword::IdeographAlpha> {
            if (hasIdeographAlpha())
                return CSS::Keyword::IdeographAlpha { };
            return std::nullopt;
        };
        auto ideographNumericValue = [&]() -> std::optional<CSS::Keyword::IdeographNumeric> {
            if (hasIdeographNumeric())
                return CSS::Keyword::IdeographNumeric { };
            return std::nullopt;
        };
        auto insertValue = [&]() -> std::optional<CSS::Keyword::Insert> {
            if (hasInsert())
                return CSS::Keyword::Insert { };
            return std::nullopt;
        };

        return visitor(SpaceSeparatedTuple {
            ideographAlphaValue(),
            ideographNumericValue(),
            insertValue(),
        });
    }

    bool shouldApplySpacing(WebCore::TextSpacing::CharacterClass firstCharacterClass, WebCore::TextSpacing::CharacterClass secondCharacterClass) const { return m_value.shouldApplySpacing(firstCharacterClass, secondCharacterClass); }
    bool shouldApplySpacing(char32_t firstCharacter, char32_t secondCharacter) const { return m_value.shouldApplySpacing(firstCharacter, secondCharacter); }

    constexpr bool operator==(const TextAutospace&) const = default;

private:
    friend struct ToPlatform<TextAutospace>;

    WebCore::TextAutospace m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextAutospace> { auto operator()(BuilderState&, const CSSValue&) -> TextAutospace; };

// MARK: - Platform

template<> struct ToPlatform<TextAutospace> {
    constexpr auto operator()(const TextAutospace& value) -> WebCore::TextAutospace
    {
        return value.m_value;
    }
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextAutospace);
