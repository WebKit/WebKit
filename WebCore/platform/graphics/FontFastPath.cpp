/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Torch Mobile, Inc.
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
#include "GlyphBuffer.h"
#include "GlyphPageTreeNode.h"
#include "IntPoint.h"
#include "SimpleFontData.h"
#include "WidthIterator.h"

#include <wtf/MathExtras.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

GlyphData Font::glyphDataForCharacter(UChar32 c, bool mirror, bool forceSmallCaps) const
{
    ASSERT(isMainThread());

    bool useSmallCapsFont = forceSmallCaps;
    if (m_fontDescription.smallCaps()) {
        UChar32 upperC = toUpper(c);
        if (upperC != c) {
            c = upperC;
            useSmallCapsFont = true;
        }
    }

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNode* node = pageNumber ? m_fontList->m_pages.get(pageNumber) : m_fontList->m_pageZero;
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(fontDataAt(0), pageNumber);
        if (pageNumber)
            m_fontList->m_pages.set(pageNumber, node);
        else
            m_fontList->m_pageZero = node;
    }

    GlyphPage* page;
    if (!useSmallCapsFont) {
        // Fastest loop, for the common case (not small caps).
        while (true) {
            page = node->page();
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData)
                    return data;
                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = node->getChild(fontDataAt(node->level()), pageNumber);
            if (pageNumber)
                m_fontList->m_pages.set(pageNumber, node);
            else
                m_fontList->m_pageZero = node;
        }
    } else {
        while (true) {
            page = node->page();
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    // The smallCapsFontData function should not normally return 0.
                    // But if it does, we will just render the capital letter big.
                    const SimpleFontData* smallCapsFontData = data.fontData->smallCapsFontData(m_fontDescription);
                    if (!smallCapsFontData)
                        return data;

                    GlyphPageTreeNode* smallCapsNode = GlyphPageTreeNode::getRootChild(smallCapsFontData, pageNumber);
                    const GlyphPage* smallCapsPage = smallCapsNode->page();
                    if (smallCapsPage) {
                        GlyphData data = smallCapsPage->glyphDataForCharacter(c);
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
                m_fontList->m_pages.set(pageNumber, node);
            else
                m_fontList->m_pageZero = node;
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
        codeUnits[0] = Font::normalizeSpaces(c);
        codeUnitsLength = 1;
    } else {
        codeUnits[0] = U16_LEAD(c);
        codeUnits[1] = U16_TRAIL(c);
        codeUnitsLength = 2;
    }
    const SimpleFontData* characterFontData = fontCache()->getFontDataForCharacters(*this, codeUnits, codeUnitsLength);
    if (useSmallCapsFont && characterFontData)
        characterFontData = characterFontData->smallCapsFontData(m_fontDescription);
    if (characterFontData) {
        // Got the fallback glyph and font.
        GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData, pageNumber)->page();
        GlyphData data = fallbackPage && fallbackPage->fontDataForCharacter(c) ? fallbackPage->glyphDataForCharacter(c) : characterFontData->missingGlyphData();
        // Cache it so we don't have to do system fallback again next time.
        if (!useSmallCapsFont) {
#if OS(WINCE)
            // missingGlyphData returns a null character, which is not suitable for GDI to display.
            // Also, sometimes we cannot map a font for the character on WINCE, but GDI can still
            // display the character, probably because the font package is not installed correctly.
            // So we just always set the glyph to be same as the character, and let GDI solve it.
            page->setGlyphDataForCharacter(c, c, characterFontData);
            return page->glyphDataForCharacter(c);
#else
            page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
#endif
        }
        return data;
    }

    // Even system fallback can fail; use the missing glyph in that case.
    // FIXME: It would be nicer to use the missing glyph from the last resort font instead.
    GlyphData data = primaryFont()->missingGlyphData();
    if (!useSmallCapsFont) {
#if OS(WINCE)
        // See comment about WINCE GDI handling near setGlyphDataForCharacter above.
        page->setGlyphDataForCharacter(c, c, data.fontData);
        return page->glyphDataForCharacter(c);
#else
        page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
#endif
    }
    return data;
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

void Font::drawGlyphBuffer(GraphicsContext* context, const GlyphBuffer& glyphBuffer, const TextRun&, const FloatPoint& point) const
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

float Font::floatWidthForSimpleText(const TextRun& run, GlyphBuffer* glyphBuffer, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    WidthIterator it(this, run, fallbackFonts, glyphOverflow);
    it.advance(run.length(), glyphBuffer);

    if (glyphOverflow) {
        glyphOverflow->top = max<int>(glyphOverflow->top, ceilf(-it.minGlyphBoundingBoxY()) - ascent());
        glyphOverflow->bottom = max<int>(glyphOverflow->bottom, ceilf(it.maxGlyphBoundingBoxY()) - descent());
        glyphOverflow->left = ceilf(it.firstGlyphOverflow());
        glyphOverflow->right = ceilf(it.lastGlyphOverflow());
    }

    return it.m_runWidthSoFar;
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

}
