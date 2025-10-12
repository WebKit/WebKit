



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

// <'font-variant-ligatures'> = normal | none | [ <common-lig-values> || <discretionary-lig-values> || <historical-lig-values> || <contextual-alt-values> ]
// https://drafts.csswg.org/css-fonts-4/#font-variant-ligatures-prop
struct FontVariantLigatures {
    using Platform = WebCore::FontVariantLigaturesValues;

    constexpr FontVariantLigatures(CSS::Keyword::Normal)
        : m_platform { }
    {
    }

    constexpr FontVariantLigatures(CSS::Keyword::None)
        : m_platform { WebCore::FontVariantLigatures::No, WebCore::FontVariantLigatures::No, WebCore::FontVariantLigatures::No, WebCore::FontVariantLigatures::No }
    {
    }

    constexpr FontVariantLigatures(Platform value)
        : m_platform { value }
    {
    }

    constexpr Platform platform() const { return m_platform; }

    constexpr bool isNormal() const
    {
        using enum WebCore::FontVariantLigatures;
        return m_platform.common == Normal
            && m_platform.discretionary == Normal
            && m_platform.historical == Normal
            && m_platform.contextual == Normal;
    }

    constexpr bool isNone() const
    {
        using enum WebCore::FontVariantLigatures;
        return m_platform.common == No
            && m_platform.discretionary == No
            && m_platform.historical == No
            && m_platform.contextual == No;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        if (isNormal())
            return visitor(CSS::Keyword::Normal { });

        auto tupleValue = []<typename YesType, typename NoType>(auto value) -> std::optional<Variant<YesType, NoType>> {
            using enum WebCore::FontVariantLigatures;
            switch (value) {
            case Normal:    return std::nullopt;
            case No:        return NoType { };
            case Yes:       return YesType { };
            }
            RELEASE_ASSERT_NOT_REACHED();
        };

        return visitor(SpaceSeparatedTuple {
            tupleValue.template operator()<CSS::Keyword::CommonLigatures, CSS::Keyword::NoCommonLigatures>(m_platform.common),
            tupleValue.template operator()<CSS::Keyword::DiscretionaryLigatures, CSS::Keyword::NoDiscretionaryLigatures>(m_platform.discretionary),
            tupleValue.template operator()<CSS::Keyword::HistoricalLigatures, CSS::Keyword::NoHistoricalLigatures>(m_platform.historical),
            tupleValue.template operator()<CSS::Keyword::Contextual, CSS::Keyword::NoContextual>(m_platform.contextual),
        });
    }

    constexpr bool operator==(const FontVariantLigatures&) const = default;

private:
    Platform m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontVariantLigatures> { auto operator()(BuilderState&, const CSSValue&) -> FontVariantLigatures; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontVariantLigatures)
