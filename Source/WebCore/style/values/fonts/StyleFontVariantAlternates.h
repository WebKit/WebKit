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

// <'font-variant-alternates'> = normal | [ stylistic(<feature-value-name>) || historical-forms || styleset(<feature-value-name>#) || character-variant(<feature-value-name>#) || swash(<feature-value-name>) || ornaments(<feature-value-name>) || annotation(<feature-value-name>) ]
// https://drafts.csswg.org/css-fonts-4/#propdef-font-variant-alternates
struct FontVariantAlternates {
    using Platform = WebCore::FontVariantAlternates;

    FontVariantAlternates(CSS::Keyword::Normal)
        : m_platform { Platform::Normal() }
    {
    }

    FontVariantAlternates(CSS::Keyword::HistoricalForms)
        : m_platform { Platform::Normal() }
    {
        m_platform.valuesRef().historicalForms = true;
    }

    FontVariantAlternates(Platform value)
        : m_platform { value }
    {
    }

    const Platform& platform() const { return m_platform; }
    Platform takePlatform() { return WTFMove(m_platform); }

    bool isNormal() const { return m_platform.isNormal(); }

    bool operator==(const FontVariantAlternates&) const = default;

private:
    Platform m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontVariantAlternates> { auto operator()(BuilderState&, const CSSValue&) -> FontVariantAlternates; };
template<> struct CSSValueCreation<FontVariantAlternates> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const FontVariantAlternates&); };

// MARK: - Serialization

template<> struct Serialize<FontVariantAlternates> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const FontVariantAlternates&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const FontVariantAlternates&);

} // namespace Style
} // namespace WebCore
