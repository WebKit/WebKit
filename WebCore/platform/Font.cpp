/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "Font.h"
#include "FontData.h"

#include "FontFallbackList.h"
#include "GraphicsContext.h"
#include "KWQKHTMLSettings.h"

#include "GlyphBuffer.h"

#include <unicode/umachine.h>

namespace WebCore {

#if __APPLE__

// FIXME: Cross-platform eventually, but for now we compile only on OS X.
#define SPACE 0x0020
#define NO_BREAK_SPACE 0x00A0

// According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
#define HIRAGANA_KATAKANA_VOICING_MARKS 8

inline bool isSpace(unsigned c)
{
    return c == SPACE || c == '\t' || c == '\n' || c == NO_BREAK_SPACE;
}

static const uint8_t isRoundingHackCharacterTable[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

inline bool isRoundingHackCharacter(UChar32 c)
{
    return (((c & ~0xFF) == 0 && isRoundingHackCharacterTable[c]));
}

struct WidthIterator {
    WidthIterator(const Font* font, const UChar* str, int from, int to, int len,
                  int tabWidth, int xpos, int toAdd, TextDirection dir, bool visuallyOrdered,
                  bool applyWordRounding, bool applyRunRounding, const FontData* substituteFontData);

    void advance(int to, GlyphBuffer* glyphBuffer = 0);

    const Font* m_font;

    const UChar* m_characters;
    int m_from;
    int m_to;
    int m_len;
    int m_tabWidth;
    int m_xPos;
    int m_toAdd;
    TextDirection m_dir;
    bool m_visuallyOrdered;
    bool m_applyWordRounding;
    bool m_applyRunRounding;
    const FontData* m_substituteFontData;

    unsigned m_currentCharacter;
    float m_runWidthSoFar;
    float m_widthToStart;
    float m_padding;
    float m_padPerSpace;
    float m_finalRoundingWidth;
    
private:
    UChar32 normalizeVoicingMarks();
};

WidthIterator::WidthIterator(const Font* font, const UChar* str, int from, int to, int len,
                             int tabWidth, int xpos, int toAdd, TextDirection dir, bool visuallyOrdered,
                             bool applyWordRounding, bool applyRunRounding, const FontData* substituteFontData)
:m_font(font), m_characters((const UChar*)str), m_from(from), m_to(to), m_len(len),
 m_tabWidth(tabWidth), m_xPos(xpos), m_toAdd(toAdd), m_dir(dir), m_visuallyOrdered(visuallyOrdered),
 m_applyWordRounding(applyWordRounding), m_applyRunRounding(applyRunRounding), m_substituteFontData(substituteFontData),
 m_currentCharacter(from), m_runWidthSoFar(0), m_finalRoundingWidth(0)
{
    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    if (!toAdd) {
        m_padding = 0;
        m_padPerSpace = 0;
    } else {
        float numSpaces = 0;
        for (int i = from; i < to; i++)
            if (isSpace(m_characters[i]))
                numSpaces++;

        m_padding = toAdd;
        m_padPerSpace = ceilf(m_padding / numSpaces);
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (m_from == 0)
        m_widthToStart = 0;
    else {
        WidthIterator startPositionIterator(font, str, 0, len, len, tabWidth, xpos, toAdd, dir, visuallyOrdered,
                                            applyWordRounding, applyRunRounding, substituteFontData);
        startPositionIterator.advance(from);
        m_widthToStart = startPositionIterator.m_runWidthSoFar;
    }
}

void WidthIterator::advance(int offset, GlyphBuffer* glyphBuffer)
{
    if (offset > m_to)
        offset = m_to;

    int currentCharacter = m_currentCharacter;
    const UChar* cp = &m_characters[currentCharacter];

    bool rtl = (m_dir == RTL);
    bool needCharTransform = rtl || m_font->isSmallCaps();
    bool hasExtraSpacing = m_font->letterSpacing() || m_font->wordSpacing() || m_padding;

    float runWidthSoFar = m_runWidthSoFar;
    float lastRoundingWidth = m_finalRoundingWidth;

    const FontData* primaryFontData = m_font->primaryFont();
    
    while (currentCharacter < offset) {
        UChar32 c = *cp;
        unsigned clusterLength = 1;
        if (c >= 0x3041) {
            if (c <= 0x30FE) {
                // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
                // Normalize into composed form, and then look for glyph with base + combined mark.
                // Check above for character range to minimize performance impact.
                UChar32 normalized = normalizeVoicingMarks();
                if (normalized) {
                    c = normalized;
                    clusterLength = 2;
                }
            } else if (U16_IS_SURROGATE(c)) {
                if (!U16_IS_SURROGATE_LEAD(c))
                    break;

                // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
                // code point before glyph lookup.
                // Make sure we have another character and it's a low surrogate.
                if (currentCharacter + 1 >= m_len)
                    break;
                UniChar low = cp[1];
                if (!U16_IS_TRAIL(low))
                    break;
                c = U16_GET_SUPPLEMENTARY(c, low);
                clusterLength = 2;
            }
        }

        const FontData* fontData = m_substituteFontData ? m_substituteFontData :primaryFontData;

        if (needCharTransform) {
            if (rtl)
                c = u_charMirror(c);

            // If small-caps, convert lowercase to upper.
            if (m_font->isSmallCaps() && !u_isUUppercase(c)) {
                UChar32 upperC = u_toupper(c);
                if (upperC != c) {
                    c = upperC;
                    fontData = fontData->smallCapsFontData();
                }
            }
        }

        // FIXME: Should go through fallback list eventually when we rework the glyph map.
        Glyph glyph = fontData->glyphForCharacter(&fontData, c);

        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string. If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
        if (glyph == 0 && !m_substituteFontData) {
            // FIXME: Should go through fallback list eventually.
            const FontData* substituteFontData = fontData->findSubstituteFontData(cp, clusterLength, m_font->fontDescription());
            if (substituteFontData) {
                GlyphBuffer localGlyphBuffer;
                m_font->floatWidthForSimpleText((UChar*)cp, clusterLength, 0, clusterLength, 0, 0, 0, m_dir, m_visuallyOrdered, 
                                                 m_applyWordRounding, false, substituteFontData, 0, &localGlyphBuffer);
                if (localGlyphBuffer.size() == 1) {
                    assert(substituteFontData == localGlyphBuffer.fontDataAt(0));
                    glyph = localGlyphBuffer.glyphAt(0);
                    fontData->updateGlyphMapEntry(c, glyph, substituteFontData);
                    fontData = substituteFontData;
                }
            }
        }

        // Now that we have a glyph and font data, get its width.
        float width;
        if (c == '\t' && m_tabWidth)
            width = m_tabWidth - fmodf(m_xPos + runWidthSoFar, m_tabWidth);
        else {
            width = fontData->widthForGlyph(glyph);
            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (width == fontData->m_spaceWidth && (fontData->m_treatAsFixedPitch || glyph == fontData->m_spaceGlyph) && m_applyWordRounding)
                width = fontData->m_adjustedSpaceWidth;
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && m_font->letterSpacing())
                width += m_font->letterSpacing();

            if (isSpace(c)) {
                // Account for padding. WebCore uses space padding to justify text.
                // We distribute the specified padding over the available spaces in the run.
                if (m_padding) {
                    // Use left over padding if not evenly divisible by number of spaces.
                    if (m_padding < m_padPerSpace) {
                        width += m_padding;
                        m_padding = 0;
                    } else {
                        width += m_padPerSpace;
                        m_padding -= m_padPerSpace;
                    }
                }

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                if (currentCharacter != 0 && !isSpace(cp[-1]) && m_font->wordSpacing())
                    width += m_font->wordSpacing();
            }
        }

        // Advance past the character we just dealt with.
        cp += clusterLength;
        currentCharacter += clusterLength;

        // Account for float/integer impedance mismatch between CG and KHTML. "Words" (characters 
        // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
        // We adjust the width of the last character of a "word" to ensure an integer width.
        // If we move KHTML to floats we can remove this (and related) hacks.

        float oldWidth = width;

        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so following words will start on an integer boundary.
        if (m_applyWordRounding && isRoundingHackCharacter(c))
            width = ceilf(width);

        // Check to see if the next character is a "rounding hack character", if so, adjust
        // width so that the total run width will be on an integer boundary.
        if ((m_applyWordRounding && currentCharacter < m_len && isRoundingHackCharacter(*cp))
                || (m_applyRunRounding && currentCharacter >= m_to)) {
            float totalWidth = m_widthToStart + runWidthSoFar + width;
            width += ceilf(totalWidth) - totalWidth;
        }

        runWidthSoFar += width;

        if (glyphBuffer)
            glyphBuffer->add(glyph, fontData, (rtl ? oldWidth + lastRoundingWidth : width));

        lastRoundingWidth = width - oldWidth;
    }

    m_currentCharacter = currentCharacter;
    m_runWidthSoFar = runWidthSoFar;
    m_finalRoundingWidth = lastRoundingWidth;
}

UChar32 WidthIterator::normalizeVoicingMarks()
{
    int currentCharacter = m_currentCharacter;
    if (currentCharacter + 1 < m_to) {
        if (u_getCombiningClass(m_characters[currentCharacter + 1]) == HIRAGANA_KATAKANA_VOICING_MARKS) {
            // Normalize into composed form using 3.2 rules.
            UChar normalizedCharacters[2] = { 0, 0 };
            UErrorCode uStatus = (UErrorCode)0;  
            int32_t resultLength = unorm_normalize(&m_characters[currentCharacter], 2,
                UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
            if (resultLength == 1 && uStatus == 0)
                return normalizedCharacters[0];
        }
    }
    return 0;
}
#endif

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font() :m_fontList(0), m_letterSpacing(0), m_wordSpacing(0) {}
Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
: m_fontDescription(fd),
  m_fontList(0),
  m_letterSpacing(letterSpacing),
  m_wordSpacing(wordSpacing)
{}

Font::Font(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontList = other.m_fontList;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
}

Font& Font::operator=(const Font& other)
{
    if (&other != this) {
        m_fontDescription = other.m_fontDescription;
        m_fontList = other.m_fontList;
        m_letterSpacing = other.m_letterSpacing;
        m_wordSpacing = other.m_wordSpacing;
    }
    return *this;
}

Font::~Font()
{
}

const FontData* Font::primaryFont() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(m_fontDescription);
}

void Font::update() const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway.  Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble).  Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontList)
        m_fontList = new FontFallbackList();
    m_fontList->invalidate();
}

int Font::width(const UChar* chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    // FIXME: Want to define an lroundf for win32.
#if __APPLE__
    return lroundf(floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos));
#else
    return floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos) + 0.5f;
#endif
}

int Font::ascent() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(fontDescription())->ascent();
}

int Font::descent() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(fontDescription())->descent();
}

int Font::lineSpacing() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(fontDescription())->lineSpacing();
}

float Font::xHeight() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(fontDescription())->xHeight();
}

bool Font::isFixedPitch() const
{
    assert(m_fontList);
    return m_fontList->isFixedPitch(fontDescription());
}

#if __APPLE__
// FIXME: These methods will eventually be cross-platform, but to keep Windows compiling we'll make this Apple-only for now.
bool Font::gAlwaysUseComplexPath = false;
void Font::setAlwaysUseComplexPath(bool alwaysUse)
{
    gAlwaysUseComplexPath = alwaysUse;
}

bool Font::canUseGlyphCache(const UChar* str, int to) const
{
    if (gAlwaysUseComplexPath)
        return false;
    
    // Start from 0 since drawing and highlighting also measure the characters before run->from
    for (int i = 0; i < to; i++) {
        UChar c = str[i];
        if (c < 0x300)      // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return false;

        if (c < 0x0591 || c == 0x05BE)     // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return false;

        if (c < 0x0600)     // U+0600 through U+1059 Arabic, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
            continue;
        if (c <= 0x1059)
            return false;

        if (c < 0x1100)     // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose; Modern Korean will be precomposed as a result of step A)
            continue;
        if (c <= 0x11FF)
            return false;

        if (c < 0x1780)     // U+1780 through U+18AF Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return false;

        if (c < 0x1900)     // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return false;

        if (c < 0x20D0)     // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return false;

        if (c < 0xFE20)     // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return false;
    }

    return true;

}

void Font::drawSimpleText(GraphicsContext* context, const IntPoint& point, int tabWidth, int xpos, const UChar* str, int len, int from, int to,
                          int toAdd, TextDirection dir, bool visuallyOrdered) const
{
    // This glyph buffer holds our glyphs+advances+font data for each glyph.
    GlyphBuffer glyphBuffer;

    // Our measuring code will generate glyphs and advances for us.
    float startX;
    floatWidthForSimpleText(str, len, from, to, tabWidth, xpos, toAdd, dir, visuallyOrdered, 
                            true, true, 0,
                            &startX, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    startX += point.x();
    FloatPoint startPoint(startX, point.y());

    // Swap the order of the glyphs if right-to-left.
    if (dir == RTL)
        for (int i = 0, end = glyphBuffer.size() - 1; i < glyphBuffer.size() / 2; ++i, --end)
            glyphBuffer.swap(i, end);

    // Draw each contiguous run of glyphs that use the same font data.
    const FontData* fontData = glyphBuffer.fontDataAt(0);
    float nextX = startX;
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < glyphBuffer.size()) {
        const FontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);
        if (nextFontData != fontData) {
            drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
            lastFrom = nextGlyph;
            fontData = nextFontData;
            startPoint.setX(nextX);
        }
        nextX += glyphBuffer.advanceAt(nextGlyph);
        nextGlyph++;
    }
    drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
}

void Font::drawText(GraphicsContext* context, const IntPoint& point, int tabWidth, int xpos, const UChar* str, int len, int from, int to,
                    int toAdd, TextDirection d, bool visuallyOrdered) const
{
    if (len == 0)
        return;

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;

    if (canUseGlyphCache(str, to))
        drawSimpleText(context, point, tabWidth, xpos, str, len, from, to, toAdd, d, visuallyOrdered);
    else
        drawComplexText(context, point, tabWidth, xpos, str, len, from, to, toAdd, d, visuallyOrdered);
}

float Font::floatWidth(const UChar* str, int slen, int from, int len, int tabWidth, int xpos, bool runRounding) const
{
    int to = from + len;
    if (canUseGlyphCache(str, to))
        return floatWidthForSimpleText(str, slen, from, to, tabWidth, xpos, 0, LTR, false, true, runRounding, 0, 0, 0);
    else
        return floatWidthForComplexText(str, slen, from, to, tabWidth, xpos, runRounding);
}

float Font::floatWidthForSimpleText(const UChar* str, int len, int from, int to, int tabWidth, int xpos, int toAdd, 
                                    TextDirection dir, bool visuallyOrdered, 
                                    bool applyWordRounding, bool applyRunRounding,
                                    const FontData* substituteFont,
                                    float* startPosition, GlyphBuffer* glyphBuffer) const
{
    WidthIterator it(this, str, from, dir == LTR ? to : len, len, tabWidth, xpos, toAdd, dir, visuallyOrdered, 
                     applyWordRounding, applyRunRounding, substituteFont);
    it.advance(to, glyphBuffer);
    float runWidth = it.m_runWidthSoFar;
    if (startPosition) {
        if (dir == LTR)
            *startPosition = it.m_widthToStart;
        else {
            float finalRoundingWidth = it.m_finalRoundingWidth;
            it.advance(len);
            *startPosition = it.m_runWidthSoFar - runWidth + finalRoundingWidth;
        }
    }
    return runWidth;
}
#endif

}
