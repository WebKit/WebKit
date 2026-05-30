/*
 * Copyright (C) 2026 Alexsander Borges Damaceno <alexbdamac@gmail.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {

namespace Style {

// Single inset edge: a resolved <length>.
struct TextDecorationInsetValue : LengthWrapperBase<LengthPercentage<CSS::AllUnzoomed>> {
    using Base::Base;

    float resolve(const Style::ComputedStyle&) const;
};

// <'text-decoration-inset'> = auto | <length>{1,2}
// https://drafts.csswg.org/css-text-decor-4/#text-decoration-skip-inset-property
struct TextDecorationInset {
    using Value = MinimallySerializingSpaceSeparatedPair<TextDecorationInsetValue>;

    TextDecorationInset(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    TextDecorationInset(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : m_value { Value { TextDecorationInsetValue { literal }, TextDecorationInsetValue { literal } } }
    {
    }

    TextDecorationInset(TextDecorationInsetValue startInset, TextDecorationInsetValue endInset)
        : m_value { Value { startInset, endInset } }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    const TextDecorationInsetValue& startInset() const { return std::get<Value>(m_value).first(); }
    const TextDecorationInsetValue& endInset()   const { return std::get<Value>(m_value).second(); }

    std::pair<float, float> resolve(const Style::ComputedStyle&) const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const TextDecorationInset&) const = default;

private:
    Variant<CSS::Keyword::Auto, Value> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextDecorationInset> { auto operator()(BuilderState&, const CSSValue&) -> TextDecorationInset; };

// MARK: - Blending

template<> struct Blending<TextDecorationInset> {
    auto canBlend(const TextDecorationInset&, const TextDecorationInset&, const Style::ComputedStyle&, const Style::ComputedStyle&) -> bool;
    auto blend(const TextDecorationInset&, const TextDecorationInset&, const Style::ComputedStyle&, const Style::ComputedStyle&, const BlendingContext&) -> TextDecorationInset;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationInsetValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationInset)
