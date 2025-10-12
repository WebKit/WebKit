/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

class FontMetrics;

namespace Style {

struct TextDecorationThicknessLength : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

// <'text-decoration-thickness'> = auto | from-font | <length-percentage>
// https://drafts.csswg.org/css-text-decor-4/#propdef-text-decoration-thickness
struct TextDecorationThickness {
    TextDecorationThickness(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    TextDecorationThickness(CSS::Keyword::FromFont keyword)
        : m_value { keyword }
    {
    }

    TextDecorationThickness(TextDecorationThicknessLength&& length)
        : m_value { WTFMove(length) }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isFromFont() const { return WTF::holdsAlternative<CSS::Keyword::FromFont>(m_value); }
    bool isLength() const { return WTF::holdsAlternative<TextDecorationThicknessLength>(m_value); }

    float resolve(const RenderStyle&) const;
    float resolve(float fontSize, const FontMetrics&) const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const TextDecorationThickness&) const = default;

private:
    Variant<CSS::Keyword::Auto, CSS::Keyword::FromFont, TextDecorationThicknessLength> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextDecorationThickness> { auto operator()(BuilderState&, const CSSValue&) -> TextDecorationThickness; };

// MARK: - Blending

template<> struct Blending<TextDecorationThickness> {
    auto canBlend(const TextDecorationThickness&, const TextDecorationThickness&, const RenderStyle&, const RenderStyle&) -> bool;
    auto blend(const TextDecorationThickness&, const TextDecorationThickness&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> TextDecorationThickness;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationThicknessLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationThickness)
