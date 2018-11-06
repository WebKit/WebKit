/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef TextRun_h
#define TextRun_h

#include "TextFlags.h"
#include "WritingMode.h"
#include <wtf/text/StringView.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontCascade;
class GraphicsContext;
class GlyphBuffer;
class Font;

struct GlyphData;
struct WidthIterator;

class TextRun {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TextRun(const String& text, float xpos = 0, float expansion = 0, ExpansionBehavior expansionBehavior = DefaultExpansion, TextDirection direction = TextDirection::LTR, bool directionalOverride = false, bool characterScanForCodePath = true)
        : m_text(text)
        , m_tabSize(0)
        , m_xpos(xpos)
        , m_horizontalGlyphStretch(1)
        , m_expansion(expansion)
        , m_expansionBehavior(expansionBehavior)
        , m_allowTabs(false)
        , m_direction(static_cast<unsigned>(direction))
        , m_directionalOverride(directionalOverride)
        , m_characterScanForCodePath(characterScanForCodePath)
        , m_disableSpacing(false)
    {
        ASSERT(!m_text.isNull());
    }

    explicit TextRun(StringView stringView, float xpos = 0, float expansion = 0, ExpansionBehavior expansionBehavior = DefaultExpansion, TextDirection direction = TextDirection::LTR, bool directionalOverride = false, bool characterScanForCodePath = true)
        : TextRun(stringView.toStringWithoutCopying(), xpos, expansion, expansionBehavior, direction, directionalOverride, characterScanForCodePath)
    {
    }

    TextRun subRun(unsigned startOffset, unsigned length) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(startOffset < m_text.length());

        auto result { *this };

        if (is8Bit())
            result.setText(data8(startOffset), length);
        else
            result.setText(data16(startOffset), length);
        return result;
    }

    UChar operator[](unsigned i) const { ASSERT_WITH_SECURITY_IMPLICATION(i < m_text.length()); return m_text[i]; }
    const LChar* data8(unsigned i) const { ASSERT_WITH_SECURITY_IMPLICATION(i < m_text.length()); ASSERT(is8Bit()); return &m_text.characters8()[i]; }
    const UChar* data16(unsigned i) const { ASSERT_WITH_SECURITY_IMPLICATION(i < m_text.length()); ASSERT(!is8Bit()); return &m_text.characters16()[i]; }

    const LChar* characters8() const { ASSERT(is8Bit()); return m_text.characters8(); }
    const UChar* characters16() const { ASSERT(!is8Bit()); return m_text.characters16(); }

    bool is8Bit() const { return m_text.is8Bit(); }
    unsigned length() const { return m_text.length(); }

    void setText(const LChar* text, unsigned length) { setText({ text, length }); }
    void setText(const UChar* text, unsigned length) { setText({ text, length }); }
    void setText(StringView text) { ASSERT(!text.isNull()); m_text = text.toStringWithoutCopying(); }

    float horizontalGlyphStretch() const { return m_horizontalGlyphStretch; }
    void setHorizontalGlyphStretch(float scale) { m_horizontalGlyphStretch = scale; }

    bool allowTabs() const { return m_allowTabs; }
    unsigned tabSize() const { return m_tabSize; }
    void setTabSize(bool, unsigned);

    float xPos() const { return m_xpos; }
    void setXPos(float xPos) { m_xpos = xPos; }
    float expansion() const { return m_expansion; }
    ExpansionBehavior expansionBehavior() const { return m_expansionBehavior; }
    TextDirection direction() const { return static_cast<TextDirection>(m_direction); }
    bool rtl() const { return static_cast<TextDirection>(m_direction) == TextDirection::RTL; }
    bool ltr() const { return static_cast<TextDirection>(m_direction) == TextDirection::LTR; }
    bool directionalOverride() const { return m_directionalOverride; }
    bool characterScanForCodePath() const { return m_characterScanForCodePath; }
    bool spacingDisabled() const { return m_disableSpacing; }

    void disableSpacing() { m_disableSpacing = true; }
    void setDirection(TextDirection direction) { m_direction = static_cast<unsigned>(direction); }
    void setDirectionalOverride(bool override) { m_directionalOverride = override; }
    void setCharacterScanForCodePath(bool scan) { m_characterScanForCodePath = scan; }
    StringView text() const { return m_text; }

private:
    String m_text;

    unsigned m_tabSize;

    // m_xpos is the x position relative to the left start of the text line, not relative to the left
    // start of the containing block. In the case of right alignment or center alignment, left start of
    // the text line is not the same as left start of the containing block. This variable is only used
    // to calculate the width of \t
    float m_xpos;  
    float m_horizontalGlyphStretch;

    float m_expansion;
    unsigned m_expansionBehavior : 4;
    unsigned m_allowTabs : 1;
    unsigned m_direction : 1;
    unsigned m_directionalOverride : 1; // Was this direction set by an override character.
    unsigned m_characterScanForCodePath : 1;
    unsigned m_disableSpacing : 1;
};

inline void TextRun::setTabSize(bool allow, unsigned size)
{
    m_allowTabs = allow;
    m_tabSize = size;
}

}

#endif
