/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ComplexTextController.h"

#include "FloatSize.h"
#include "Font.h"
#include "TextBreakIterator.h"
#include "TextRun.h"
#include <ApplicationServices/ApplicationServices.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

#if defined(BUILDING_ON_LEOPARD)
// Undefined when compiling agains the 10.5 SDK.
#define kCTVersionNumber10_6 0x00030000
#endif

using namespace std;

namespace WebCore {

static inline CGFloat roundCGFloat(CGFloat f)
{
    if (sizeof(CGFloat) == sizeof(float))
        return roundf(static_cast<float>(f));
    return static_cast<CGFloat>(round(f));
}

static inline CGFloat ceilCGFloat(CGFloat f)
{
    if (sizeof(CGFloat) == sizeof(float))
        return ceilf(static_cast<float>(f));
    return static_cast<CGFloat>(ceil(f));
}

ComplexTextController::ComplexTextController(const Font* font, const TextRun& run, bool mayUseNaturalWritingDirection, HashSet<const SimpleFontData*>* fallbackFonts, bool forTextEmphasis)
    : m_font(*font)
    , m_run(run)
    , m_mayUseNaturalWritingDirection(mayUseNaturalWritingDirection)
    , m_forTextEmphasis(forTextEmphasis)
    , m_currentCharacter(0)
    , m_end(run.length())
    , m_totalWidth(0)
    , m_runWidthSoFar(0)
    , m_numGlyphsSoFar(0)
    , m_currentRun(0)
    , m_glyphInCurrentRun(0)
    , m_characterInCurrentGlyph(0)
    , m_finalRoundingWidth(0)
    , m_expansion(run.expansion())
    , m_leadingExpansion(0)
    , m_afterExpansion(!run.allowsLeadingExpansion())
    , m_fallbackFonts(fallbackFonts)
    , m_minGlyphBoundingBoxX(numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxX(numeric_limits<float>::min())
    , m_minGlyphBoundingBoxY(numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxY(numeric_limits<float>::min())
    , m_lastRoundingGlyph(0)
{
    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        bool isAfterExpansion = m_afterExpansion;
        unsigned expansionOpportunityCount = Font::expansionOpportunityCount(m_run.characters(), m_end, m_run.ltr() ? LTR : RTL, isAfterExpansion);
        if (isAfterExpansion && !m_run.allowsTrailingExpansion())
            expansionOpportunityCount--;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }

    collectComplexTextRuns();
    adjustGlyphsAndAdvances();

    m_runWidthSoFar = m_leadingExpansion;
}

int ComplexTextController::offsetForPosition(float h, bool includePartialGlyphs)
{
    if (h >= m_totalWidth)
        return m_run.ltr() ? m_end : 0;

    h -= m_leadingExpansion;
    if (h < 0)
        return m_run.ltr() ? 0 : m_end;

    CGFloat x = h;

    size_t runCount = m_complexTextRuns.size();
    size_t offsetIntoAdjustedGlyphs = 0;

    for (size_t r = 0; r < runCount; ++r) {
        const ComplexTextRun& complexTextRun = *m_complexTextRuns[r];
        for (unsigned j = 0; j < complexTextRun.glyphCount(); ++j) {
            CGFloat adjustedAdvance = m_adjustedAdvances[offsetIntoAdjustedGlyphs + j].width;
            if (x < adjustedAdvance) {
                CFIndex hitGlyphStart = complexTextRun.indexAt(j);
                CFIndex hitGlyphEnd;
                if (m_run.ltr())
                    hitGlyphEnd = max<CFIndex>(hitGlyphStart, j + 1 < complexTextRun.glyphCount() ? complexTextRun.indexAt(j + 1) : static_cast<CFIndex>(complexTextRun.indexEnd()));
                else
                    hitGlyphEnd = max<CFIndex>(hitGlyphStart, j > 0 ? complexTextRun.indexAt(j - 1) : static_cast<CFIndex>(complexTextRun.indexEnd()));

                // FIXME: Instead of dividing the glyph's advance equally between the characters, this
                // could use the glyph's "ligature carets". However, there is no Core Text API to get the
                // ligature carets.
                CFIndex hitIndex = hitGlyphStart + (hitGlyphEnd - hitGlyphStart) * (m_run.ltr() ? x / adjustedAdvance : 1 - x / adjustedAdvance);
                int stringLength = complexTextRun.stringLength();
                TextBreakIterator* cursorPositionIterator = cursorMovementIterator(complexTextRun.characters(), stringLength);
                int clusterStart;
                if (isTextBreak(cursorPositionIterator, hitIndex))
                    clusterStart = hitIndex;
                else {
                    clusterStart = textBreakPreceding(cursorPositionIterator, hitIndex);
                    if (clusterStart == TextBreakDone)
                        clusterStart = 0;
                }

                if (!includePartialGlyphs)
                    return complexTextRun.stringLocation() + clusterStart;

                int clusterEnd = textBreakFollowing(cursorPositionIterator, hitIndex);
                if (clusterEnd == TextBreakDone)
                    clusterEnd = stringLength;

                CGFloat clusterWidth;
                // FIXME: The search stops at the boundaries of complexTextRun. In theory, it should go on into neighboring ComplexTextRuns
                // derived from the same CTLine. In practice, we do not expect there to be more than one CTRun in a CTLine, as no
                // reordering and no font fallback should occur within a CTLine.
                if (clusterEnd - clusterStart > 1) {
                    clusterWidth = adjustedAdvance;
                    int firstGlyphBeforeCluster = j - 1;
                    while (firstGlyphBeforeCluster >= 0 && complexTextRun.indexAt(firstGlyphBeforeCluster) >= clusterStart && complexTextRun.indexAt(firstGlyphBeforeCluster) < clusterEnd) {
                        CGFloat width = m_adjustedAdvances[offsetIntoAdjustedGlyphs + firstGlyphBeforeCluster].width;
                        clusterWidth += width;
                        x += width;
                        firstGlyphBeforeCluster--;
                    }
                    unsigned firstGlyphAfterCluster = j + 1;
                    while (firstGlyphAfterCluster < complexTextRun.glyphCount() && complexTextRun.indexAt(firstGlyphAfterCluster) >= clusterStart && complexTextRun.indexAt(firstGlyphAfterCluster) < clusterEnd) {
                        clusterWidth += m_adjustedAdvances[offsetIntoAdjustedGlyphs + firstGlyphAfterCluster].width;
                        firstGlyphAfterCluster++;
                    }
                } else {
                    clusterWidth = adjustedAdvance / (hitGlyphEnd - hitGlyphStart);
                    x -=  clusterWidth * (m_run.ltr() ? hitIndex - hitGlyphStart : hitGlyphEnd - hitIndex - 1);
                }
                if (x <= clusterWidth / 2)
                    return complexTextRun.stringLocation() + (m_run.ltr() ? clusterStart : clusterEnd);
                else
                    return complexTextRun.stringLocation() + (m_run.ltr() ? clusterEnd : clusterStart);
            }
            x -= adjustedAdvance;
        }
        offsetIntoAdjustedGlyphs += complexTextRun.glyphCount();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static bool advanceByCombiningCharacterSequence(const UChar*& iterator, const UChar* end, UChar32& baseCharacter, unsigned& markCount)
{
    ASSERT(iterator < end);

    markCount = 0;

    baseCharacter = *iterator++;

    if (U16_IS_SURROGATE(baseCharacter)) {
        if (!U16_IS_LEAD(baseCharacter))
            return false;
        if (iterator == end)
            return false;
        UChar trail = *iterator++;
        if (!U16_IS_TRAIL(trail))
            return false;
        baseCharacter = U16_GET_SUPPLEMENTARY(baseCharacter, trail);
    }

    // Consume marks.
    while (iterator < end) {
        UChar32 nextCharacter;
        int markLength = 0;
        U16_NEXT(iterator, markLength, end - iterator, nextCharacter);
        if (!(U_GET_GC_MASK(nextCharacter) & U_GC_M_MASK))
            break;
        markCount += markLength;
        iterator += markLength;
    }

    return true;
}

void ComplexTextController::collectComplexTextRuns()
{
    if (!m_end)
        return;

    // We break up glyph run generation for the string by FontData.
    const UChar* cp = m_run.characters();

    if (m_font.isSmallCaps())
        m_smallCapsBuffer.resize(m_end);

    unsigned indexOfFontTransition = 0;
    const UChar* curr = cp;
    const UChar* end = cp + m_end;

    const SimpleFontData* fontData;
    bool isMissingGlyph;
    const SimpleFontData* nextFontData;
    bool nextIsMissingGlyph;

    unsigned markCount;
    const UChar* sequenceStart = curr;
    UChar32 baseCharacter;
    if (!advanceByCombiningCharacterSequence(curr, end, baseCharacter, markCount))
        return;

    UChar uppercaseCharacter = 0;

    bool isSmallCaps;
    bool nextIsSmallCaps = m_font.isSmallCaps() && !(U_GET_GC_MASK(baseCharacter) & U_GC_M_MASK) && (uppercaseCharacter = u_toupper(baseCharacter)) != baseCharacter;

    if (nextIsSmallCaps) {
        m_smallCapsBuffer[sequenceStart - cp] = uppercaseCharacter;
        for (unsigned i = 0; i < markCount; ++i)
            m_smallCapsBuffer[sequenceStart - cp + i + 1] = sequenceStart[i + 1];
    }

    nextIsMissingGlyph = false;
#if !PLATFORM(WX)
    nextFontData = m_font.fontDataForCombiningCharacterSequence(sequenceStart, curr - sequenceStart, nextIsSmallCaps ? SmallCapsVariant : NormalVariant);
    if (!nextFontData) {
        if (markCount)
            nextFontData = systemFallbackFontData();
        else
            nextIsMissingGlyph = true;
    }
#endif

    while (curr < end) {
        fontData = nextFontData;
        isMissingGlyph = nextIsMissingGlyph;
        isSmallCaps = nextIsSmallCaps;
        int index = curr - cp;

        if (!advanceByCombiningCharacterSequence(curr, end, baseCharacter, markCount))
            return;

        if (m_font.isSmallCaps()) {
            nextIsSmallCaps = (uppercaseCharacter = u_toupper(baseCharacter)) != baseCharacter;
            if (nextIsSmallCaps) {
                m_smallCapsBuffer[index] = uppercaseCharacter;
                for (unsigned i = 0; i < markCount; ++i)
                    m_smallCapsBuffer[index + i + 1] = cp[index + i + 1];
            }
        }

        nextIsMissingGlyph = false;
        if (baseCharacter == zeroWidthJoiner)
            nextFontData = fontData;
#if !PLATFORM(WX)
        else {
            nextFontData = m_font.fontDataForCombiningCharacterSequence(cp + index, curr - cp - index, nextIsSmallCaps ? SmallCapsVariant : NormalVariant);
            if (!nextFontData) {
                if (markCount)
                    nextFontData = systemFallbackFontData();
                else
                    nextIsMissingGlyph = true;
            }
        }
#endif

        if (nextFontData != fontData || nextIsMissingGlyph != isMissingGlyph) {
            int itemStart = static_cast<int>(indexOfFontTransition);
            int itemLength = index - indexOfFontTransition;
            collectComplexTextRunsForCharacters((isSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, !isMissingGlyph ? fontData : 0);
            indexOfFontTransition = index;
        }
    }

    int itemLength = m_end - indexOfFontTransition;
    if (itemLength) {
        int itemStart = indexOfFontTransition;
        collectComplexTextRunsForCharacters((nextIsSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, !nextIsMissingGlyph ? nextFontData : 0);
    }

    if (!m_run.ltr())
        m_complexTextRuns.reverse();
}

#if USE(CORE_TEXT) && USE(ATSUI)
static inline bool shouldUseATSUIAPI()
{
    enum TypeRenderingAPIToUse { UnInitialized, UseATSUI, UseCoreText };
    static TypeRenderingAPIToUse apiToUse = UnInitialized;

    if (UNLIKELY(apiToUse == UnInitialized)) {
        if (&CTGetCoreTextVersion != 0 && CTGetCoreTextVersion() >= kCTVersionNumber10_6)
            apiToUse = UseCoreText;
        else
            apiToUse = UseATSUI;
    }

    return apiToUse == UseATSUI;
}
#endif

CFIndex ComplexTextController::ComplexTextRun::indexAt(size_t i) const
{
#if USE(CORE_TEXT) && USE(ATSUI)
    return shouldUseATSUIAPI() ? m_atsuiIndices[i] : m_coreTextIndices[i];
#elif USE(ATSUI)
    return m_atsuiIndices[i];
#elif USE(CORE_TEXT)
    return m_coreTextIndices[i];
#endif
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* cp, unsigned length, unsigned stringLocation, const SimpleFontData* fontData)
{
    if (!fontData) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, m_run.ltr()));
        return;
    }

#if USE(CORE_TEXT) && USE(ATSUI)
    if (shouldUseATSUIAPI())
        return collectComplexTextRunsForCharactersATSUI(cp, length, stringLocation, fontData);
    return collectComplexTextRunsForCharactersCoreText(cp, length, stringLocation, fontData);
#elif USE(ATSUI)
    return collectComplexTextRunsForCharactersATSUI(cp, length, stringLocation, fontData);
#elif USE(CORE_TEXT)
    return collectComplexTextRunsForCharactersCoreText(cp, length, stringLocation, fontData);
#endif
}

ComplexTextController::ComplexTextRun::ComplexTextRun(const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr)
    : m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
    , m_indexEnd(stringLength)
    , m_isMonotonic(true)
{
#if USE(CORE_TEXT) && USE(ATSUI)
    shouldUseATSUIAPI() ? createTextRunFromFontDataATSUI(ltr) : createTextRunFromFontDataCoreText(ltr);
#elif USE(ATSUI)
    createTextRunFromFontDataATSUI(ltr);
#elif USE(CORE_TEXT)
    createTextRunFromFontDataCoreText(ltr);
#endif
}

void ComplexTextController::ComplexTextRun::setIsNonMonotonic()
{
    ASSERT(m_isMonotonic);
    m_isMonotonic = false;

    Vector<bool, 64> mappedIndices(m_stringLength);
    for (size_t i = 0; i < m_glyphCount; ++i) {
        ASSERT(indexAt(i) < static_cast<CFIndex>(m_stringLength));
        mappedIndices[indexAt(i)] = true;
    }

    m_glyphEndOffsets.grow(m_glyphCount);
    for (size_t i = 0; i < m_glyphCount; ++i) {
        CFIndex nextMappedIndex = m_indexEnd;
        for (size_t j = indexAt(i) + 1; j < m_stringLength; ++j) {
            if (mappedIndices[j]) {
                nextMappedIndex = j;
                break;
            }
        }
        m_glyphEndOffsets[i] = nextMappedIndex;
    }
}

void ComplexTextController::advance(unsigned offset, GlyphBuffer* glyphBuffer)
{
    if (static_cast<int>(offset) > m_end)
        offset = m_end;

    if (offset <= m_currentCharacter)
        return;

    m_currentCharacter = offset;

    size_t runCount = m_complexTextRuns.size();

    bool ltr = m_run.ltr();

    unsigned k = ltr ? m_numGlyphsSoFar : m_adjustedGlyphs.size() - 1 - m_numGlyphsSoFar;
    while (m_currentRun < runCount) {
        const ComplexTextRun& complexTextRun = *m_complexTextRuns[ltr ? m_currentRun : runCount - 1 - m_currentRun];
        size_t glyphCount = complexTextRun.glyphCount();
        unsigned g = ltr ? m_glyphInCurrentRun : glyphCount - 1 - m_glyphInCurrentRun;
        while (m_glyphInCurrentRun < glyphCount) {
            unsigned glyphStartOffset = complexTextRun.indexAt(g);
            unsigned glyphEndOffset;
            if (complexTextRun.isMonotonic()) {
                if (ltr)
                    glyphEndOffset = max<unsigned>(glyphStartOffset, static_cast<unsigned>(g + 1 < glyphCount ? complexTextRun.indexAt(g + 1) : complexTextRun.indexEnd()));
                else
                    glyphEndOffset = max<unsigned>(glyphStartOffset, static_cast<unsigned>(g > 0 ? complexTextRun.indexAt(g - 1) : complexTextRun.indexEnd()));
            } else
                glyphEndOffset = complexTextRun.endOffsetAt(g);

            CGSize adjustedAdvance = m_adjustedAdvances[k];

            if (glyphStartOffset + complexTextRun.stringLocation() >= m_currentCharacter)
                return;

            if (glyphBuffer && !m_characterInCurrentGlyph)
                glyphBuffer->add(m_adjustedGlyphs[k], complexTextRun.fontData(), adjustedAdvance);

            unsigned oldCharacterInCurrentGlyph = m_characterInCurrentGlyph;
            m_characterInCurrentGlyph = min(m_currentCharacter - complexTextRun.stringLocation(), glyphEndOffset) - glyphStartOffset;
            // FIXME: Instead of dividing the glyph's advance equally between the characters, this
            // could use the glyph's "ligature carets". However, there is no Core Text API to get the
            // ligature carets.
            if (glyphStartOffset == glyphEndOffset) {
                // When there are multiple glyphs per character we need to advance by the full width of the glyph.
                ASSERT(m_characterInCurrentGlyph == oldCharacterInCurrentGlyph);
                m_runWidthSoFar += adjustedAdvance.width;
            } else
                m_runWidthSoFar += adjustedAdvance.width * (m_characterInCurrentGlyph - oldCharacterInCurrentGlyph) / (glyphEndOffset - glyphStartOffset);

            if (glyphEndOffset + complexTextRun.stringLocation() > m_currentCharacter)
                return;

            m_numGlyphsSoFar++;
            m_glyphInCurrentRun++;
            m_characterInCurrentGlyph = 0;
            if (ltr) {
                g++;
                k++;
            } else {
                g--;
                k--;
            }
        }
        m_currentRun++;
        m_glyphInCurrentRun = 0;
    }
    if (!ltr && m_numGlyphsSoFar == m_adjustedAdvances.size())
        m_runWidthSoFar += m_finalRoundingWidth;
}

void ComplexTextController::adjustGlyphsAndAdvances()
{
    CGFloat widthSinceLastCommit = 0;
    size_t runCount = m_complexTextRuns.size();
    bool hasExtraSpacing = (m_font.letterSpacing() || m_font.wordSpacing() || m_expansion) && !m_run.spacingDisabled();
    for (size_t r = 0; r < runCount; ++r) {
        ComplexTextRun& complexTextRun = *m_complexTextRuns[r];
        unsigned glyphCount = complexTextRun.glyphCount();
        const SimpleFontData* fontData = complexTextRun.fontData();

        const CGGlyph* glyphs = complexTextRun.glyphs();
        const CGSize* advances = complexTextRun.advances();

        bool lastRun = r + 1 == runCount;
        bool roundsAdvances = !m_font.isPrinterFont() && fontData->platformData().roundsGlyphAdvances();
        float spaceWidth = fontData->spaceWidth() - fontData->syntheticBoldOffset();
        CGFloat roundedSpaceWidth = roundCGFloat(spaceWidth);
        const UChar* cp = complexTextRun.characters();
        CGPoint glyphOrigin = CGPointZero;
        CFIndex lastCharacterIndex = m_run.ltr() ? numeric_limits<CFIndex>::min() : numeric_limits<CFIndex>::max();
        bool isMonotonic = true;

        for (unsigned i = 0; i < glyphCount; i++) {
            CFIndex characterIndex = complexTextRun.indexAt(i);
            if (m_run.ltr()) {
                if (characterIndex < lastCharacterIndex)
                    isMonotonic = false;
            } else {
                if (characterIndex > lastCharacterIndex)
                    isMonotonic = false;
            }
            UChar ch = *(cp + characterIndex);
            bool lastGlyph = lastRun && i + 1 == glyphCount;
            UChar nextCh;
            if (lastGlyph)
                nextCh = ' ';
            else if (i + 1 < glyphCount)
                nextCh = *(cp + complexTextRun.indexAt(i + 1));
            else
                nextCh = *(m_complexTextRuns[r + 1]->characters() + m_complexTextRuns[r + 1]->indexAt(0));

            bool treatAsSpace = Font::treatAsSpace(ch);
            CGGlyph glyph = treatAsSpace ? fontData->spaceGlyph() : glyphs[i];
            CGSize advance = treatAsSpace ? CGSizeMake(spaceWidth, advances[i].height) : advances[i];

            if (ch == '\t' && m_run.allowTabs()) {
                float tabWidth = m_font.tabWidth(*fontData);
                advance.width = tabWidth - fmodf(m_run.xPos() + m_totalWidth + widthSinceLastCommit, tabWidth);
            } else if (Font::treatAsZeroWidthSpace(ch) && !treatAsSpace) {
                advance.width = 0;
                glyph = fontData->spaceGlyph();
            }

            float roundedAdvanceWidth = roundf(advance.width);
            if (roundsAdvances)
                advance.width = roundedAdvanceWidth;

            advance.width += fontData->syntheticBoldOffset();

 
            // We special case spaces in two ways when applying word rounding. 
            // First, we round spaces to an adjusted width in all fonts. 
            // Second, in fixed-pitch fonts we ensure that all glyphs that 
            // match the width of the space glyph have the same width as the space glyph. 
            if (m_run.applyWordRounding() && roundedAdvanceWidth == roundedSpaceWidth && (fontData->pitch() == FixedPitch || glyph == fontData->spaceGlyph()))
                advance.width = fontData->adjustedSpaceWidth();

            if (hasExtraSpacing) {
                // If we're a glyph with an advance, go ahead and add in letter-spacing.
                // That way we weed out zero width lurkers.  This behavior matches the fast text code path.
                if (advance.width && m_font.letterSpacing())
                    advance.width += m_font.letterSpacing();

                // Handle justification and word-spacing.
                if (treatAsSpace || Font::isCJKIdeographOrSymbol(ch)) {
                    // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                    if (m_expansion) {
                        float previousExpansion = m_expansion;
                        if (!treatAsSpace && !m_afterExpansion) {
                            // Take the expansion opportunity before this ideograph.
                            m_expansion -= m_expansionPerOpportunity;
                            float expansionAtThisOpportunity = !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                            m_totalWidth += expansionAtThisOpportunity;
                            if (m_adjustedAdvances.isEmpty())
                                m_leadingExpansion = expansionAtThisOpportunity;
                            else
                                m_adjustedAdvances.last().width += expansionAtThisOpportunity;
                            previousExpansion = m_expansion;
                        }
                        if (!lastGlyph || m_run.allowsTrailingExpansion()) {
                            m_expansion -= m_expansionPerOpportunity;
                            advance.width += !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                            m_afterExpansion = true;
                        }
                    } else
                        m_afterExpansion = false;

                    // Account for word-spacing.
                    if (treatAsSpace && characterIndex > 0 && !Font::treatAsSpace(*m_run.data(characterIndex - 1)) && m_font.wordSpacing())
                        advance.width += m_font.wordSpacing();
                } else
                    m_afterExpansion = false;
            }

            // Apply rounding hacks if needed.
            // We adjust the width of the last character of a "word" to ensure an integer width. 
            // Force characters that are used to determine word boundaries for the rounding hack 
            // to be integer width, so the following words will start on an integer boundary. 
            if (m_run.applyWordRounding() && Font::isRoundingHackCharacter(ch)) 
                advance.width = ceilCGFloat(advance.width); 

            // Check to see if the next character is a "rounding hack character", if so, adjust the 
            // width so that the total run width will be on an integer boundary. 
            if ((m_run.applyWordRounding() && !lastGlyph && Font::isRoundingHackCharacter(nextCh)) || (m_run.applyRunRounding() && lastGlyph)) { 
                CGFloat totalWidth = widthSinceLastCommit + advance.width; 
                widthSinceLastCommit = ceilCGFloat(totalWidth); 
                CGFloat extraWidth = widthSinceLastCommit - totalWidth; 
                if (m_run.ltr()) 
                    advance.width += extraWidth; 
                else { 
                    if (m_lastRoundingGlyph) 
                        m_adjustedAdvances[m_lastRoundingGlyph - 1].width += extraWidth; 
                    else 
                        m_finalRoundingWidth = extraWidth; 
                    m_lastRoundingGlyph = m_adjustedAdvances.size() + 1; 
                } 
                m_totalWidth += widthSinceLastCommit; 
                widthSinceLastCommit = 0; 
            } else 
                widthSinceLastCommit += advance.width; 

            // FIXME: Combining marks should receive a text emphasis mark if they are combine with a space.
            if (m_forTextEmphasis && (!Font::canReceiveTextEmphasis(ch) || (U_GET_GC_MASK(ch) & U_GC_M_MASK)))
                glyph = 0;

            advance.height *= -1;
            m_adjustedAdvances.append(advance);
            m_adjustedGlyphs.append(glyph);
            
            FloatRect glyphBounds = fontData->boundsForGlyph(glyph);
            glyphBounds.move(glyphOrigin.x, glyphOrigin.y);
            m_minGlyphBoundingBoxX = min(m_minGlyphBoundingBoxX, glyphBounds.x());
            m_maxGlyphBoundingBoxX = max(m_maxGlyphBoundingBoxX, glyphBounds.maxX());
            m_minGlyphBoundingBoxY = min(m_minGlyphBoundingBoxY, glyphBounds.y());
            m_maxGlyphBoundingBoxY = max(m_maxGlyphBoundingBoxY, glyphBounds.maxY());
            glyphOrigin.x += advance.width;
            glyphOrigin.y += advance.height;
            
            lastCharacterIndex = characterIndex;
        }
        if (!isMonotonic)
            complexTextRun.setIsNonMonotonic();
    }
    m_totalWidth += widthSinceLastCommit;
}

} // namespace WebCore
