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
    // FIXME: Should we clamp m_expansion so it can never be negative?

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
        beforeWidth += width(advances[i]);

    ASSERT(lastGlyphCount <= glyphBufferSize);

    font.applyTransforms(glyphBuffer, lastGlyphCount, m_currentCharacterIndex, m_enableKerning, m_requiresShaping, m_font.fontDescription().computedLocale(), m_run.text(), m_run.direction());
    glyphBufferSize = glyphBuffer.size();

    GlyphBufferOrigin* origins = glyphBuffer.origins(0);
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i) {
        setHeight(advances[i], -height(advances[i]));
        setY(origins[i], -y(origins[i]));
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
            setWidth(*glyphBuffer.advances(i - 1), originalAdvances.advanceBeforeCharacter);
        setWidth(*glyphBuffer.advances(i), originalAdvances.advanceAtCharacter);
    }
    charactersTreatedAsSpace.clear();

    float afterWidth = 0;
    for (unsigned i = lastGlyphCount; i < glyphBufferSize; ++i)
        afterWidth += width(advances[i]);

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

bool WidthIterator::hasExtraSpacing() const
{
    return (m_font.letterSpacing() || m_font.wordSpacing() || m_expansion) && !m_run.spacingDisabled();
}

template <typename TextIterator>
inline void WidthIterator::advanceInternal(TextIterator& textIterator, GlyphBuffer& glyphBuffer)
{
    // The core logic here needs to match FontCascade::widthForSimpleText()
    bool rtl = m_run.rtl();

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
        m_containsTabs |= character == tabCharacter;
        currentCharacterIndex = textIterator.currentIndex();
        unsigned advanceLength = clusterLength;
        if (currentCharacterIndex + advanceLength == m_run.length())
            m_lastCharacterIndex = currentCharacterIndex;
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

        float width = font->widthForGlyph(glyph);

        if (font != lastFontData) {
            commitCurrentFontRange(glyphBuffer, lastGlyphCount, currentCharacterIndex, *lastFontData, primaryFont, character, widthOfCurrentFontRange, charactersTreatedAsSpace);
            lastGlyphCount = glyphBuffer.size();
            lastFontData = font;
            widthOfCurrentFontRange = width;
        } else
            widthOfCurrentFontRange += width;

        auto transformsType = shouldApplyFontTransforms(glyphBuffer, lastGlyphCount, currentCharacterIndex);
        if (transformsType != TransformsType::None && FontCascade::treatAsSpace(character)) {
            charactersTreatedAsSpace.constructAndAppend(
                currentCharacterIndex,
                character == ' ',
                glyphBuffer.size() ? WebCore::width(glyphBuffer.advanceAt(glyphBuffer.size() - 1)) : 0,
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
}

auto WidthIterator::calculateAdditionalWidth(GlyphBuffer& glyphBuffer, GlyphBufferStringOffset currentCharacterIndex, unsigned leadingGlyphIndex, unsigned trailingGlyphIndex, float position) const -> AdditionalWidth
{
    float leftAdditionalWidth = 0;
    float rightAdditionalWidth = 0;
    float leftExpansionAdditionalWidth = 0;
    float rightExpansionAdditionalWidth = 0;

    auto character = m_run[currentCharacterIndex];

    if (character == tabCharacter && m_run.allowTabs()) {
        auto& font = glyphBuffer.fontAt(trailingGlyphIndex);
        auto newWidth = m_font.tabWidth(font, m_run.tabSize(), position);
        auto currentWidth = width(glyphBuffer.advanceAt(trailingGlyphIndex));
        rightAdditionalWidth += newWidth - currentWidth;
    }

    if (hasExtraSpacing()) {
        bool treatAsSpace = FontCascade::treatAsSpace(character);

        // This is a heuristic to determine if the character is non-visible. Non-visible characters don't get letter-spacing.
        float baseWidth = 0;
        for (unsigned i = leadingGlyphIndex; i <= trailingGlyphIndex; ++i)
            baseWidth += width(glyphBuffer.advanceAt(i));
        if (baseWidth)
            rightAdditionalWidth += m_font.letterSpacing();

        if (treatAsSpace && (character != tabCharacter || !m_run.allowTabs()) && (currentCharacterIndex || character == noBreakSpace) && m_font.wordSpacing())
            rightAdditionalWidth += m_font.wordSpacing();

        if (m_expansion > 0) {
            bool currentIsLastCharacter = m_lastCharacterIndex && currentCharacterIndex == static_cast<GlyphBufferStringOffset>(*m_lastCharacterIndex);

            bool isLeftmostCharacter = !currentCharacterIndex;
            bool isRightmostCharacter = currentIsLastCharacter;
            if (!m_run.ltr())
                std::swap(isLeftmostCharacter, isRightmostCharacter);

            bool forceLeftExpansion = isLeftmostCharacter && (m_run.expansionBehavior() & LeftExpansionMask) == ForceLeftExpansion;
            bool forceRightExpansion = isRightmostCharacter && (m_run.expansionBehavior() & RightExpansionMask) == ForceRightExpansion;
            bool forbidLeftExpansion = isLeftmostCharacter && (m_run.expansionBehavior() & LeftExpansionMask) == ForbidLeftExpansion;
            bool forbidRightExpansion = isRightmostCharacter && (m_run.expansionBehavior() & RightExpansionMask) == ForbidRightExpansion;

            static const bool expandAroundIdeographs = FontCascade::canExpandAroundIdeographsInComplexText();
            bool isIdeograph = expandAroundIdeographs && FontCascade::isCJKIdeographOrSymbol(character);

            if (treatAsSpace || isIdeograph || forceLeftExpansion || forceRightExpansion) {
                auto [expandLeft, expandRight] = expansionLocation(isIdeograph, treatAsSpace, m_run.ltr(), m_isAfterExpansion, forbidLeftExpansion, forbidRightExpansion, forceLeftExpansion, forceRightExpansion);

                if (expandLeft)
                    leftExpansionAdditionalWidth += m_expansionPerOpportunity;
                if (expandRight)
                    rightExpansionAdditionalWidth += m_expansionPerOpportunity;
            }
        }
    }

    return { leftAdditionalWidth, rightAdditionalWidth, leftExpansionAdditionalWidth, rightExpansionAdditionalWidth };
}

struct GlyphIndexRange {
    // This means the character got expanded to glyphs inside the GlyphBuffer at indices [leadingGlyphIndex, trailingGlyphIndex].
    unsigned leadingGlyphIndex;
    unsigned trailingGlyphIndex;
};

void WidthIterator::applyAdditionalWidth(GlyphBuffer& glyphBuffer, GlyphIndexRange glyphIndexRange, float leftAdditionalWidth, float rightAdditionalWidth, float leftExpansionAdditionalWidth, float rightExpansionAdditionalWidth)
{
    m_expansion -= leftExpansionAdditionalWidth + rightExpansionAdditionalWidth;

    leftAdditionalWidth += leftExpansionAdditionalWidth;
    rightAdditionalWidth += rightExpansionAdditionalWidth;

    m_runWidthSoFar += leftAdditionalWidth;
    m_runWidthSoFar += rightAdditionalWidth;

    if (leftAdditionalWidth) {
        if (m_run.ltr()) {
            // Left additional width in LTR means the previous (leading) glyph's right side gets expanded.
            auto leadingGlyphIndex = glyphIndexRange.leadingGlyphIndex;
            if (leadingGlyphIndex)
                glyphBuffer.expandAdvance(leadingGlyphIndex - 1, leftAdditionalWidth);
            else
                glyphBuffer.expandInitialAdvance(leftAdditionalWidth);
        } else {
            // Left additional width in RTL means the next (trailing) glyph's right side gets expanded.
            auto trailingGlyphIndex = glyphIndexRange.trailingGlyphIndex;
            if (trailingGlyphIndex + 1 < glyphBuffer.size())
                glyphBuffer.expandAdvance(trailingGlyphIndex + 1, leftAdditionalWidth);
            else {
                m_leftoverJustificationWidth = leftAdditionalWidth;
                // We can't actually add in this width just yet.
                // Add it in when the client calls advance() again or finalize().
                m_runWidthSoFar -= m_leftoverJustificationWidth;
            }
        }
    }

    if (rightAdditionalWidth) {
        // Right additional width means the current glyph's right side gets expanded. This is true for both LTR and RTL.
        glyphBuffer.expandAdvance(glyphIndexRange.trailingGlyphIndex, rightAdditionalWidth);
    }
}

void WidthIterator::applyExtraSpacingAfterShaping(GlyphBuffer& glyphBuffer, unsigned characterStartIndex, unsigned glyphBufferStartIndex, unsigned characterDestinationIndex, float startingRunWidth)
{
    Vector<Optional<GlyphIndexRange>> characterIndexToGlyphIndexRange(m_run.length(), WTF::nullopt);
    Vector<float> advanceWidths(m_run.length(), 0);
    for (unsigned i = glyphBufferStartIndex; i < glyphBuffer.size(); ++i) {
        auto stringOffset = glyphBuffer.stringOffsetAt(i);
        advanceWidths[stringOffset] += width(glyphBuffer.advanceAt(i));
        auto& glyphIndexRange = characterIndexToGlyphIndexRange[stringOffset];
        if (glyphIndexRange)
            glyphIndexRange->trailingGlyphIndex = i;
        else
            glyphIndexRange = {{i, i}};
    }

    // SVG can stretch advances
    if (m_run.horizontalGlyphStretch() != 1) {
        for (unsigned i = glyphBufferStartIndex; i < glyphBuffer.size(); ++i) {
            // All characters' advances get stretched, except apparently tab characters...
            // This doesn't make much sense, because even tab characters get letter-spacing...
            auto stringOffset = glyphBuffer.stringOffsetAt(i);
            auto character = m_run[stringOffset];
            if (character == tabCharacter)
                continue;

            auto currentAdvance = width(glyphBuffer.advanceAt(i));
            auto newAdvance = currentAdvance * m_run.horizontalGlyphStretch();
            glyphBuffer.expandAdvance(i, newAdvance - currentAdvance);
        }
    }

    float position = m_run.xPos() + startingRunWidth;
    for (auto i = characterStartIndex; i < characterDestinationIndex; ++i) {
        auto& glyphIndexRange = characterIndexToGlyphIndexRange[i];
        if (!glyphIndexRange)
            continue;

        auto width = calculateAdditionalWidth(glyphBuffer, i, glyphIndexRange->leadingGlyphIndex, glyphIndexRange->trailingGlyphIndex, position);
        applyAdditionalWidth(glyphBuffer, glyphIndexRange.value(), width.left, width.right, width.leftExpansion, width.rightExpansion);

        m_isAfterExpansion = (m_run.ltr() && width.rightExpansion) || (!m_run.ltr() && width.leftExpansion);

        // This isn't quite perfect, because we may come across a tab character in between two glyphs which both report to correspond to a previous character.
        // But, the fundamental concept of tabs isn't really compatible with complex text shaping, so this is probably okay.
        // We can probably just do the best we can here.
        // The only alternative, to calculate this position in glyph-space rather than character-space,
        // is O(n^2) because we're iterating across the string here, rather than glyphs, so we can't keep the calculation up-to-date,
        // which means calculateAdditionalWidth() would have to calculate the result from scratch whenever it's needed.
        // And we can't do some sort of prefix-sum thing because applyAdditionalWidth() would modify the values,
        // so updating the data structure each turn of this loop would also end up being O(n^2).
        // Unfortunately, strings with tabs are more likely to be long data-table kind of strings, which means O(n^2) is not acceptable.
        // Also, even if we did the O(n^2) thing, there would still be cases that wouldn't be perfect
        // (because the fundamental concept of tabs isn't really compatible with complex text shaping),
        // so let's choose the fast-wrong approach here instead of the slow-wrong approach.
        position += advanceWidths[i]
            + width.left
            + width.right
            + width.leftExpansion
            + width.rightExpansion;
    }
}

void WidthIterator::finalize(GlyphBuffer& buffer)
{
    ASSERT(m_run.rtl() || !m_leftoverJustificationWidth);
    // In LTR this does nothing. In RTL, this adds left width by moving the whole run to the right.
    buffer.expandInitialAdvance(m_leftoverJustificationWidth);
    m_runWidthSoFar += m_leftoverJustificationWidth;
    m_leftoverJustificationWidth = 0;
}

void WidthIterator::advance(unsigned offset, GlyphBuffer& glyphBuffer)
{
    m_containsTabs = false;
    unsigned length = m_run.length();

    if (offset > length)
        offset = length;

    if (m_currentCharacterIndex >= offset)
        return;

    unsigned characterStartIndex = m_currentCharacterIndex;
    unsigned glyphBufferStartIndex = glyphBuffer.size();
    float startingRunWidth = m_runWidthSoFar;

    if (m_run.is8Bit()) {
        Latin1TextIterator textIterator(m_run.data8(m_currentCharacterIndex), m_currentCharacterIndex, offset, length);
        advanceInternal(textIterator, glyphBuffer);
    } else {
        SurrogatePairAwareTextIterator textIterator(m_run.data16(m_currentCharacterIndex), m_currentCharacterIndex, offset, length);
        advanceInternal(textIterator, glyphBuffer);
    }

    // In general, we have to apply spacing after shaping, because shaping requires its input to be unperturbed (see https://bugs.webkit.org/show_bug.cgi?id=215052).
    // So, if there's extra spacing to add, do it here after shaping occurs.
    if (glyphBufferStartIndex < glyphBuffer.size()) {
        glyphBuffer.expandAdvance(glyphBufferStartIndex, m_leftoverJustificationWidth);
        m_runWidthSoFar += m_leftoverJustificationWidth;
        m_leftoverJustificationWidth = 0;
    }

    if (hasExtraSpacing() || m_containsTabs || m_run.horizontalGlyphStretch() != 1)
        applyExtraSpacingAfterShaping(glyphBuffer, characterStartIndex, glyphBufferStartIndex, offset, startingRunWidth);
}

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer& glyphBuffer)
{
    unsigned oldSize = glyphBuffer.size();
    advance(m_currentCharacterIndex + 1, glyphBuffer);
    float w = 0;
    for (unsigned i = oldSize; i < glyphBuffer.size(); ++i)
        w += WebCore::width(glyphBuffer.advanceAt(i));
    width = w;
    return glyphBuffer.size() > oldSize;
}

}
