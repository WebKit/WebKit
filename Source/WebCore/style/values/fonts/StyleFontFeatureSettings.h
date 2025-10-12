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

// <feature-tag-value> = <opentype-tag> [ <integer [0,∞]> | on | off ]?
// <'font-feature-settings'> = normal | <feature-tag-value>#
// https://drafts.csswg.org/css-fonts-4/#propdef-font-feature-settings
struct FontFeatureSettings {
    using Value = Integer<CSS::Nonnegative>;

    FontFeatureSettings(CSS::Keyword::Normal) : m_platform { } { }
    FontFeatureSettings(WebCore::FontFeatureSettings&& platform) : m_platform { WTFMove(platform) } { }
    FontFeatureSettings(const WebCore::FontFeatureSettings& platform) : m_platform { platform } { }

    const WebCore::FontFeatureSettings& platform() const { return m_platform; }
    WebCore::FontFeatureSettings takePlatform() { return WTFMove(m_platform); }

    bool operator==(const FontFeatureSettings&) const = default;

private:
    WebCore::FontFeatureSettings m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontFeatureSettings> { auto operator()(BuilderState&, const CSSValue&) -> FontFeatureSettings; };
template<> struct CSSValueCreation<FontFeatureSettings> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const FontFeatureSettings&); };

// MARK: - Serialization

template<> struct Serialize<FontFeatureSettings> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const FontFeatureSettings&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const FontFeatureSettings&);

} // namespace Style
} // namespace WebCore
