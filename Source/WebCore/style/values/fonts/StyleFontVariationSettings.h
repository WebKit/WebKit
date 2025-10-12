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

#include <WebCore/FontTaggedSettings.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'font-variation-settings'> = normal | [ <opentype-tag> <number> ]#
// https://drafts.csswg.org/css-fonts-4/#propdef-font-variation-settings
struct FontVariationSettings {
    using Value = Style::Number<CSS::All, float>;

    FontVariationSettings(CSS::Keyword::Normal) : m_platform { } { }
    FontVariationSettings(WebCore::FontVariationSettings&& platform) : m_platform { WTFMove(platform) } { }
    FontVariationSettings(const WebCore::FontVariationSettings& platform) : m_platform { platform } { }

    const WebCore::FontVariationSettings& platform() const { return m_platform; }
    WebCore::FontVariationSettings takePlatform() { return WTFMove(m_platform); }

    bool operator==(const FontVariationSettings&) const = default;

private:
    WebCore::FontVariationSettings m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontVariationSettings> { auto operator()(BuilderState&, const CSSValue&) -> FontVariationSettings; };
template<> struct CSSValueCreation<FontVariationSettings> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const FontVariationSettings&); };

// MARK: - Serialization

template<> struct Serialize<FontVariationSettings> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const FontVariationSettings&); };

// MARK: - Blending

template<> struct Blending<FontVariationSettings> {
    auto canBlend(const FontVariationSettings&, const FontVariationSettings&) -> bool;
    auto blend(const FontVariationSettings&, const FontVariationSettings&, const BlendingContext&) -> FontVariationSettings;
};

// MARK: - Logging

TextStream& operator<<(TextStream&, const FontVariationSettings&);

} // namespace Style
} // namespace WebCore
