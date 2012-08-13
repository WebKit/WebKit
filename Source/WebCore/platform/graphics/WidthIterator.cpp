/*
 * Copyright (C) 2003, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "WidthIterator.h"

#include "Font.h"
#include "GlyphBuffer.h"
#include "SimpleFontData.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include <wtf/MathExtras.h>

using namespace WTF;
using namespace Unicode;
using namespace std;

namespace WebCore {

WidthIterator::WidthIterator(const Font* font, const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, bool accountForGlyphBounds, bool forTextEmphasis)
    : m_font(font)
    , m_run(run)
    , m_currentCharacter(0)
    , m_runWidthSoFar(0)
    , m_isAfterExpansion(!run.allowsLeadingExpansion())
    , m_finalRoundingWidth(0)
    , m_fallbackFonts(fallbackFonts)
    , m_accountForGlyphBounds(accountForGlyphBounds)
    , m_maxGlyphBoundingBoxY(numeric_limits<float>::min())
    , m_minGlyphBoundingBoxY(numeric_limits<float>::max())
    , m_firstGlyphOverflow(0)
    , m_lastGlyphOverflow(0)
    , m_forTextEmphasis(forTextEmphasis)
{
    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    m_expansion = m_run.expansion();
    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        bool isAfterExpansion = m_isAfterExpansion;
        unsigned expansionOpportunityCount = Font::expansionOpportunityCount(m_run.characters(), m_run.length(), m_run.ltr() ? LTR : RTL, isAfterExpansion);
        if (isAfterExpansion && !m_run.allowsTrailingExpansion())
            expansionOpportunityCount--;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }
}

GlyphData WidthIterator::glyphDataForCharacter(UChar32 character, bool mirror, int currentCharacter, unsigned& advanceLength)
{
    ASSERT(m_font);

#if ENABLE(SVG_FONTS)
    if (TextRun::RenderingContext* renderingContext = m_run.renderingContext())
        return renderingContext->glyphDataForCharacter(*m_font, m_run, *this, character, mirror, currentCharacter, advanceLength);
#else
    UNUSED_PARAM(currentCharacter);
    UNUSED_PARAM(advanceLength);
#endif

    return m_font->glyphDataForCharacter(character, mirror);
}

unsigned WidthIterator::advance(int offset, GlyphBuffer* glyphBuffer)
{
    if (offset > m_run.length())
        offset = m_run.length();

    if (int(m_currentCharacter) >= offset)
        return 0;

    bool rtl = m_run.rtl();
    bool hasExtraSpacing = (m_font->letterSpacing() || m_font->wordSpacing() || m_expansion) && !m_run.spacingDisabled();

    float widthSinceLastRounding = m_runWidthSoFar;
    m_runWidthSoFar = floorf(m_runWidthSoFar);
    widthSinceLastRounding -= m_runWidthSoFar;

    float lastRoundingWidth = m_finalRoundingWidth;
    FloatRect bounds;

    const SimpleFontData* primaryFont = m_font->primaryFont();
    const SimpleFontData* lastFontData = primaryFont;

    UChar32 character = 0;
    unsigned clusterLength = 0;
    SurrogatePairAwareTextIterator textIterator(m_run.data(m_currentCharacter), m_currentCharacter, offset, m_run.length());
    while (textIterator.consume(character, clusterLength)) {
        unsigned advanceLength = clusterLength;
        const GlyphData& glyphData = glyphDataForCharacter(character, rtl, textIterator.currentCharacter(), advanceLength);
        Glyph glyph = glyphData.glyph;
        const SimpleFontData* fontData = glyphData.fontData;

        ASSERT(fontData);

        // Now that we have a glyph and font data, get its width.
        float width;
        if (character == '\t' && m_run.allowTabs())
            width = m_font->tabWidth(*fontData, m_run.tabSize(), m_run.xPos() + m_runWidthSoFar + widthSinceLastRounding);
        else {
            width = fontData->widthForGlyph(glyph);

#if ENABLE(SVG)
            // SVG uses horizontalGlyphStretch(), when textLength is used to stretch/squeeze text.
            width *= m_run.horizontalGlyphStretch();
#endif

            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (m_run.applyWordRounding() && width == fontData->spaceWidth() && (fontData->pitch() == FixedPitch || glyph == fontData->spaceGlyph()))
                width = fontData->adjustedSpaceWidth();
        }

        if (fontData != lastFontData && width) {
            lastFontData = fontData;
            if (m_fallbackFonts && fontData != primaryFont) {
                // FIXME: This does a little extra work that could be avoided if
                // glyphDataForCharacter() returned whether it chose to use a small caps font.
                if (!m_font->isSmallCaps() || character == toUpper(character))
                    m_fallbackFonts->add(fontData);
                else {
                    const GlyphData& uppercaseGlyphData = m_font->glyphDataForCharacter(toUpper(character), rtl);
                    if (uppercaseGlyphData.fontData != primaryFont)
                        m_fallbackFonts->add(uppercaseGlyphData.fontData);
                }
            }
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && m_font->letterSpacing())
                width += m_font->letterSpacing();

            static bool expandAroundIdeographs = Font::canExpandAroundIdeographsInComplexText();
            bool treatAsSpace = Font::treatAsSpace(character);
            if (treatAsSpace || (expandAroundIdeographs && Font::isCJKIdeographOrSymbol(character))) {
                // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                if (m_expansion) {
                    float previousExpansion = m_expansion;
                    if (!treatAsSpace && !m_isAfterExpansion) {
                        // Take the expansion opportunity before this ideograph.
                        m_expansion -= m_expansionPerOpportunity;
                        float expansionAtThisOpportunity = !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                        m_runWidthSoFar += expansionAtThisOpportunity;
                        if (glyphBuffer) {
                            if (glyphBuffer->isEmpty())
                                glyphBuffer->add(fontData->spaceGlyph(), fontData, expansionAtThisOpportunity);
                            else
                                glyphBuffer->expandLastAdvance(expansionAtThisOpportunity);
                        }
                        previousExpansion = m_expansion;
                    }
                    if (m_run.allowsTrailingExpansion() || (m_run.ltr() && textIterator.currentCharacter() + advanceLength < static_cast<size_t>(m_run.length()))
                        || (m_run.rtl() && textIterator.currentCharacter())) {
                        m_expansion -= m_expansionPerOpportunity;
                        width += !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                        m_isAfterExpansion = true;
                    }
                } else
                    m_isAfterExpansion = false;

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                // Word-spacing affects each space (U+0020) and non-breaking space (U+00A0).
                if ((character == noBreakSpace || character == ' ') && textIterator.currentCharacter() && m_font->wordSpacing())
                    width += m_font->wordSpacing();
            } else
                m_isAfterExpansion = false;
        }

        if (m_accountForGlyphBounds) {
            bounds = fontData->boundsForGlyph(glyph);
            if (!textIterator.currentCharacter())
                m_firstGlyphOverflow = max<float>(0, -bounds.x());
        }

        if (m_forTextEmphasis && !Font::canReceiveTextEmphasis(character))
            glyph = 0;

        // Advance past the character we just dealt with.
        textIterator.advance(advanceLength);

        float oldWidth = width; 

        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so following words will start on an integer boundary.
        if (m_run.applyWordRounding() && Font::isRoundingHackCharacter(character)) {
            width = ceilf(width);

            // Since widthSinceLastRounding can lose precision if we include measurements for
            // preceding whitespace, we bypass it here.
            m_runWidthSoFar += width;

            // Since this is a rounding hack character, we should have reset this sum on the previous
            // iteration.
            ASSERT(!widthSinceLastRounding);
        } else {
            // Check to see if the next character is a "rounding hack character", if so, adjust
            // width so that the total run width will be on an integer boundary.
            if ((m_run.applyWordRounding() && textIterator.currentCharacter() < m_run.length() && Font::isRoundingHackCharacter(*(textIterator.characters())))
                    || (m_run.applyRunRounding() && textIterator.currentCharacter() >= m_run.length())) {
                float totalWidth = widthSinceLastRounding + width;
                widthSinceLastRounding = ceilf(totalWidth);
                width += widthSinceLastRounding - totalWidth;
                m_runWidthSoFar += widthSinceLastRounding;
                widthSinceLastRounding = 0;
            } else
                widthSinceLastRounding += width;
        }

        if (glyphBuffer)
            glyphBuffer->add(glyph, fontData, (rtl ? oldWidth + lastRoundingWidth : width));

        lastRoundingWidth = width - oldWidth;

        if (m_accountForGlyphBounds) {
            m_maxGlyphBoundingBoxY = max(m_maxGlyphBoundingBoxY, bounds.maxY());
            m_minGlyphBoundingBoxY = min(m_minGlyphBoundingBoxY, bounds.y());
            m_lastGlyphOverflow = max<float>(0, bounds.maxX() - width);
        }
    }

    unsigned consumedCharacters = textIterator.currentCharacter() - m_currentCharacter;
    m_currentCharacter = textIterator.currentCharacter();
    m_runWidthSoFar += widthSinceLastRounding;
    m_finalRoundingWidth = lastRoundingWidth;
    return consumedCharacters;
}

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer* glyphBuffer)
{
    int oldSize = glyphBuffer->size();
    advance(m_currentCharacter + 1, glyphBuffer);
    float w = 0;
    for (int i = oldSize; i < glyphBuffer->size(); ++i)
        w += glyphBuffer->advanceAt(i);
    width = w;
    return glyphBuffer->size() > oldSize;
}

}
