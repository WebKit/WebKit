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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Font.h"

#include "CharacterNames.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "FontFallbackList.h"
#include "IntPoint.h"
#include "GlyphBuffer.h"
#include "TextStyle.h"
#include <wtf/unicode/Unicode.h>
#include <wtf/MathExtras.h>

#if USE(ICU_UNICODE)
#include <unicode/unorm.h>
#endif

using namespace WTF;
using namespace Unicode;

namespace WebCore {

// According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
const uint8_t hiraganaKatakanaVoicingMarksCombiningClass = 8;

const uint8_t Font::gRoundingHackCharacterTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

Font::CodePath Font::codePath = Auto;

struct WidthIterator {
    WidthIterator(const Font* font, const TextRun& run, const TextStyle& style);

    void advance(int to, GlyphBuffer* glyphBuffer = 0);
    bool advanceOneCharacter(float& width, GlyphBuffer* glyphBuffer = 0);
    
    const Font* m_font;

    const TextRun& m_run;
    int m_end;

    const TextStyle& m_style;
    
    unsigned m_currentCharacter;
    float m_runWidthSoFar;
    float m_padding;
    float m_padPerSpace;
    float m_finalRoundingWidth;
    
private:
    UChar32 normalizeVoicingMarks(int currentCharacter);
};

WidthIterator::WidthIterator(const Font* font, const TextRun& run, const TextStyle& style)
    : m_font(font)
    , m_run(run)
    , m_end(run.length())
    , m_style(style)
    , m_currentCharacter(0)
    , m_runWidthSoFar(0)
    , m_finalRoundingWidth(0)
{
    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    m_padding = m_style.padding();
    if (!m_padding)
        m_padPerSpace = 0;
    else {
        float numSpaces = 0;
        for (int i = 0; i < run.length(); i++)
            if (Font::treatAsSpace(m_run[i]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_style.padding() / numSpaces);
    }
}

void WidthIterator::advance(int offset, GlyphBuffer* glyphBuffer)
{
    if (offset > m_end)
        offset = m_end;

    int currentCharacter = m_currentCharacter;
    const UChar* cp = m_run.data(currentCharacter);

    bool rtl = m_style.rtl();
    bool attemptFontSubstitution = m_style.attemptFontSubstitution();
    bool hasExtraSpacing = m_font->letterSpacing() || m_font->wordSpacing() || m_padding;

    float runWidthSoFar = m_runWidthSoFar;
    float lastRoundingWidth = m_finalRoundingWidth;
    
    while (currentCharacter < offset) {
        UChar32 c = *cp;
        unsigned clusterLength = 1;
        if (c >= 0x3041) {
            if (c <= 0x30FE) {
                // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
                // Normalize into composed form, and then look for glyph with base + combined mark.
                // Check above for character range to minimize performance impact.
                UChar32 normalized = normalizeVoicingMarks(currentCharacter);
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
                if (currentCharacter + 1 >= m_run.length())
                    break;
                UChar low = cp[1];
                if (!U16_IS_TRAIL(low))
                    break;
                c = U16_GET_SUPPLEMENTARY(c, low);
                clusterLength = 2;
            }
        }

        const GlyphData& glyphData = m_font->glyphDataForCharacter(c, cp, clusterLength, rtl, attemptFontSubstitution);
        Glyph glyph = glyphData.glyph;
        const FontData* fontData = glyphData.fontData;

        ASSERT(fontData);

        // Now that we have a glyph and font data, get its width.
        float width;
        if (c == '\t' && m_style.allowTabs()) {
            float tabWidth = m_font->tabWidth();
            width = tabWidth - fmodf(m_style.xPos() + runWidthSoFar, tabWidth);
        } else {
            width = fontData->widthForGlyph(glyph);
            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (width == fontData->m_spaceWidth && (fontData->m_treatAsFixedPitch || glyph == fontData->m_spaceGlyph) && m_style.applyWordRounding())
                width = fontData->m_adjustedSpaceWidth;
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && m_font->letterSpacing())
                width += m_font->letterSpacing();

            if (Font::treatAsSpace(c)) {
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
                if (currentCharacter != 0 && !Font::treatAsSpace(cp[-1]) && m_font->wordSpacing())
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
        if (m_style.applyWordRounding() && Font::isRoundingHackCharacter(c))
            width = ceilf(width);

        // Check to see if the next character is a "rounding hack character", if so, adjust
        // width so that the total run width will be on an integer boundary.
        if ((m_style.applyWordRounding() && currentCharacter < m_run.length() && Font::isRoundingHackCharacter(*cp))
                || (m_style.applyRunRounding() && currentCharacter >= m_end)) {
            float totalWidth = runWidthSoFar + width;
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

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer* glyphBuffer)
{
    glyphBuffer->clear();
    advance(m_currentCharacter + 1, glyphBuffer);
    float w = 0;
    for (int i = 0; i < glyphBuffer->size(); ++i)
        w += glyphBuffer->advanceAt(i);
    width = w;
    return !glyphBuffer->isEmpty();
}

UChar32 WidthIterator::normalizeVoicingMarks(int currentCharacter)
{
    if (currentCharacter + 1 < m_end) {
        if (combiningClass(m_run[currentCharacter + 1]) == hiraganaKatakanaVoicingMarksCombiningClass) {
#if USE(ICU_UNICODE)
            // Normalize into composed form using 3.2 rules.
            UChar normalizedCharacters[2] = { 0, 0 };
            UErrorCode uStatus = U_ZERO_ERROR;  
            int32_t resultLength = unorm_normalize(m_run.data(currentCharacter), 2,
                UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
            if (resultLength == 1 && uStatus == 0)
                return normalizedCharacters[0];
#elif USE(QT4_UNICODE)
            QString tmp(reinterpret_cast<const QChar*>(m_run.data(currentCharacter)), 2);
            QString res = tmp.normalized(QString::NormalizationForm_C, QChar::Unicode_3_2);
            if (res.length() == 1)
                return res.at(0).unicode();
#endif
        }
    }
    return 0;
}

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font()
    : m_pageZero(0)
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_isPlatformFont(false)
{
}

Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
    : m_fontDescription(fd)
    , m_pageZero(0)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_isPlatformFont(false)
{
}

Font::Font(const FontPlatformData& fontData, bool isPrinterFont)
    : m_fontList(new FontFallbackList)
    , m_pageZero(0)
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_isPlatformFont(true)
{
    m_fontDescription.setUsePrinterFont(isPrinterFont);
    m_fontList->setPlatformFont(fontData);
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fontList(other.m_fontList)
    , m_pages(other.m_pages)
    , m_pageZero(other.m_pageZero)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_isPlatformFont(other.m_isPlatformFont)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontList = other.m_fontList;
    m_pages = other.m_pages;
    m_pageZero = other.m_pageZero;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_isPlatformFont = other.m_isPlatformFont;
    return *this;
}

Font::~Font()
{
}

// FIXME: It is unfortunate that this function needs to be passed the original cluster.
// It is only required for the platform's FontCache::getFontDataForCharacters(), and it means
// that this function is not correct if it transforms the character to uppercase and calls
// FontCache::getFontDataForCharacters() afterwards.
const GlyphData& Font::glyphDataForCharacter(UChar32 c, const UChar* cluster, unsigned clusterLength, bool mirror, bool attemptFontSubstitution) const
{
    bool smallCaps = false;

    if (m_fontDescription.smallCaps() && !Unicode::isUpper(c)) {
        // Convert lowercase to upper.
        UChar32 upperC = Unicode::toUpper(c);
        if (upperC != c) {
            c = upperC;
            smallCaps = true;
        }
    }

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNode* node = pageNumber ? m_pages.get(pageNumber) : m_pageZero;
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(primaryFont(), pageNumber);
        if (pageNumber)
            m_pages.set(pageNumber, node);
        else
            m_pageZero = node;
    }

    if (!attemptFontSubstitution && node->level() != 1)
        node = GlyphPageTreeNode::getRootChild(primaryFont(), pageNumber);

    while (true) {
        GlyphPage* page = node->page();

        if (page) {
            const GlyphData& data = page->glyphDataForCharacter(c);
            if (data.glyph || !attemptFontSubstitution) {
                if (!smallCaps)
                    return data;  // We have a glyph for the character in question in the current page (or we've been told not to fall back).

                const FontData* smallCapsFontData = data.fontData->smallCapsFontData(m_fontDescription);

                if (!smallCapsFontData)
                    // This should not happen, but if it does, we will return a big cap.
                    return data;

                GlyphPageTreeNode* smallCapsNode = GlyphPageTreeNode::getRootChild(smallCapsFontData, pageNumber);
                GlyphPage* smallCapsPage = smallCapsNode->page();

                if (smallCapsPage) {
                    const GlyphData& data = smallCapsPage->glyphDataForCharacter(c);
                    if (data.glyph || !attemptFontSubstitution)
                        return data;
                }
                // Not attempting system fallback off the smallCapsFontData. This is the very unlikely case that
                // a font has the lowercase character but not its uppercase version.
                return smallCapsFontData->missingGlyphData();
            }
        } else if (!attemptFontSubstitution) {
            if (smallCaps) {
                if (const FontData* smallCapsFontData = primaryFont()->smallCapsFontData(m_fontDescription))
                    return smallCapsFontData->missingGlyphData();
            }
            return primaryFont()->missingGlyphData();
        }

        if (node->isSystemFallback()) {
            // System fallback is character-dependent. When we get here, we
            // know that the character in question isn't in the system fallback
            // font's glyph page. Try to lazily create it here.

            // Convert characters that shouldn't render to zero width spaces when asking what font is
            // appropriate.
            const FontData* characterFontData;
            if (clusterLength == 1 && Font::treatAsZeroWidthSpace(cluster[0]))
                characterFontData = FontCache::getFontDataForCharacters(*this, &zeroWidthSpace, 1);
            else
                characterFontData = FontCache::getFontDataForCharacters(*this, cluster, clusterLength);
            if (smallCaps)
                characterFontData = characterFontData->smallCapsFontData(m_fontDescription);
            if (characterFontData) {
                // Got the fallback font, return the glyph page associated with
                // it. We also store the FontData for the glyph in the fallback
                // page for future use (it's lazily populated by us).
                GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData, pageNumber)->page();
                const GlyphData& data = fallbackPage ? fallbackPage->glyphDataForCharacter(c) : characterFontData->missingGlyphData();
                if (!smallCaps)
                    page->setGlyphDataForCharacter(c, data.glyph, characterFontData);
                return data;
            }
            // Even system fallback can fail.
            // FIXME: Should the last resort font be used?
            const GlyphData& data = primaryFont()->missingGlyphData();
            if (!smallCaps && page)
                page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
            return data;
        }

        // Proceed with the fallback list.
        const FontData* fontData = fontDataAt(node->level());
        node = node->getChild(fontData, pageNumber);

        if (pageNumber)
            m_pages.set(pageNumber, node);
        else
            m_pageZero = node;
    }
}

const FontData* Font::primaryFont() const
{
    ASSERT(m_fontList);
    return m_fontList->primaryFont(this);
}

const FontData* Font::fontDataAt(unsigned index) const
{
    ASSERT(m_fontList);
    return m_fontList->fontDataAt(this, index);
}

const FontData* Font::fontDataForCharacters(const UChar* characters, int length) const
{
    ASSERT(m_fontList);
    return m_fontList->fontDataForCharacters(this, characters, length);
}

void Font::update() const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontList)
        m_fontList = new FontFallbackList;
    m_fontList->invalidate();
    m_pageZero = 0;
    m_pages.clear();
}

int Font::width(const TextRun& run) const
{
    return width(run, TextStyle());
}

int Font::width(const TextRun& run, const TextStyle& style) const
{
    return lroundf(floatWidth(run, style));
}

int Font::ascent() const
{
    return primaryFont()->ascent();
}

int Font::descent() const
{
    return primaryFont()->descent();
}

int Font::lineSpacing() const
{
    return primaryFont()->lineSpacing();
}

float Font::xHeight() const
{
    return primaryFont()->xHeight();
}

int Font::spaceWidth() const
{
    return (int)ceilf(primaryFont()->m_adjustedSpaceWidth + m_letterSpacing);
}

bool Font::isFixedPitch() const
{
    ASSERT(m_fontList);
    return m_fontList->isFixedPitch(this);
}

void Font::setCodePath(CodePath p)
{
    codePath = p;
}

bool Font::canUseGlyphCache(const TextRun& run) const
{
    switch (codePath) {
        case Auto:
            break;
        case Simple:
            return true;
        case Complex:
            return false;
    }
    
    // Start from 0 since drawing and highlighting also measure the characters before run->from
    for (int i = 0; i < run.length(); i++) {
        const UChar c = run[i];
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

void Font::drawSimpleText(GraphicsContext* context, const TextRun& run, const TextStyle& style, const FloatPoint& point, int from, int to) const
{
    // This glyph buffer holds our glyphs+advances+font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x();
    WidthIterator it(this, run, style);
    it.advance(from);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    float afterWidth = it.m_runWidthSoFar;

    if (style.rtl()) {
        float finalRoundingWidth = it.m_finalRoundingWidth;
        it.advance(run.length());
        startX += finalRoundingWidth + it.m_runWidthSoFar - afterWidth;
    } else
        startX += beforeWidth;

    // Swap the order of the glyphs if right-to-left.
    if (style.rtl())
        for (int i = 0, end = glyphBuffer.size() - 1; i < glyphBuffer.size() / 2; ++i, --end)
            glyphBuffer.swap(i, end);

    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, run, style, startPoint);
}

void Font::drawGlyphBuffer(GraphicsContext* context, const GlyphBuffer& glyphBuffer, 
                           const TextRun& run, const TextStyle& style, const FloatPoint& point) const
{   
    // Draw each contiguous run of glyphs that use the same font data.
    const FontData* fontData = glyphBuffer.fontDataAt(0);
    FloatSize offset = glyphBuffer.offsetAt(0);
    FloatPoint startPoint(point);
    float nextX = startPoint.x();
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < glyphBuffer.size()) {
        const FontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);
        FloatSize nextOffset = glyphBuffer.offsetAt(nextGlyph);
        if (nextFontData != fontData || nextOffset != offset) {
            drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
            lastFrom = nextGlyph;
            fontData = nextFontData;
            offset = nextOffset;
            startPoint.setX(nextX);
        }
        nextX += glyphBuffer.advanceAt(nextGlyph);
        nextGlyph++;
    }
    drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
}

void Font::drawText(GraphicsContext* context, const TextRun& run, const TextStyle& style, const FloatPoint& point, int from, int to) const
{
    to = (to == -1 ? run.length() : to);
    if (canUseGlyphCache(run))
        drawSimpleText(context, run, style, point, from, to);
    else
        drawComplexText(context, run, style, point, from, to);
}

float Font::floatWidth(const TextRun& run, const TextStyle& style) const
{
    if (canUseGlyphCache(run))
        return floatWidthForSimpleText(run, style, 0);
    return floatWidthForComplexText(run, style);
}

float Font::floatWidthForSimpleText(const TextRun& run, const TextStyle& style, GlyphBuffer* glyphBuffer) const
{
    WidthIterator it(this, run, style);
    it.advance(run.length(), glyphBuffer);
    return it.m_runWidthSoFar;
}

FloatRect Font::selectionRectForText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h, int from, int to) const
{
    to = (to == -1 ? run.length() : to);
    if (canUseGlyphCache(run))
        return selectionRectForSimpleText(run, style, point, h, from, to);
    return selectionRectForComplexText(run, style, point, h, from, to);
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h, int from, int to) const
{
    WidthIterator it(this, run, style);
    it.advance(from);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to);
    float afterWidth = it.m_runWidthSoFar;

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (style.rtl()) {
        it.advance(run.length());
        float totalWidth = it.m_runWidthSoFar;
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
    } else {
        return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
    }
}

int Font::offsetForPosition(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    if (canUseGlyphCache(run))
        return offsetForPositionForSimpleText(run, style, x, includePartialGlyphs);
    return offsetForPositionForComplexText(run, style, x, includePartialGlyphs);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    float delta = (float)x;

    WidthIterator it(this, run, style);
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (style.rtl()) {
        delta -= floatWidthForSimpleText(run, style, 0);
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, &localGlyphBuffer))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, &localGlyphBuffer))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset;
}

}
