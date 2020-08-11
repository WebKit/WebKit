/*
 * Copyright (C) 2003 - 2020 Apple Inc. All rights reserved.
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
#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace WTF::Unicode;

WidthIterator::WidthIterator(const FontCascade& font, const TextRun& run, HashSet<const Font*>* fallbackFonts, bool accountForGlyphBounds, bool forTextEmphasis)
    : m_font(font)
    , m_run(run)
    , m_fallbackFonts(fallbackFonts)
    , m_expansion(run.expansion())
    , m_isAfterExpansion((run.expansionBehavior() & LeftExpansionMask) == ForbidLeftExpansion)
    , m_accountForGlyphBounds(accountForGlyphBounds)
    , m_enableKerning(font.enableKerning())
    , m_requiresShaping(font.requiresShaping())
    , m_forTextEmphasis(forTextEmphasis)
{
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
    explicit OriginalAdvancesForCharacterTreatedAsSpace(GlyphBufferStringOffset stringOffset, bool isSpace, float advanceBefore, float advanceAt)
        : stringOffset(stringOffset)
        , characterIsSpace(isSpace)
        , advanceBeforeCharacter(advanceBefore)
        , advanceAtCharacter(advanceAt)
    {
    }

    GlyphBufferStringOffset stringOffset;
    bool characterIsSpace;
    float advanceBeforeCharacter;
    float advanceAtCharacter;
};

static inline bool isSoftBankEmoji(UChar32 codepoint)
{
    return codepoint >= 0xE001 && codepoint <= 0xE537;
}

inline auto WidthIterator::shouldApplyFontTransforms(const GlyphBuffer& glyphBuffer, unsigned lastGlyphCount, unsigned currentCharacterIndex) const -> TransformsType
{
    ASSERT(currentCharacterIndex <= m_run.length());
    if (glyphBuffer.size() == (lastGlyphCount + 1) && currentCharacterIndex && isSoftBankEmoji(m_run.text()[currentCharacterIndex - 1]))
        return TransformsType::Forced;
    if (m_run.length() <= 1)
        return TransformsType::None;
    return TransformsType::NotForced;
}

inline float WidthIterator::applyFontTransforms(GlyphBuffer& glyphBuffer, unsigned lastGlyphCount, unsigned currentCharacterIndex, const Font& font, bool force, CharactersTreatedAsSpace& charactersTreatedAsSpace)
{
    ASSERT_UNUSED(currentCharacterIndex, shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, currentCharacterIndex) != WidthIterator::TransformsType::None);

    auto glyphBufferSize = glyphBuffer.size();
    if (!force && glyphBufferSize <= lastGlyphCount + 1)
        return 0;

    GlyphBufferAdvance* advances = glyphBuffer.advances(0);
    float beforeWidth = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        beforeWidth += advances[i].width();

    ASSERT(lastGlyphCount <= glyphBufferSize);

    font.applyTransforms(glyphBuffer, lastGlyphCount, m_currentCharacterIndex, m_enableKerning, m_requiresShaping, m_font.fontDescription().computedLocale(), m_run.text(), m_run.direction());
    glyphBufferSize = glyphBuffer.size();

    GlyphBufferOrigin* origins = glyphBuffer.origins(0);
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i) {
        advances[i].setHeight(-advances[i].height());
        origins[i].setY(-origins[i].y());
    }

    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i) {
        auto characterIndex = glyphBuffer.stringOffsetAt(i);
        auto iterator = std::lower_bound(charactersTreatedAsSpace.begin(), charactersTreatedAsSpace.end(), characterIndex, [](const OriginalAdvancesForCharacterTreatedAsSpace& l, GlyphBufferStringOffset r) -> bool {
            return l.stringOffset < r;
        });
        if (iterator == charactersTreatedAsSpace.end() || iterator->stringOffset != characterIndex)
            continue;
        const auto& originalAdvances = *iterator;
        if (i && !originalAdvances.characterIsSpace)
            glyphBuffer.advances(i - 1)->setWidth(originalAdvances.advanceBeforeCharacter);
        glyphBuffer.advances(i)->setWidth(originalAdvances.advanceAtCharacter);
    }
    charactersTreatedAsSpace.clear();

    float afterWidth = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        afterWidth += advances[i].width();

    return afterWidth - beforeWidth;
}

static inline std::pair<bool, bool> expansionLocation(bool ideograph, bool treatAsSpace, bool ltr, bool isAfterExpansion, bool forbidLeftExpansion, bool forbidRightExpansion, bool forceLeftExpansion, bool forceRightExpansion)
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
    ASSERT(!forbidLeftExpansion || !forceLeftExpansion);
    ASSERT(!forbidRightExpansion || !forceRightExpansion);
    if (forbidLeftExpansion)
        expandLeft = false;
    if (forbidRightExpansion)
        expandRight = false;
    if (forceLeftExpansion)
        expandLeft = true;
    if (forceRightExpansion)
        expandRight = true;
    return std::make_pair(expandLeft, expandRight);
}

void WidthIterator::commitCurrentFontRange(GlyphBuffer& glyphBuffer, unsigned lastGlyphCount, unsigned currentCharacterIndex, const Font& font, const Font& primaryFont, UChar32 character, float widthOfCurrentFontRange, CharactersTreatedAsSpace& charactersTreatedAsSpace)
{
    auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, currentCharacterIndex);
    if (transformsType != TransformsType::None)
        m_runWidthSoFar += applyFontTransforms(glyphBuffer, lastGlyphCount, currentCharacterIndex, font, transformsType == TransformsType::Forced, charactersTreatedAsSpace);
    m_currentCharacterIndex = currentCharacterIndex;

    if (widthOfCurrentFontRange && m_fallbackFonts && &font != &primaryFont) {
        // FIXME: This does a little extra work that could be avoided if
        // glyphDataForCharacter() returned whether it chose to use a small caps font.
        if (!m_font.isSmallCaps() || character == u_toupper(character))
            m_fallbackFonts->add(&font);
        else {
            auto glyphFont = m_font.glyphDataForCharacter(u_toupper(character), m_run.rtl()).font;
            if (glyphFont != &primaryFont)
                m_fallbackFonts->add(glyphFont);
        }
    }
}

template <typename TextIterator>
inline void WidthIterator::advanceInternal(TextIterator& textIterator, GlyphBuffer& glyphBuffer)
{
    // The core logic here needs to match FontCascade::widthForSimpleText()
    bool rtl = m_run.rtl();
    bool hasExtraSpacing = (m_font.letterSpacing() || m_font.wordSpacing() || m_expansion) && !m_run.spacingDisabled();

    bool runForcesLeftExpansion = (m_run.expansionBehavior() & LeftExpansionMask) == ForceLeftExpansion;
    bool runForcesRightExpansion = (m_run.expansionBehavior() & RightExpansionMask) == ForceRightExpansion;
    bool runForbidsLeftExpansion = (m_run.expansionBehavior() & LeftExpansionMask) == ForbidLeftExpansion;
    bool runForbidsRightExpansion = (m_run.expansionBehavior() & RightExpansionMask) == ForbidRightExpansion;
    float leftoverJustificationWidth = 0;

    FloatRect bounds;

    const Font& primaryFont = m_font.primaryFont();
    const Font* lastFontData = &primaryFont;
    unsigned lastGlyphCount = glyphBuffer.size();

    auto currentCharacterIndex = textIterator.currentIndex();
    UChar32 character = 0;
    unsigned clusterLength = 0;
    CharactersTreatedAsSpace charactersTreatedAsSpace;
    float widthOfCurrentFontRange = 0;
    // We are iterating in string order, not glyph order. Compare this to ComplexTextController::adjustGlyphsAndAdvances()
    while (textIterator.consume(character, clusterLength)) {
        unsigned advanceLength = clusterLength;
        bool characterMustDrawSomething = !isDefaultIgnorableCodePoint(character);
#if USE(FREETYPE)
        // Freetype based ports only override the characters with Default_Ignorable unicode property when the font
        // doesn't support the code point. We should ignore them at this point to ensure they are not displayed.
        if (!characterMustDrawSomething) {
            textIterator.advance(advanceLength);
            currentCharacterIndex = textIterator.currentIndex();
            continue;
        }
#endif
        const GlyphData& glyphData = m_font.glyphDataForCharacter(character, rtl);
        Glyph glyph = glyphData.glyph;
        if (!glyph && !characterMustDrawSomething) {
            textIterator.advance(advanceLength);
            currentCharacterIndex = textIterator.currentIndex();
            continue;
        }
        const Font* font = glyphData.font ? glyphData.font : &m_font.primaryFont();
        ASSERT(font);

        // Now that we have a glyph and font data, get its width.
        float width;
        if (character == '\t' && m_run.allowTabs())
            width = m_font.tabWidth(*font, m_run.tabSize(), m_run.xPos() + m_runWidthSoFar);
        else {
            width = font->widthForGlyph(glyph);

            // SVG uses horizontalGlyphStretch(), when textLength is used to stretch/squeeze text.
            width *= m_run.horizontalGlyphStretch();
        }

        if (font != lastFontData) {
            commitCurrentFontRange(glyphBuffer, lastGlyphCount, currentCharacterIndex, *lastFontData, primaryFont, character, widthOfCurrentFontRange, charactersTreatedAsSpace);
            lastGlyphCount = glyphBuffer.size();
            lastFontData = font;
            widthOfCurrentFontRange = width;
        } else
            widthOfCurrentFontRange += width;

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width) {
                width += m_font.letterSpacing();
                width += leftoverJustificationWidth;
                leftoverJustificationWidth = 0;
            }

            static bool expandAroundIdeographs = FontCascade::canExpandAroundIdeographsInComplexText();
            bool treatAsSpace = FontCascade::treatAsSpace(character);
            bool currentIsLastCharacter = currentCharacterIndex + advanceLength == static_cast<size_t>(m_run.length());
            bool forceLeftExpansion = false; // On the left, regardless of m_run.ltr()
            bool forceRightExpansion = false; // On the right, regardless of m_run.ltr()
            bool forbidLeftExpansion = false;
            bool forbidRightExpansion = false;
            if (runForcesLeftExpansion)
                forceLeftExpansion = m_run.ltr() ? !currentCharacterIndex : currentIsLastCharacter;
            if (runForcesRightExpansion)
                forceRightExpansion = m_run.ltr() ? currentIsLastCharacter : !currentCharacterIndex;
            if (runForbidsLeftExpansion)
                forbidLeftExpansion = m_run.ltr() ? !currentCharacterIndex : currentIsLastCharacter;
            if (runForbidsRightExpansion)
                forbidRightExpansion = m_run.ltr() ? currentIsLastCharacter : !currentCharacterIndex;
            bool ideograph = (expandAroundIdeographs && FontCascade::isCJKIdeographOrSymbol(character));
            if (treatAsSpace || ideograph || forceLeftExpansion || forceRightExpansion) {
                // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                if (m_expansion) {
                    auto [expandLeft, expandRight] = expansionLocation(ideograph, treatAsSpace, m_run.ltr(), m_isAfterExpansion, forbidLeftExpansion, forbidRightExpansion, forceLeftExpansion, forceRightExpansion);
                    if (expandLeft) {
                        if (m_run.ltr()) {
                            // Increase previous width
                            m_expansion -= m_expansionPerOpportunity;
                            m_runWidthSoFar += m_expansionPerOpportunity;
                            if (glyphBuffer.isEmpty()) {
                                if (m_forTextEmphasis)
                                    glyphBuffer.add(font->zeroWidthSpaceGlyph(), *font, m_expansionPerOpportunity, currentCharacterIndex);
                                else
                                    glyphBuffer.add(font->spaceGlyph(), *font, m_expansionPerOpportunity, currentCharacterIndex);
                                lastGlyphCount = glyphBuffer.size();
                                m_currentCharacterIndex = currentCharacterIndex;
                            } else
                                glyphBuffer.expandLastAdvance(m_expansionPerOpportunity);
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
                if (treatAsSpace && (character != '\t' || !m_run.allowTabs()) && (currentCharacterIndex || character == noBreakSpace) && m_font.wordSpacing())
                    width += m_font.wordSpacing();
            } else
                m_isAfterExpansion = false;
        }

        auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, currentCharacterIndex);
        if (transformsType != TransformsType::None && FontCascade::treatAsSpace(character)) {
            charactersTreatedAsSpace.constructAndAppend(
                currentCharacterIndex,
                character == ' ',
                glyphBuffer.size() ? glyphBuffer.advanceAt(glyphBuffer.size() - 1).width() : 0,
                width);
        }

        if (m_accountForGlyphBounds) {
            bounds = font->boundsForGlyph(glyph);
            if (!currentCharacterIndex)
                m_firstGlyphOverflow = std::max<float>(0, -bounds.x());
        }

        if (m_forTextEmphasis && !FontCascade::canReceiveTextEmphasis(character))
            glyph = 0;

        glyphBuffer.add(glyph, *font, width, currentCharacterIndex);

        // Advance past the character we just dealt with.
        textIterator.advance(advanceLength);
        currentCharacterIndex = textIterator.currentIndex();

        m_runWidthSoFar += width;

        if (m_accountForGlyphBounds) {
            m_maxGlyphBoundingBoxY = std::max(m_maxGlyphBoundingBoxY, bounds.maxY());
            m_minGlyphBoundingBoxY = std::min(m_minGlyphBoundingBoxY, bounds.y());
            m_lastGlyphOverflow = std::max<float>(0, bounds.maxX() - width);
        }
    }

    commitCurrentFontRange(glyphBuffer, lastGlyphCount, currentCharacterIndex, *lastFontData, primaryFont, character, widthOfCurrentFontRange, charactersTreatedAsSpace);

    if (leftoverJustificationWidth) {
        if (m_forTextEmphasis)
            glyphBuffer.add(lastFontData->zeroWidthSpaceGlyph(), *lastFontData, leftoverJustificationWidth, m_run.length() - 1);
        else
            glyphBuffer.add(lastFontData->spaceGlyph(), *lastFontData, leftoverJustificationWidth, m_run.length() - 1);
    }
}

void WidthIterator::advance(unsigned offset, GlyphBuffer& glyphBuffer)
{
    unsigned length = m_run.length();

    if (offset > length)
        offset = length;

    if (m_currentCharacterIndex >= offset)
        return;

    if (m_run.is8Bit()) {
        Latin1TextIterator textIterator(m_run.data8(m_currentCharacterIndex), m_currentCharacterIndex, offset, length);
        advanceInternal(textIterator, glyphBuffer);
        return;
    }

    SurrogatePairAwareTextIterator textIterator(m_run.data16(m_currentCharacterIndex), m_currentCharacterIndex, offset, length);
    advanceInternal(textIterator, glyphBuffer);
}

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer& glyphBuffer)
{
    unsigned oldSize = glyphBuffer.size();
    advance(m_currentCharacterIndex + 1, glyphBuffer);
    float w = 0;
    for (unsigned i = oldSize; i < glyphBuffer.size(); ++i)
        w += glyphBuffer.advanceAt(i).width();
    width = w;
    return glyphBuffer.size() > oldSize;
}

}
