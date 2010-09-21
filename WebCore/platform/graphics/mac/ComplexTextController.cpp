/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include <ApplicationServices/ApplicationServices.h>
#include "CharacterNames.h"
#include "FloatSize.h"
#include "Font.h"
#include "TextBreakIterator.h"

#include <wtf/StdLibExtras.h>

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

ComplexTextController::ComplexTextController(const Font* font, const TextRun& run, bool mayUseNaturalWritingDirection, HashSet<const SimpleFontData*>* fallbackFonts)
    : m_font(*font)
    , m_run(run)
    , m_mayUseNaturalWritingDirection(mayUseNaturalWritingDirection)
    , m_currentCharacter(0)
    , m_end(run.length())
    , m_totalWidth(0)
    , m_runWidthSoFar(0)
    , m_numGlyphsSoFar(0)
    , m_currentRun(0)
    , m_glyphInCurrentRun(0)
    , m_characterInCurrentGlyph(0)
    , m_finalRoundingWidth(0)
    , m_padding(run.padding())
    , m_fallbackFonts(fallbackFonts)
    , m_minGlyphBoundingBoxX(numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxX(numeric_limits<float>::min())
    , m_minGlyphBoundingBoxY(numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxY(numeric_limits<float>::min())
    , m_lastRoundingGlyph(0)
{
    if (!m_padding)
        m_padPerSpace = 0;
    else {
        int numSpaces = 0;
        for (int s = 0; s < m_run.length(); s++) {
            if (Font::treatAsSpace(m_run[s]))
                numSpaces++;
        }

        if (!numSpaces)
            m_padPerSpace = 0;
        else
            m_padPerSpace = m_padding / numSpaces;
    }

    collectComplexTextRuns();
    adjustGlyphsAndAdvances();
}

int ComplexTextController::offsetForPosition(float h, bool includePartialGlyphs)
{
    if (h >= m_totalWidth)
        return m_run.ltr() ? m_end : 0;
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
                    hitGlyphEnd = max<CFIndex>(hitGlyphStart, j + 1 < complexTextRun.glyphCount() ? complexTextRun.indexAt(j + 1) : static_cast<CFIndex>(complexTextRun.stringLength()));
                else
                    hitGlyphEnd = max<CFIndex>(hitGlyphStart, j > 0 ? complexTextRun.indexAt(j - 1) : static_cast<CFIndex>(complexTextRun.stringLength()));

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
                // reordering and on font fallback should occur within a CTLine.
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

void ComplexTextController::collectComplexTextRuns()
{
    if (!m_end)
        return;

    // We break up glyph run generation for the string by FontData and (if needed) the use of small caps.
    const UChar* cp = m_run.characters();
    bool hasTrailingSoftHyphen = m_run[m_end - 1] == softHyphen;

    if (m_font.isSmallCaps() || hasTrailingSoftHyphen)
        m_smallCapsBuffer.resize(m_end);

    unsigned indexOfFontTransition = m_run.rtl() ? m_end - 1 : 0;
    const UChar* curr = m_run.rtl() ? cp + m_end  - 1 : cp;
    const UChar* end = m_run.rtl() ? cp - 1 : cp + m_end;

    // FIXME: Using HYPHEN-MINUS rather than HYPHEN because Times has a HYPHEN-MINUS glyph that looks like its
    // SOFT-HYPHEN glyph, and has no HYPHEN glyph.
    static const UChar hyphen = '-';

    if (hasTrailingSoftHyphen && m_run.rtl()) {
        collectComplexTextRunsForCharacters(&hyphen, 1, m_end - 1, m_font.glyphDataForCharacter(hyphen, false).fontData);
        indexOfFontTransition--;
        curr--;
    }

    GlyphData glyphData;
    GlyphData nextGlyphData;

    bool isSurrogate = U16_IS_SURROGATE(*curr);
    if (isSurrogate) {
        if (m_run.ltr()) {
            if (!U16_IS_SURROGATE_LEAD(curr[0]) || curr + 1 == end || !U16_IS_TRAIL(curr[1]))
                return;
            nextGlyphData = m_font.glyphDataForCharacter(U16_GET_SUPPLEMENTARY(curr[0], curr[1]), false);
        } else {
            if (!U16_IS_TRAIL(curr[0]) || curr -1 == end || !U16_IS_SURROGATE_LEAD(curr[-1]))
                return;
            nextGlyphData = m_font.glyphDataForCharacter(U16_GET_SUPPLEMENTARY(curr[-1], curr[0]), false);
        }
    } else
        nextGlyphData = m_font.glyphDataForCharacter(*curr, false);

    UChar newC = 0;

    bool isSmallCaps;
    bool nextIsSmallCaps = !isSurrogate && m_font.isSmallCaps() && !(U_GET_GC_MASK(*curr) & U_GC_M_MASK) && (newC = u_toupper(*curr)) != *curr;

    if (nextIsSmallCaps)
        m_smallCapsBuffer[curr - cp] = newC;

    while (true) {
        curr = m_run.rtl() ? curr - (isSurrogate ? 2 : 1) : curr + (isSurrogate ? 2 : 1);
        if (curr == end)
            break;

        glyphData = nextGlyphData;
        isSmallCaps = nextIsSmallCaps;
        int index = curr - cp;
        isSurrogate = U16_IS_SURROGATE(*curr);
        UChar c = *curr;
        bool forceSmallCaps = !isSurrogate && isSmallCaps && (U_GET_GC_MASK(c) & U_GC_M_MASK);
        if (isSurrogate) {
            if (m_run.ltr()) {
                if (!U16_IS_SURROGATE_LEAD(curr[0]) || curr + 1 == end || !U16_IS_TRAIL(curr[1]))
                    return;
                nextGlyphData = m_font.glyphDataForCharacter(U16_GET_SUPPLEMENTARY(curr[0], curr[1]), false);
            } else {
                if (!U16_IS_TRAIL(curr[0]) || curr -1 == end || !U16_IS_SURROGATE_LEAD(curr[-1]))
                    return;
                nextGlyphData = m_font.glyphDataForCharacter(U16_GET_SUPPLEMENTARY(curr[-1], curr[0]), false);
            }
        } else
            nextGlyphData = m_font.glyphDataForCharacter(*curr, false, forceSmallCaps);

        if (!isSurrogate && m_font.isSmallCaps()) {
            nextIsSmallCaps = forceSmallCaps || (newC = u_toupper(c)) != c;
            if (nextIsSmallCaps)
                m_smallCapsBuffer[index] = forceSmallCaps ? c : newC;
        }

        if (nextGlyphData.fontData != glyphData.fontData || nextIsSmallCaps != isSmallCaps || !nextGlyphData.glyph != !glyphData.glyph) {
            int itemStart = m_run.rtl() ? index + 1 : static_cast<int>(indexOfFontTransition);
            int itemLength = m_run.rtl() ? indexOfFontTransition - index : index - indexOfFontTransition;
            collectComplexTextRunsForCharacters((isSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, glyphData.glyph ? glyphData.fontData : 0);
            indexOfFontTransition = index;
        }
    }

    int itemLength = m_run.rtl() ? indexOfFontTransition + 1 : m_end - indexOfFontTransition - (hasTrailingSoftHyphen ? 1 : 0);
    if (itemLength) {
        int itemStart = m_run.rtl() ? 0 : indexOfFontTransition;
        collectComplexTextRunsForCharacters((nextIsSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, nextGlyphData.glyph ? nextGlyphData.fontData : 0);
    }

    if (hasTrailingSoftHyphen && m_run.ltr())
        collectComplexTextRunsForCharacters(&hyphen, 1, m_end - 1, m_font.glyphDataForCharacter(hyphen, false).fontData);
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
        CFIndex nextMappedIndex = m_stringLength;
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
                    glyphEndOffset = max<unsigned>(glyphStartOffset, g + 1 < glyphCount ? static_cast<unsigned>(complexTextRun.indexAt(g + 1)) : complexTextRun.stringLength());
                else
                    glyphEndOffset = max<unsigned>(glyphStartOffset, g > 0 ? static_cast<unsigned>(complexTextRun.indexAt(g - 1)) : complexTextRun.stringLength());
            } else
                glyphEndOffset = complexTextRun.endOffsetAt(g);

            CGSize adjustedAdvance = m_adjustedAdvances[k];

            if (glyphStartOffset + complexTextRun.stringLocation() >= m_currentCharacter)
                return;

            if (glyphBuffer && !m_characterInCurrentGlyph)
                glyphBuffer->add(m_adjustedGlyphs[k], complexTextRun.fontData(), adjustedAdvance);

            unsigned oldCharacterInCurrentGlyph = m_characterInCurrentGlyph;
            m_characterInCurrentGlyph = min(m_currentCharacter - complexTextRun.stringLocation(), glyphEndOffset) - glyphStartOffset;
            // FIXME: Instead of dividing the glyph's advance equially between the characters, this
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
    CGFloat widthSinceLastRounding = 0;
    size_t runCount = m_complexTextRuns.size();
    for (size_t r = 0; r < runCount; ++r) {
        ComplexTextRun& complexTextRun = *m_complexTextRuns[r];
        unsigned glyphCount = complexTextRun.glyphCount();
        const SimpleFontData* fontData = complexTextRun.fontData();

        const CGGlyph* glyphs = complexTextRun.glyphs();
        const CGSize* advances = complexTextRun.advances();

        bool lastRun = r + 1 == runCount;
        const UChar* cp = complexTextRun.characters();
        CGFloat roundedSpaceWidth = roundCGFloat(fontData->spaceWidth());
        bool roundsAdvances = !m_font.isPrinterFont() && fontData->platformData().roundsGlyphAdvances();
        bool hasExtraSpacing = (m_font.letterSpacing() || m_font.wordSpacing() || m_padding) && !m_run.spacingDisabled();
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
            CGSize advance = treatAsSpace ? CGSizeMake(fontData->spaceWidth(), advances[i].height) : advances[i];

            if (ch == '\t' && m_run.allowTabs()) {
                float tabWidth = m_font.tabWidth(*fontData);
                advance.width = tabWidth - fmodf(m_run.xPos() + m_totalWidth + widthSinceLastRounding, tabWidth);
            } else if (ch == zeroWidthSpace || Font::treatAsZeroWidthSpace(ch) && !treatAsSpace) {
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
            if (roundedAdvanceWidth == roundedSpaceWidth && (fontData->pitch() == FixedPitch || glyph == fontData->spaceGlyph()) && m_run.applyWordRounding())
                advance.width = fontData->adjustedSpaceWidth();

            if (hasExtraSpacing) {
                // If we're a glyph with an advance, go ahead and add in letter-spacing.
                // That way we weed out zero width lurkers.  This behavior matches the fast text code path.
                if (advance.width && m_font.letterSpacing())
                    advance.width += m_font.letterSpacing();

                // Handle justification and word-spacing.
                if (glyph == fontData->spaceGlyph()) {
                    // Account for padding. WebCore uses space padding to justify text.
                    // We distribute the specified padding over the available spaces in the run.
                    if (m_padding) {
                        // Use leftover padding if not evenly divisible by number of spaces.
                        if (m_padding < m_padPerSpace) {
                            advance.width += m_padding;
                            m_padding = 0;
                        } else {
                            float previousPadding = m_padding;
                            m_padding -= m_padPerSpace;
                            advance.width += roundf(previousPadding) - roundf(m_padding);
                        }
                    }

                    // Account for word-spacing.
                    if (treatAsSpace && characterIndex > 0 && !Font::treatAsSpace(*m_run.data(characterIndex - 1)) && m_font.wordSpacing())
                        advance.width += m_font.wordSpacing();
                }
            }

            // Deal with the float/integer impedance mismatch between CG and WebCore. "Words" (characters 
            // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
            // We adjust the width of the last character of a "word" to ensure an integer width.
            // Force characters that are used to determine word boundaries for the rounding hack
            // to be integer width, so the following words will start on an integer boundary.
            if (m_run.applyWordRounding() && Font::isRoundingHackCharacter(ch))
                advance.width = ceilCGFloat(advance.width);

            // Check to see if the next character is a "rounding hack character", if so, adjust the
            // width so that the total run width will be on an integer boundary.
            if (m_run.applyWordRounding() && !lastGlyph && Font::isRoundingHackCharacter(nextCh) || m_run.applyRunRounding() && lastGlyph) {
                CGFloat totalWidth = widthSinceLastRounding + advance.width;
                widthSinceLastRounding = ceilCGFloat(totalWidth);
                CGFloat extraWidth = widthSinceLastRounding - totalWidth;
                if (m_run.ltr())
                    advance.width += extraWidth;
                else {
                    if (m_lastRoundingGlyph)
                        m_adjustedAdvances[m_lastRoundingGlyph - 1].width += extraWidth;
                    else
                        m_finalRoundingWidth = extraWidth;
                    m_lastRoundingGlyph = m_adjustedAdvances.size() + 1;
                }
                m_totalWidth += widthSinceLastRounding;
                widthSinceLastRounding = 0;
            } else
                widthSinceLastRounding += advance.width;

            advance.height *= -1;
            m_adjustedAdvances.append(advance);
            m_adjustedGlyphs.append(glyph);
            
            FloatRect glyphBounds = fontData->boundsForGlyph(glyph);
            glyphBounds.move(glyphOrigin.x, glyphOrigin.y);
            m_minGlyphBoundingBoxX = min(m_minGlyphBoundingBoxX, glyphBounds.x());
            m_maxGlyphBoundingBoxX = max(m_maxGlyphBoundingBoxX, glyphBounds.right());
            m_minGlyphBoundingBoxY = min(m_minGlyphBoundingBoxY, glyphBounds.y());
            m_maxGlyphBoundingBoxY = max(m_maxGlyphBoundingBoxY, glyphBounds.bottom());
            glyphOrigin.x += advance.width;
            glyphOrigin.y += advance.height;
            
            lastCharacterIndex = characterIndex;
        }
        if (!isMonotonic)
            complexTextRun.setIsNonMonotonic();
    }
    m_totalWidth += widthSinceLastRounding;
}

} // namespace WebCore
