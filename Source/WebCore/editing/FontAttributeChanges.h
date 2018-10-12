/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FontShadow.h"
#include <wtf/Forward.h>
#include <wtf/Optional.h>

namespace WebCore {

class EditingStyle;
class MutableStyleProperties;

enum class EditAction : uint8_t;
enum class VerticalAlignChange : uint8_t { Superscript, Baseline, Subscript };

class FontChanges {
public:
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, FontChanges&);

    void setFontName(const String& fontName) { m_fontName = fontName; }
    void setFontFamily(const String& fontFamily) { m_fontFamily = fontFamily; }
    void setFontSize(double fontSize) { m_fontSize = fontSize; }
    void setFontSizeDelta(double fontSizeDelta) { m_fontSizeDelta = fontSizeDelta; }
    void setBold(bool bold) { m_bold = bold; }
    void setItalic(bool italic) { m_italic = italic; }

    bool isEmpty() const
    {
        return !m_fontName && !m_fontFamily && !m_fontSize && !m_fontSizeDelta && !m_bold && !m_italic;
    }

    WEBCORE_EXPORT Ref<EditingStyle> createEditingStyle() const;
    Ref<MutableStyleProperties> createStyleProperties() const;

private:
    const String& platformFontFamilyNameForCSS() const;

    String m_fontName;
    String m_fontFamily;
    std::optional<double> m_fontSize;
    std::optional<double> m_fontSizeDelta;
    std::optional<bool> m_bold;
    std::optional<bool> m_italic;
};

class FontAttributeChanges {
public:
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, FontAttributeChanges&);

    void setVerticalAlign(VerticalAlignChange align) { m_verticalAlign = align; }
    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }
    void setForegroundColor(const Color& color) { m_foregroundColor = color; }
    void setShadow(const FontShadow& shadow) { m_shadow = shadow; }
    void setStrikeThrough(bool strikeThrough) { m_strikeThrough = strikeThrough; }
    void setUnderline(bool underline) { m_underline = underline; }
    void setFontChanges(const FontChanges& fontChanges) { m_fontChanges = fontChanges; }

    WEBCORE_EXPORT Ref<EditingStyle> createEditingStyle() const;
    WEBCORE_EXPORT EditAction editAction() const;

private:
    std::optional<VerticalAlignChange> m_verticalAlign;
    std::optional<Color> m_backgroundColor;
    std::optional<Color> m_foregroundColor;
    std::optional<FontShadow> m_shadow;
    std::optional<bool> m_strikeThrough;
    std::optional<bool> m_underline;
    FontChanges m_fontChanges;
};

template<class Encoder>
void FontChanges::encode(Encoder& encoder) const
{
    ASSERT(!m_fontSize || !m_fontSizeDelta);
    encoder << m_fontName << m_fontFamily << m_fontSize << m_fontSizeDelta << m_bold << m_italic;
}

template<class Decoder>
bool FontChanges::decode(Decoder& decoder, FontChanges& changes)
{
    if (!decoder.decode(changes.m_fontName))
        return false;

    if (!decoder.decode(changes.m_fontFamily))
        return false;

    if (!decoder.decode(changes.m_fontSize))
        return false;

    if (!decoder.decode(changes.m_fontSizeDelta))
        return false;

    if (!decoder.decode(changes.m_bold))
        return false;

    if (!decoder.decode(changes.m_italic))
        return false;

    ASSERT(!changes.m_fontSize || !changes.m_fontSizeDelta);
    return true;
}

template<class Encoder>
void FontAttributeChanges::encode(Encoder& encoder) const
{
    encoder << m_verticalAlign << m_backgroundColor << m_foregroundColor << m_shadow << m_strikeThrough << m_underline << m_fontChanges;
}

template<class Decoder>
bool FontAttributeChanges::decode(Decoder& decoder, FontAttributeChanges& changes)
{
    if (!decoder.decode(changes.m_verticalAlign))
        return false;

    if (!decoder.decode(changes.m_backgroundColor))
        return false;

    if (!decoder.decode(changes.m_foregroundColor))
        return false;

    if (!decoder.decode(changes.m_shadow))
        return false;

    if (!decoder.decode(changes.m_strikeThrough))
        return false;

    if (!decoder.decode(changes.m_underline))
        return false;

    if (!decoder.decode(changes.m_fontChanges))
        return false;

    return true;
}

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::VerticalAlignChange> {
    using values = EnumValues<
        WebCore::VerticalAlignChange,
        WebCore::VerticalAlignChange::Superscript,
        WebCore::VerticalAlignChange::Baseline,
        WebCore::VerticalAlignChange::Subscript
    >;
};
}
