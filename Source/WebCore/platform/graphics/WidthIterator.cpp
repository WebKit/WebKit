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

#include "CharacterProperties.h"
#include "Font.h"
#include "FontCascade.h"
#include "GlyphBuffer.h"
#include "Latin1TextIterator.h"
#include "SurrogatePairAwareTextIterator.h"
#include <wtf/MathExtras.h>


namespace WebCore {

using namespace WTF::Unicode;

WidthIterator::WidthIterator(const FontCascade* font, const TextRun& run, HashSet<const Font*>* fallbackFonts, bool accountForGlyphBounds, bool forTextEmphasis)
    : m_font(font)
    , m_run(run)
    , m_currentCharacter(0)
    , m_runWidthSoFar(0)
    , m_isAfterExpansion((run.expansionBehavior() & LeadingExpansionMask) == ForbidLeadingExpansion)
    , m_finalRoundingWidth(0)
    , m_fallbackFonts(fallbackFonts)
    , m_accountForGlyphBounds(accountForGlyphBounds)
    , m_enableKerning(font->enableKerning())
    , m_requiresShaping(font->requiresShaping())
    , m_forTextEmphasis(forTextEmphasis)
{
    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    m_expansion = m_run.expansion();
    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        unsigned expansionOpportunityCount = FontCascade::expansionOpportunityCount(m_run.text(), m_run.ltr() ? TextDirection::LTR : TextDirection::RTL, run.expansionBehavior()).first;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }
}

struct OriginalAdvancesForCharacterTreatedAsSpace {
public:
    explicit OriginalAdvancesForCharacterTreatedAsSpace(bool isSpace, float advanceBefore, float advanceAt)
        : characterIsSpace(isSpace)
        , advanceBeforeCharacter(advanceBefore)
        , advanceAtCharacter(advanceAt)
    {
    }

    bool characterIsSpace;
    float advanceBeforeCharacter;
    float advanceAtCharacter;
};

static inline bool isSoftBankEmoji(UChar32 codepoint)
{
    return codepoint >= 0xE001 && codepoint <= 0xE537;
}

inline auto WidthIterator::shouldApplyFontTransforms(const GlyphBuffer* glyphBuffer, unsigned lastGlyphCount, UChar32 previousCharacter) const -> TransformsType
{
    if (glyphBuffer && glyphBuffer->size() == (lastGlyphCount + 1) && isSoftBankEmoji(previousCharacter))
        return TransformsType::Forced;
    if (m_run.length() <= 1 || !(m_enableKerning || m_requiresShaping))
        return TransformsType::None;
    return TransformsType::NotForced;
}

inline float WidthIterator::applyFontTransforms(GlyphBuffer* glyphBuffer, bool ltr, unsigned& lastGlyphCount, const Font* font, UChar32 previousCharacter, bool force, CharactersTreatedAsSpace& charactersTreatedAsSpace)
{
    ASSERT_UNUSED(previousCharacter, shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, previousCharacter) != WidthIterator::TransformsType::None);

    if (!glyphBuffer)
        return 0;

    unsigned glyphBufferSize = glyphBuffer->size();
    if (!force && glyphBufferSize <= lastGlyphCount + 1) {
        lastGlyphCount = glyphBufferSize;
        return 0;
    }

    GlyphBufferAdvance* advances = glyphBuffer->advances(0);
    float widthDifference = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        widthDifference -= advances[i].width();

    ASSERT(lastGlyphCount <= glyphBufferSize);
    if (!ltr)
        glyphBuffer->reverse(lastGlyphCount, glyphBufferSize - lastGlyphCount);

    font->applyTransforms(glyphBuffer->glyphs(lastGlyphCount), advances + lastGlyphCount, glyphBufferSize - lastGlyphCount, m_enableKerning, m_requiresShaping);

    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        advances[i].setHeight(-advances[i].height());

    if (!ltr)
        glyphBuffer->reverse(lastGlyphCount, glyphBufferSize - lastGlyphCount);

    for (size_t i = 0; i < charactersTreatedAsSpace.size(); ++i) {
        int spaceOffset = charactersTreatedAsSpace[i].first;
        const OriginalAdvancesForCharacterTreatedAsSpace& originalAdvances = charactersTreatedAsSpace[i].second;
        if (spaceOffset && !originalAdvances.characterIsSpace)
            glyphBuffer->advances(spaceOffset - 1)->setWidth(originalAdvances.advanceBeforeCharacter);
        glyphBuffer->advances(spaceOffset)->setWidth(originalAdvances.advanceAtCharacter);
    }
    charactersTreatedAsSpace.clear();

    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        widthDifference += advances[i].width();

    lastGlyphCount = glyphBufferSize;
    return widthDifference;
}

static inline std::pair<bool, bool> expansionLocation(bool ideograph, bool treatAsSpace, bool ltr, bool isAfterExpansion, bool forbidLeadingExpansion, bool forbidTrailingExpansion, bool forceLeadingExpansion, bool forceTrailingExpansion)
{
    bool expandLeft = ideograph;
    bool expandRight = ideograph;
    if (treatAsSpace) {
        if (ltr)
            expandRight = true;
        else
            expandLeft = true;
    }
    if (isAfterExpansion) {
        if (ltr)
            expandLeft = false;
        else
            expandRight = false;
    }
    ASSERT(!forbidLeadingExpansion || !forceLeadingExpansion);
    ASSERT(!forbidTrailingExpansion || !forceTrailingExpansion);
    if (forbidLeadingExpansion)
        expandLeft = false;
    if (forbidTrailingExpansion)
        expandRight = false;
    if (forceLeadingExpansion)
        expandLeft = true;
    if (forceTrailingExpansion)
        expandRight = true;
    return std::make_pair(expandLeft, expandRight);
}

template <typename TextIterator>
inline unsigned WidthIterator::advanceInternal(TextIterator& textIterator, GlyphBuffer* glyphBuffer)
{
    // The core logic here needs to match SimpleLineLayout::widthForSimpleText()
    bool rtl = m_run.rtl();
    bool hasExtraSpacing = (m_font->letterSpacing() || m_font->wordSpacing() || m_expansion) && !m_run.spacingDisabled();

    bool runForcesLeadingExpansion = (m_run.expansionBehavior() & LeadingExpansionMask) == ForceLeadingExpansion;
    bool runForcesTrailingExpansion = (m_run.expansionBehavior() & TrailingExpansionMask) == ForceTrailingExpansion;
    bool runForbidsLeadingExpansion = (m_run.expansionBehavior() & LeadingExpansionMask) == ForbidLeadingExpansion;
    bool runForbidsTrailingExpansion = (m_run.expansionBehavior() & TrailingExpansionMask) == ForbidTrailingExpansion;
    float widthSinceLastRounding = m_runWidthSoFar;
    float leftoverJustificationWidth = 0;
    m_runWidthSoFar = floorf(m_runWidthSoFar);
    widthSinceLastRounding -= m_runWidthSoFar;

    float lastRoundingWidth = m_finalRoundingWidth;
    FloatRect bounds;

    const Font& primaryFont = m_font->primaryFont();
    const Font* lastFontData = &primaryFont;
    unsigned lastGlyphCount = glyphBuffer ? glyphBuffer->size() : 0;

    UChar32 character = 0;
    UChar32 previousCharacter = 0;
    unsigned clusterLength = 0;
    CharactersTreatedAsSpace charactersTreatedAsSpace;
    String normalizedSpacesStringCache;
    // We are iterating in string order, not glyph order. Compare this to ComplexTextController::adjustGlyphsAndAdvances()
    while (textIterator.consume(character, clusterLength)) {
        unsigned advanceLength = clusterLength;
        bool characterMustDrawSomething = !isDefaultIgnorableCodePoint(character);
#if USE(FREETYPE)
        // Freetype based ports only override the characters with Default_Ignorable unicode property when the font
        // doesn't support the code point. We should ignore them at this point to ensure they are not displayed.
        if (!characterMustDrawSomething) {
            textIterator.advance(advanceLength);
            continue;
        }
#endif
        int currentCharacter = textIterator.currentIndex();
        const GlyphData& glyphData = m_font->glyphDataForCharacter(character, rtl);
        Glyph glyph = glyphData.glyph;
        if (!glyph && !characterMustDrawSomething) {
            textIterator.advance(advanceLength);
            continue;
        }
        const Font* font = glyphData.font ? glyphData.font : &m_font->primaryFont();
        ASSERT(font);

        // Now that we have a glyph and font data, get its width.
        float width;
        if (character == '\t' && m_run.allowTabs())
            width = m_font->tabWidth(*font, m_run.tabSize(), m_run.xPos() + m_runWidthSoFar + widthSinceLastRounding);
        else {
            width = font->widthForGlyph(glyph);

            // SVG uses horizontalGlyphStretch(), when textLength is used to stretch/squeeze text.
            width *= m_run.horizontalGlyphStretch();
        }

        if (font != lastFontData && width) {
            auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, previousCharacter);
            if (transformsType != TransformsType::None) {
                m_runWidthSoFar += applyFontTransforms(glyphBuffer, m_run.ltr(), lastGlyphCount, lastFontData, previousCharacter, transformsType == TransformsType::Forced, charactersTreatedAsSpace);
                if (glyphBuffer)
                    glyphBuffer->shrink(lastGlyphCount);
            }

            lastFontData = font;
            if (m_fallbackFonts && font != &primaryFont) {
                // FIXME: This does a little extra work that could be avoided if
                // glyphDataForCharacter() returned whether it chose to use a small caps font.
                if (!m_font->isSmallCaps() || character == u_toupper(character))
                    m_fallbackFonts->add(font);
                else {
                    const GlyphData& uppercaseGlyphData = m_font->glyphDataForCharacter(u_toupper(character), rtl);
                    if (uppercaseGlyphData.font != &primaryFont)
                        m_fallbackFonts->add(uppercaseGlyphData.font);
                }
            }
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width) {
                width += m_font->letterSpacing();
                width += leftoverJustificationWidth;
                leftoverJustificationWidth = 0;
            }

            static bool expandAroundIdeographs = FontCascade::canExpandAroundIdeographsInComplexText();
            bool treatAsSpace = FontCascade::treatAsSpace(character);
            bool currentIsLastCharacter = currentCharacter + advanceLength == static_cast<size_t>(m_run.length());
            bool forceLeadingExpansion = false; // On the left, regardless of m_run.ltr()
            bool forceTrailingExpansion = false; // On the right, regardless of m_run.ltr()
            bool forbidLeadingExpansion = false;
            bool forbidTrailingExpansion = false;
            if (runForcesLeadingExpansion)
                forceLeadingExpansion = m_run.ltr() ? !currentCharacter : currentIsLastCharacter;
            if (runForcesTrailingExpansion)
                forceTrailingExpansion = m_run.ltr() ? currentIsLastCharacter : !currentCharacter;
            if (runForbidsLeadingExpansion)
                forbidLeadingExpansion = m_run.ltr() ? !currentCharacter : currentIsLastCharacter;
            if (runForbidsTrailingExpansion)
                forbidTrailingExpansion = m_run.ltr() ? currentIsLastCharacter : !currentCharacter;
            bool ideograph = (expandAroundIdeographs && FontCascade::isCJKIdeographOrSymbol(character));
            if (treatAsSpace || ideograph || forceLeadingExpansion || forceTrailingExpansion) {
                // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                if (m_expansion) {
                    auto [expandLeft, expandRight] = expansionLocation(ideograph, treatAsSpace, m_run.ltr(), m_isAfterExpansion, forbidLeadingExpansion, forbidTrailingExpansion, forceLeadingExpansion, forceTrailingExpansion);
                    if (expandLeft) {
                        if (m_run.ltr()) {
                            // Increase previous width
                            m_expansion -= m_expansionPerOpportunity;
                            m_runWidthSoFar += m_expansionPerOpportunity;
                            if (glyphBuffer) {
                                if (glyphBuffer->isEmpty()) {
                                    if (m_forTextEmphasis)
                                        glyphBuffer->add(font->zeroWidthSpaceGlyph(), font, m_expansionPerOpportunity, currentCharacter);
                                    else
                                        glyphBuffer->add(font->spaceGlyph(), font, m_expansionPerOpportunity, currentCharacter);
                                } else
                                    glyphBuffer->expandLastAdvance(m_expansionPerOpportunity);
                            }
                        } else {
                            // Increase next width
                            leftoverJustificationWidth += m_expansionPerOpportunity;
                            m_isAfterExpansion = true;
                        }
                    }
                    if (expandRight) {
                        m_expansion -= m_expansionPerOpportunity;
                        width += m_expansionPerOpportunity;
                        if (m_run.ltr())
                            m_isAfterExpansion = true;
                    }
                } else
                    m_isAfterExpansion = false;

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                if (treatAsSpace && (character != '\t' || !m_run.allowTabs()) && (currentCharacter || character == noBreakSpace) && m_font->wordSpacing())
                    width += m_font->wordSpacing();
            } else
                m_isAfterExpansion = false;
        }

        auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, previousCharacter);
        if (transformsType != TransformsType::None && glyphBuffer && FontCascade::treatAsSpace(character)) {
            charactersTreatedAsSpace.append(std::make_pair(glyphBuffer->size(),
                OriginalAdvancesForCharacterTreatedAsSpace(character == ' ', glyphBuffer->size() ? glyphBuffer->advanceAt(glyphBuffer->size() - 1).width() : 0, width)));
        }

        if (m_accountForGlyphBounds) {
            bounds = font->boundsForGlyph(glyph);
            if (!currentCharacter)
                m_firstGlyphOverflow = std::max<float>(0, -bounds.x());
        }

        if (m_forTextEmphasis && !FontCascade::canReceiveTextEmphasis(character))
            glyph = 0;

        // Advance past the character we just dealt with.
        textIterator.advance(advanceLength);

        float oldWidth = width;

        widthSinceLastRounding += width;

        if (glyphBuffer)
            glyphBuffer->add(glyph, font, (rtl ? oldWidth + lastRoundingWidth : width), currentCharacter);

        lastRoundingWidth = width - oldWidth;

        if (m_accountForGlyphBounds) {
            m_maxGlyphBoundingBoxY = std::max(m_maxGlyphBoundingBoxY, bounds.maxY());
            m_minGlyphBoundingBoxY = std::min(m_minGlyphBoundingBoxY, bounds.y());
            m_lastGlyphOverflow = std::max<float>(0, bounds.maxX() - width);
        }
        previousCharacter = character;
    }

    if (glyphBuffer && leftoverJustificationWidth) {
        if (m_forTextEmphasis)
            glyphBuffer->add(lastFontData->zeroWidthSpaceGlyph(), lastFontData, leftoverJustificationWidth, m_run.length() - 1);
        else
            glyphBuffer->add(lastFontData->spaceGlyph(), lastFontData, leftoverJustificationWidth, m_run.length() - 1);
    }

    auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, previousCharacter);
    if (transformsType != TransformsType::None) {
        m_runWidthSoFar += applyFontTransforms(glyphBuffer, m_run.ltr(), lastGlyphCount, lastFontData, previousCharacter, transformsType == TransformsType::Forced, charactersTreatedAsSpace);
        if (glyphBuffer)
            glyphBuffer->shrink(lastGlyphCount);
    }

    unsigned consumedCharacters = textIterator.currentIndex() - m_currentCharacter;
    m_currentCharacter = textIterator.currentIndex();
    m_runWidthSoFar += widthSinceLastRounding;
    m_finalRoundingWidth = lastRoundingWidth;
    return consumedCharacters;
}

unsigned WidthIterator::advance(unsigned offset, GlyphBuffer* glyphBuffer)
{
    unsigned length = m_run.length();

    if (offset > length)
        offset = length;

    if (m_currentCharacter >= offset)
        return 0;

    if (m_run.is8Bit()) {
        Latin1TextIterator textIterator(m_run.data8(m_currentCharacter), m_currentCharacter, offset, length);
        return advanceInternal(textIterator, glyphBuffer);
    }

    SurrogatePairAwareTextIterator textIterator(m_run.data16(m_currentCharacter), m_currentCharacter, offset, length);
    return advanceInternal(textIterator, glyphBuffer);
}

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer& glyphBuffer)
{
    unsigned oldSize = glyphBuffer.size();
    advance(m_currentCharacter + 1, &glyphBuffer);
    float w = 0;
    for (unsigned i = oldSize; i < glyphBuffer.size(); ++i)
        w += glyphBuffer.advanceAt(i).width();
    width = w;
    return glyphBuffer.size() > oldSize;
}

}
