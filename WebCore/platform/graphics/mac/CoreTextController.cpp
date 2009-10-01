/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CoreTextController.h"

#if USE(CORE_TEXT)

#include "CharacterNames.h"
#include "Font.h"
#include "FontCache.h"
#include "SimpleFontData.h"
#include "TextBreakIterator.h"
#include <wtf/MathExtras.h>

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

CoreTextController::CoreTextRun::CoreTextRun(CTRunRef ctRun, const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength)
    : m_CTRun(ctRun)
    , m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
{
    m_glyphCount = CTRunGetGlyphCount(ctRun);
    m_indices = CTRunGetStringIndicesPtr(ctRun);
    if (!m_indices) {
        m_indicesData.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, m_glyphCount * sizeof(CFIndex)));
        CFDataIncreaseLength(m_indicesData.get(), m_glyphCount * sizeof(CFIndex));
        m_indices = reinterpret_cast<const CFIndex*>(CFDataGetMutableBytePtr(m_indicesData.get()));
        CTRunGetStringIndices(ctRun, CFRangeMake(0, 0), const_cast<CFIndex*>(m_indices));
    }
}

// Missing glyphs run constructor. Core Text will not generate a run of missing glyphs, instead falling back on
// glyphs from LastResort. We want to use the primary font's missing glyph in order to match the fast text code path.
CoreTextController::CoreTextRun::CoreTextRun(const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr)
    : m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
{
    Vector<CFIndex, 16> indices;
    unsigned r = 0;
    while (r < stringLength) {
        indices.append(r);
        if (U_IS_SURROGATE(characters[r])) {
            ASSERT(r + 1 < stringLength);
            ASSERT(U_IS_SURROGATE_LEAD(characters[r]));
            ASSERT(U_IS_TRAIL(characters[r + 1]));
            r += 2;
        } else
            r++;
    }
    m_glyphCount = indices.size();
    if (!ltr) {
        for (unsigned r = 0, end = m_glyphCount - 1; r < m_glyphCount / 2; ++r, --end)
            std::swap(indices[r], indices[end]);
    }
    m_indicesData.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, m_glyphCount * sizeof(CFIndex)));
    CFDataAppendBytes(m_indicesData.get(), reinterpret_cast<const UInt8*>(indices.data()), m_glyphCount * sizeof(CFIndex));
    m_indices = reinterpret_cast<const CFIndex*>(CFDataGetBytePtr(m_indicesData.get()));
}

CoreTextController::CoreTextController(const Font* font, const TextRun& run, bool mayUseNaturalWritingDirection, HashSet<const SimpleFontData*>* fallbackFonts)
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
    , m_finalRoundingWidth(0)
    , m_fallbackFonts(fallbackFonts)
    , m_lastRoundingGlyph(0)
{
    m_padding = m_run.padding();
    if (!m_padding)
        m_padPerSpace = 0;
    else {
        float numSpaces = 0;
        for (int s = 0; s < m_run.length(); s++)
            if (Font::treatAsSpace(m_run[s]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_run.padding() / numSpaces);
    }

    collectCoreTextRuns();
    adjustGlyphsAndAdvances();
}

int CoreTextController::offsetForPosition(int h, bool includePartialGlyphs)
{
    // FIXME: For positions occurring within a ligature, we should return the closest "ligature caret" or
    // approximate it by dividing the width of the ligature by the number of characters it encompasses.
    // However, Core Text does not expose a low-level API for directly finding
    // out how many characters a ligature encompasses (the "attachment count").
    if (h >= m_totalWidth)
        return m_run.ltr() ? m_end : 0;
    if (h < 0)
        return m_run.ltr() ? 0 : m_end;

    CGFloat x = h;

    size_t runCount = m_coreTextRuns.size();
    size_t offsetIntoAdjustedGlyphs = 0;

    for (size_t r = 0; r < runCount; ++r) {
        const CoreTextRun& coreTextRun = m_coreTextRuns[r];
        for (unsigned j = 0; j < coreTextRun.glyphCount(); ++j) {
            CGFloat adjustedAdvance = m_adjustedAdvances[offsetIntoAdjustedGlyphs + j].width;
            if (x <= adjustedAdvance) {
                CFIndex hitIndex = coreTextRun.indexAt(j);
                int stringLength = coreTextRun.stringLength();
                TextBreakIterator* cursorPositionIterator = cursorMovementIterator(coreTextRun.characters(), stringLength);
                int clusterStart;
                if (isTextBreak(cursorPositionIterator, hitIndex))
                    clusterStart = hitIndex;
                else {
                    clusterStart = textBreakPreceding(cursorPositionIterator, hitIndex);
                    if (clusterStart == TextBreakDone)
                        clusterStart = 0;
                }

                if (!includePartialGlyphs)
                    return coreTextRun.stringLocation() + clusterStart;

                int clusterEnd = textBreakFollowing(cursorPositionIterator, hitIndex);
                if (clusterEnd == TextBreakDone)
                    clusterEnd = stringLength;

                CGFloat clusterWidth = adjustedAdvance;
                // FIXME: The search stops at the boundaries of coreTextRun. In theory, it should go on into neighboring CoreTextRuns
                // derived from the same CTLine. In practice, we do not expect there to be more than one CTRun in a CTLine, as no
                // reordering and on font fallback should occur within a CTLine.
                if (clusterEnd - clusterStart > 1) {
                    int firstGlyphBeforeCluster = j - 1;
                    while (firstGlyphBeforeCluster >= 0 && coreTextRun.indexAt(firstGlyphBeforeCluster) >= clusterStart && coreTextRun.indexAt(firstGlyphBeforeCluster) < clusterEnd) {
                        CGFloat width = m_adjustedAdvances[offsetIntoAdjustedGlyphs + firstGlyphBeforeCluster].width;
                        clusterWidth += width;
                        x += width;
                        firstGlyphBeforeCluster--;
                    }
                    unsigned firstGlyphAfterCluster = j + 1;
                    while (firstGlyphAfterCluster < coreTextRun.glyphCount() && coreTextRun.indexAt(firstGlyphAfterCluster) >= clusterStart && coreTextRun.indexAt(firstGlyphAfterCluster) < clusterEnd) {
                        clusterWidth += m_adjustedAdvances[offsetIntoAdjustedGlyphs + firstGlyphAfterCluster].width;
                        firstGlyphAfterCluster++;
                    }
                }
                if (x <= clusterWidth / 2)
                    return coreTextRun.stringLocation() + (m_run.ltr() ? clusterStart : clusterEnd);
                else
                    return coreTextRun.stringLocation() + (m_run.ltr() ? clusterEnd : clusterStart);
            }
            x -= adjustedAdvance;
        }
        offsetIntoAdjustedGlyphs += coreTextRun.glyphCount();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void CoreTextController::collectCoreTextRuns()
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
        collectCoreTextRunsForCharacters(&hyphen, 1, m_end - 1, m_font.glyphDataForCharacter(hyphen, false).fontData);
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
            int itemStart = m_run.rtl() ? index + 1 : indexOfFontTransition;
            int itemLength = m_run.rtl() ? indexOfFontTransition - index : index - indexOfFontTransition;
            collectCoreTextRunsForCharacters((isSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, glyphData.glyph ? glyphData.fontData : 0);
            indexOfFontTransition = index;
        }
    }

    int itemLength = m_run.rtl() ? indexOfFontTransition + 1 : m_end - indexOfFontTransition - (hasTrailingSoftHyphen ? 1 : 0);
    if (itemLength) {
        int itemStart = m_run.rtl() ? 0 : indexOfFontTransition;
        collectCoreTextRunsForCharacters((nextIsSmallCaps ? m_smallCapsBuffer.data() : cp) + itemStart, itemLength, itemStart, nextGlyphData.glyph ? nextGlyphData.fontData : 0);
    }

    if (hasTrailingSoftHyphen && m_run.ltr())
        collectCoreTextRunsForCharacters(&hyphen, 1, m_end - 1, m_font.glyphDataForCharacter(hyphen, false).fontData);
}

void CoreTextController::advance(unsigned offset, GlyphBuffer* glyphBuffer)
{
    // FIXME: For offsets falling inside a ligature, we should advance only as far as the appropriate "ligature caret"
    // or divide the width of the ligature by the number of offsets it encompasses and make an advance proportional
    // to the offsets into the ligature. However, Core Text does not expose a low-level API for
    // directly finding out how many characters a ligature encompasses (the "attachment count").
    if (static_cast<int>(offset) > m_end)
        offset = m_end;

    if (offset <= m_currentCharacter)
        return;

    m_currentCharacter = offset;

    size_t runCount = m_coreTextRuns.size();

    bool ltr = m_run.ltr();

    unsigned k = ltr ? m_numGlyphsSoFar : m_adjustedGlyphs.size() - 1 - m_numGlyphsSoFar;
    while (m_currentRun < runCount) {
        const CoreTextRun& coreTextRun = m_coreTextRuns[ltr ? m_currentRun : runCount - 1 - m_currentRun];
        size_t glyphCount = coreTextRun.glyphCount();
        unsigned g = ltr ? m_glyphInCurrentRun : glyphCount - 1 - m_glyphInCurrentRun;
        while (m_glyphInCurrentRun < glyphCount) {
            if (coreTextRun.indexAt(g) + coreTextRun.stringLocation() >= m_currentCharacter)
                return;
            CGSize adjustedAdvance = m_adjustedAdvances[k];
            if (glyphBuffer)
                glyphBuffer->add(m_adjustedGlyphs[k], coreTextRun.fontData(), adjustedAdvance);
            m_runWidthSoFar += adjustedAdvance.width;
            m_numGlyphsSoFar++;
            m_glyphInCurrentRun++;
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

void CoreTextController::collectCoreTextRunsForCharacters(const UChar* cp, unsigned length, unsigned stringLocation, const SimpleFontData* fontData)
{
    if (!fontData) {
        // Create a run of missing glyphs from the primary font.
        m_coreTextRuns.append(CoreTextRun(m_font.primaryFont(), cp, stringLocation, length, m_run.ltr()));
        return;
    }

    if (m_fallbackFonts && fontData != m_font.primaryFont())
        m_fallbackFonts->add(fontData);

    RetainPtr<CFStringRef> string(AdoptCF, CFStringCreateWithCharactersNoCopy(NULL, cp, length, kCFAllocatorNull));

    RetainPtr<CFAttributedStringRef> attributedString(AdoptCF, CFAttributedStringCreate(NULL, string.get(), fontData->getCFStringAttributes(m_font.fontDescription().textRenderingMode())));

    RetainPtr<CTTypesetterRef> typesetter;

    if (!m_mayUseNaturalWritingDirection || m_run.directionalOverride()) {
        static const void* optionKeys[] = { kCTTypesetterOptionForcedEmbeddingLevel };
        static const void* ltrOptionValues[] = { kCFBooleanFalse };
        static const void* rtlOptionValues[] = { kCFBooleanTrue };
        static CFDictionaryRef ltrTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, ltrOptionValues, sizeof(optionKeys) / sizeof(*optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        static CFDictionaryRef rtlTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, rtlOptionValues, sizeof(optionKeys) / sizeof(*optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        typesetter.adoptCF(CTTypesetterCreateWithAttributedStringAndOptions(attributedString.get(), m_run.ltr() ? ltrTypesetterOptions : rtlTypesetterOptions));
    } else
        typesetter.adoptCF(CTTypesetterCreateWithAttributedString(attributedString.get()));

    RetainPtr<CTLineRef> line(AdoptCF, CTTypesetterCreateLine(typesetter.get(), CFRangeMake(0, 0)));

    CFArrayRef runArray = CTLineGetGlyphRuns(line.get());

    CFIndex runCount = CFArrayGetCount(runArray);

    for (CFIndex r = 0; r < runCount; r++) {
        CTRunRef ctRun = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runArray, r));
        ASSERT(CFGetTypeID(ctRun) == CTRunGetTypeID());
        m_coreTextRuns.append(CoreTextRun(ctRun, fontData, cp, stringLocation, length));
    }
}

void CoreTextController::adjustGlyphsAndAdvances()
{
    size_t runCount = m_coreTextRuns.size();
    for (size_t r = 0; r < runCount; ++r) {
        const CoreTextRun& coreTextRun = m_coreTextRuns[r];
        unsigned glyphCount = coreTextRun.glyphCount();
        const SimpleFontData* fontData = coreTextRun.fontData();

        Vector<CGGlyph, 256> glyphsVector;
        const CGGlyph* glyphs;

        Vector<CGSize, 256> advancesVector;
        const CGSize* advances;

        if (coreTextRun.ctRun()) {
            glyphs = CTRunGetGlyphsPtr(coreTextRun.ctRun());
            if (!glyphs) {
                glyphsVector.grow(glyphCount);
                CTRunGetGlyphs(coreTextRun.ctRun(), CFRangeMake(0, 0), glyphsVector.data());
                glyphs = glyphsVector.data();
            }

            advances = CTRunGetAdvancesPtr(coreTextRun.ctRun());
            if (!advances) {
                advancesVector.grow(glyphCount);
                CTRunGetAdvances(coreTextRun.ctRun(), CFRangeMake(0, 0), advancesVector.data());
                advances = advancesVector.data();
            }
        } else {
            // Synthesize a run of missing glyphs.
            glyphsVector.fill(0, glyphCount);
            glyphs = glyphsVector.data();
            advancesVector.fill(CGSizeMake(fontData->widthForGlyph(0), 0), glyphCount);
            advances = advancesVector.data();
        }

        bool lastRun = r + 1 == runCount;
        const UChar* cp = coreTextRun.characters();
        CGFloat roundedSpaceWidth = roundCGFloat(fontData->spaceWidth());
        bool roundsAdvances = !m_font.isPrinterFont() && fontData->platformData().roundsGlyphAdvances();
        bool hasExtraSpacing = (m_font.letterSpacing() || m_font.wordSpacing() || m_padding) && !m_run.spacingDisabled();


        for (unsigned i = 0; i < glyphCount; i++) {
            CFIndex characterIndex = coreTextRun.indexAt(i);
            UChar ch = *(cp + characterIndex);
            bool lastGlyph = lastRun && i + 1 == glyphCount;
            UChar nextCh;
            if (lastGlyph)
                nextCh = ' ';
            else if (i + 1 < glyphCount)
                nextCh = *(cp + coreTextRun.indexAt(i + 1));
            else
                nextCh = *(m_coreTextRuns[r + 1].characters() + m_coreTextRuns[r + 1].indexAt(0));

            bool treatAsSpace = Font::treatAsSpace(ch);
            CGGlyph glyph = treatAsSpace ? fontData->spaceGlyph() : glyphs[i];
            CGSize advance = treatAsSpace ? CGSizeMake(fontData->spaceWidth(), advances[i].height) : advances[i];

            if (ch == '\t' && m_run.allowTabs()) {
                float tabWidth = m_font.tabWidth();
                advance.width = tabWidth - fmodf(m_run.xPos() + m_totalWidth, tabWidth);
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
                            advance.width += m_padPerSpace;
                            m_padding -= m_padPerSpace;
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
                CGFloat totalWidth = m_totalWidth + advance.width;
                CGFloat extraWidth = ceilCGFloat(totalWidth) - totalWidth;
                if (m_run.ltr())
                    advance.width += extraWidth;
                else {
                    m_totalWidth += extraWidth;
                    if (m_lastRoundingGlyph)
                        m_adjustedAdvances[m_lastRoundingGlyph - 1].width += extraWidth;
                    else
                        m_finalRoundingWidth = extraWidth;
                    m_lastRoundingGlyph = m_adjustedAdvances.size() + 1;
                }
            }

            m_totalWidth += advance.width;
            advance.height *= -1;
            m_adjustedAdvances.append(advance);
            m_adjustedGlyphs.append(glyph);
        }
    }
}

} // namespace WebCore

#endif // USE(CORE_TEXT)
