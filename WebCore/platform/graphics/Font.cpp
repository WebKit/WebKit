/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Inc. All rights reserved.
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
#include "WidthIterator.h"
#include <wtf/MathExtras.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

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

Font::CodePath Font::s_codePath = Auto;

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font()
    : m_pageZero(0)
    , m_cachedPrimaryFont(0)
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_isPlatformFont(false)
{
}

Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
    : m_fontDescription(fd)
    , m_pageZero(0)
    , m_cachedPrimaryFont(0)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_isPlatformFont(false)
{
}

Font::Font(const FontPlatformData& fontData, bool isPrinterFont)
    : m_fontList(FontFallbackList::create())
    , m_pageZero(0)
    , m_cachedPrimaryFont(0)
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
    , m_cachedPrimaryFont(other.m_cachedPrimaryFont)
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
    m_cachedPrimaryFont = other.m_cachedPrimaryFont;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_isPlatformFont = other.m_isPlatformFont;
    return *this;
}

Font::~Font()
{
}

bool Font::operator==(const Font& other) const
{
    // Our FontData don't have to be checked, since checking the font description will be fine.
    // FIXME: This does not work if the font was made with the FontPlatformData constructor.
    if ((m_fontList && m_fontList->loadingCustomFonts()) ||
        (other.m_fontList && other.m_fontList->loadingCustomFonts()))
        return false;
    
    FontSelector* first = m_fontList ? m_fontList->fontSelector() : 0;
    FontSelector* second = other.m_fontList ? other.m_fontList->fontSelector() : 0;
    
    return first == second
           && m_fontDescription == other.m_fontDescription
           && m_letterSpacing == other.m_letterSpacing
           && m_wordSpacing == other.m_wordSpacing
           && (m_fontList ? m_fontList->generation() : 0) == (other.m_fontList ? other.m_fontList->generation() : 0);
}

const GlyphData& Font::glyphDataForCharacter(UChar32 c, bool mirror, bool forceSmallCaps) const
{
    bool useSmallCapsFont = forceSmallCaps;
    if (m_fontDescription.smallCaps()) {
        UChar32 upperC = Unicode::toUpper(c);
        if (upperC != c) {
            c = upperC;
            useSmallCapsFont = true;
        }
    }

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNode* node = pageNumber ? m_pages.get(pageNumber) : m_pageZero;
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(fontDataAt(0), pageNumber);
        if (pageNumber)
            m_pages.set(pageNumber, node);
        else
            m_pageZero = node;
    }

    GlyphPage* page;
    if (!useSmallCapsFont) {
        // Fastest loop, for the common case (not small caps).
        while (true) {
            page = node->page();
            if (page) {
                const GlyphData& data = page->glyphDataForCharacter(c);
                if (data.fontData)
                    return data;
                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = node->getChild(fontDataAt(node->level()), pageNumber);
            if (pageNumber)
                m_pages.set(pageNumber, node);
            else
                m_pageZero = node;
        }
    } else {
        while (true) {
            page = node->page();
            if (page) {
                const GlyphData& data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    // The smallCapsFontData function should not normally return 0.
                    // But if it does, we will just render the capital letter big.
                    const SimpleFontData* smallCapsFontData = data.fontData->smallCapsFontData(m_fontDescription);
                    if (!smallCapsFontData)
                        return data;

                    GlyphPageTreeNode* smallCapsNode = GlyphPageTreeNode::getRootChild(smallCapsFontData, pageNumber);
                    const GlyphPage* smallCapsPage = smallCapsNode->page();
                    if (smallCapsPage) {
                        const GlyphData& data = smallCapsPage->glyphDataForCharacter(c);
                        if (data.fontData)
                            return data;
                    }

                    // Do not attempt system fallback off the smallCapsFontData. This is the very unlikely case that
                    // a font has the lowercase character but the small caps font does not have its uppercase version.
                    return smallCapsFontData->missingGlyphData();
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = node->getChild(fontDataAt(node->level()), pageNumber);
            if (pageNumber)
                m_pages.set(pageNumber, node);
            else
                m_pageZero = node;
        }
    }

    ASSERT(page);
    ASSERT(node->isSystemFallback());

    // System fallback is character-dependent. When we get here, we
    // know that the character in question isn't in the system fallback
    // font's glyph page. Try to lazily create it here.
    UChar codeUnits[2];
    int codeUnitsLength;
    if (c <= 0xFFFF) {
        UChar c16 = c;
        if (Font::treatAsSpace(c16))
            codeUnits[0] = ' ';
        else if (Font::treatAsZeroWidthSpace(c16))
            codeUnits[0] = zeroWidthSpace;
        else
            codeUnits[0] = c16;
        codeUnitsLength = 1;
    } else {
        codeUnits[0] = U16_LEAD(c);
        codeUnits[1] = U16_TRAIL(c);
        codeUnitsLength = 2;
    }
    const SimpleFontData* characterFontData = FontCache::getFontDataForCharacters(*this, codeUnits, codeUnitsLength);
    if (useSmallCapsFont && characterFontData)
        characterFontData = characterFontData->smallCapsFontData(m_fontDescription);
    if (characterFontData) {
        // Got the fallback glyph and font.
        GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData, pageNumber)->page();
        const GlyphData& data = fallbackPage && fallbackPage->glyphDataForCharacter(c).fontData ? fallbackPage->glyphDataForCharacter(c) : characterFontData->missingGlyphData();
        // Cache it so we don't have to do system fallback again next time.
        if (!useSmallCapsFont)
            page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
        return data;
    }

    // Even system fallback can fail; use the missing glyph in that case.
    // FIXME: It would be nicer to use the missing glyph from the last resort font instead.
    const GlyphData& data = primaryFont()->missingGlyphData();
    if (!useSmallCapsFont)
        page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
    return data;
}

void Font::cachePrimaryFont() const
{
    ASSERT(m_fontList);
    ASSERT(!m_cachedPrimaryFont);
    m_cachedPrimaryFont = m_fontList->primaryFont(this)->fontDataForCharacter(' ');
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

void Font::update(PassRefPtr<FontSelector> fontSelector) const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontList)
        m_fontList = FontFallbackList::create();
    m_fontList->invalidate(fontSelector);
    m_cachedPrimaryFont = 0;
    m_pageZero = 0;
    m_pages.clear();
}

int Font::width(const TextRun& run) const
{
    return lroundf(floatWidth(run));
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

int Font::lineGap() const
{
    return primaryFont()->lineGap();
}

float Font::xHeight() const
{
    return primaryFont()->xHeight();
}

unsigned Font::unitsPerEm() const
{
    return primaryFont()->unitsPerEm();
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
    s_codePath = p;
}

Font::CodePath Font::codePath()
{
    return s_codePath;
}

bool Font::canUseGlyphCache(const TextRun& run) const
{
    switch (s_codePath) {
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

void Font::drawSimpleText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    // This glyph buffer holds our glyphs+advances+font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x();
    WidthIterator it(this, run);
    it.advance(from);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    float afterWidth = it.m_runWidthSoFar;

    if (run.rtl()) {
        float finalRoundingWidth = it.m_finalRoundingWidth;
        it.advance(run.length());
        startX += finalRoundingWidth + it.m_runWidthSoFar - afterWidth;
    } else
        startX += beforeWidth;

    // Swap the order of the glyphs if right-to-left.
    if (run.rtl())
        for (int i = 0, end = glyphBuffer.size() - 1; i < glyphBuffer.size() / 2; ++i, --end)
            glyphBuffer.swap(i, end);

    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, run, startPoint);
}

void Font::drawGlyphBuffer(GraphicsContext* context, const GlyphBuffer& glyphBuffer, 
                           const TextRun& run, const FloatPoint& point) const
{   
    // Draw each contiguous run of glyphs that use the same font data.
    const SimpleFontData* fontData = glyphBuffer.fontDataAt(0);
    FloatSize offset = glyphBuffer.offsetAt(0);
    FloatPoint startPoint(point);
    float nextX = startPoint.x();
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < glyphBuffer.size()) {
        const SimpleFontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);
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

void Font::drawText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading.
    if (m_fontList && m_fontList->loadingCustomFonts())
        return;
    
    to = (to == -1 ? run.length() : to);

#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont()) {
        drawTextUsingSVGFont(context, run, point, from, to);
        return;
    }
#endif

    if (canUseGlyphCache(run))
        drawSimpleText(context, run, point, from, to);
    else
        drawComplexText(context, run, point, from, to);
}

float Font::floatWidth(const TextRun& run) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return floatWidthUsingSVGFont(run);
#endif

    if (canUseGlyphCache(run))
        return floatWidthForSimpleText(run, 0);
    return floatWidthForComplexText(run);
}

float Font::floatWidth(const TextRun& run, int extraCharsAvailable, int& charsConsumed, String& glyphName) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return floatWidthUsingSVGFont(run, extraCharsAvailable, charsConsumed, glyphName);
#endif

    charsConsumed = run.length();
    glyphName = "";
    if (canUseGlyphCache(run))
        return floatWidthForSimpleText(run, 0);
    return floatWidthForComplexText(run);
}

float Font::floatWidthForSimpleText(const TextRun& run, GlyphBuffer* glyphBuffer) const
{
    WidthIterator it(this, run);
    it.advance(run.length(), glyphBuffer);
    return it.m_runWidthSoFar;
}

FloatRect Font::selectionRectForText(const TextRun& run, const IntPoint& point, int h, int from, int to) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return selectionRectForTextUsingSVGFont(run, point, h, from, to);
#endif

    to = (to == -1 ? run.length() : to);
    if (canUseGlyphCache(run))
        return selectionRectForSimpleText(run, point, h, from, to);
    return selectionRectForComplexText(run, point, h, from, to);
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const IntPoint& point, int h, int from, int to) const
{
    WidthIterator it(this, run);
    it.advance(from);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to);
    float afterWidth = it.m_runWidthSoFar;

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (run.rtl()) {
        it.advance(run.length());
        float totalWidth = it.m_runWidthSoFar;
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
    } else {
        return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
    }
}

int Font::offsetForPosition(const TextRun& run, int x, bool includePartialGlyphs) const
{
#if ENABLE(SVG_FONTS)
    if (primaryFont()->isSVGFont())
        return offsetForPositionForTextUsingSVGFont(run, x, includePartialGlyphs);
#endif

    if (canUseGlyphCache(run))
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);
    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, int x, bool includePartialGlyphs) const
{
    float delta = (float)x;

    WidthIterator it(this, run);
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (run.rtl()) {
        delta -= floatWidthForSimpleText(run, 0);
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

#if ENABLE(SVG_FONTS)
bool Font::isSVGFont() const
{ 
    return primaryFont()->isSVGFont(); 
}
#endif

FontSelector* Font::fontSelector() const
{
    return m_fontList ? m_fontList->fontSelector() : 0;
}

}
