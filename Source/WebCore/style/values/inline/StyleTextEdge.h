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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <text-edge> = [ text | cap | ex | ideographic | ideographic-ink ]
//               [ text | alphabetic | ideographic | ideographic-ink ]?
// https://drafts.csswg.org/css-inline-3/#typedef-text-edge

struct TextEdgePair {
    TextEdgeOver over;
    TextEdgeUnder under;

    template<typename... F> constexpr  decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (over) {
        case TextEdgeOver::Text:
        case TextEdgeOver::Cap:
        case TextEdgeOver::Ex:
            if (under == TextEdgeUnder::Text)
                return visitor(over);
            break;
        case TextEdgeOver::Ideographic:
            if (under == TextEdgeUnder::Ideographic)
                return visitor(over);
            break;
        case TextEdgeOver::IdeographicInk:
            if (under == TextEdgeUnder::IdeographicInk)
                return visitor(over);
            break;
        }

        return visitor(SpaceSeparatedTuple { over, under });
    }

    constexpr bool operator==(const TextEdgePair&) const = default;
};

template<typename K> struct TextEdge {
    using Keyword = K;
    using Base = TextEdge<Keyword>;

    constexpr TextEdge(Keyword keyword) : m_value { keyword } { }
    constexpr TextEdge(TextEdgeOver over, TextEdgeUnder under) : m_value { TextEdgePair { over, under } } { }

    constexpr bool isKeyword() const { return holdsAlternative<Keyword>(); }
    constexpr bool isTextEdgePair() const { return holdsAlternative<TextEdgePair>(); }
    constexpr std::optional<TextEdgePair> tryTextEdgePair() const { return isTextEdgePair() ? std::make_optional(std::get<TextEdgePair>(m_value)) : std::nullopt; }

    template<typename U> constexpr bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    constexpr bool operator==(const TextEdge<Keyword>&) const = default;

protected:
    Variant<Keyword, TextEdgePair> m_value;
};

// MARK: - Concepts

template<typename T> concept TextEdgeDerived = WTF::IsBaseOfTemplate<TextEdge, T>::value && VariantLike<T>;

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextEdgePair)
