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

#include "CharacterProperties.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "RenderBlock.h"
#include "RenderText.h"
#include "TextBreakIterator.h"
#include "TextRun.h"
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS)
#include <CoreText/CoreText.h>
#endif

#if PLATFORM(MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace WebCore {

class TextLayout {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static bool isNeeded(RenderText& text, const FontCascade& font)
    {
        TextRun run = RenderBlock::constructTextRun(&text, font, &text, text.style());
        return font.codePath(run) == FontCascade::Complex;
    }

    TextLayout(RenderText& text, const FontCascade& font, float xPos)
        : m_font(font)
        , m_run(constructTextRun(text, font, xPos))
        , m_controller(std::make_unique<ComplexTextController>(m_font, m_run, true))
    {
    }

    float width(unsigned from, unsigned len, HashSet<const Font*>* fallbackFonts)
    {
        m_controller->advance(from, 0, ByWholeGlyphs, fallbackFonts);
        float beforeWidth = m_controller->runWidthSoFar();
        if (m_font.wordSpacing() && from && FontCascade::treatAsSpace(m_run[from]))
            beforeWidth += m_font.wordSpacing();
        m_controller->advance(from + len, 0, ByWholeGlyphs, fallbackFonts);
        float afterWidth = m_controller->runWidthSoFar();
        return afterWidth - beforeWidth;
    }

private:
    static TextRun constructTextRun(RenderText& text, const FontCascade& font, float xPos)
    {
        TextRun run = RenderBlock::constructTextRun(&text, font, &text, text.style());
        run.setCharactersLength(text.textLength());
        ASSERT(run.charactersLength() >= run.length());
        run.setXPos(xPos);
        return run;
    }

    // ComplexTextController has only references to its FontCascade and TextRun so they must be kept alive here.
    FontCascade m_font;
    TextRun m_run;
    std::unique_ptr<ComplexTextController> m_controller;
};

void TextLayoutDeleter::operator()(TextLayout* layout) const
{
    delete layout;
}

std::unique_ptr<TextLayout, TextLayoutDeleter> FontCascade::createLayout(RenderText& text, float xPos, bool collapseWhiteSpace) const
{
    if (!collapseWhiteSpace || !TextLayout::isNeeded(text, *this))
        return nullptr;
    return std::unique_ptr<TextLayout, TextLayoutDeleter>(new TextLayout(text, *this, xPos));
}

float FontCascade::width(TextLayout& layout, unsigned from, unsigned len, HashSet<const Font*>* fallbackFonts)
{
    return layout.width(from, len, fallbackFonts);
}

ComplexTextController::ComplexTextController(const FontCascade& font, const TextRun& run, bool mayUseNaturalWritingDirection, HashSet<const Font*>* fallbackFonts, bool forTextEmphasis)
    : m_font(font)
    , m_run(run)
    , m_isLTROnly(true)
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
    , m_fallbackFonts(fallbackFonts)
    , m_minGlyphBoundingBoxX(std::numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxX(std::numeric_limits<float>::min())
    , m_minGlyphBoundingBoxY(std::numeric_limits<float>::max())
    , m_maxGlyphBoundingBoxY(std::numeric_limits<float>::min())
    , m_lastRoundingGlyph(0)
{
    if (!m_expansion)
        m_expansionPerOpportunity = 0;
    else {
        unsigned expansionOpportunityCount = FontCascade::expansionOpportunityCount(m_run.text(), m_run.ltr() ? LTR : RTL, run.expansionBehavior()).first;

        if (!expansionOpportunityCount)
            m_expansionPerOpportunity = 0;
        else
            m_expansionPerOpportunity = m_expansion / expansionOpportunityCount;
    }

    collectComplexTextRuns();
    adjustGlyphsAndAdvances();

    if (!m_isLTROnly) {
        m_runIndices.reserveInitialCapacity(m_complexTextRuns.size());

        m_glyphCountFromStartToIndex.reserveInitialCapacity(m_complexTextRuns.size());
        unsigned glyphCountSoFar = 0;
        for (unsigned i = 0; i < m_complexTextRuns.size(); ++i) {
            m_glyphCountFromStartToIndex.uncheckedAppend(glyphCountSoFar);
            glyphCountSoFar += m_complexTextRuns[i]->glyphCount();
        }
    }

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
            size_t index = offsetIntoAdjustedGlyphs + j;
            CGFloat adjustedAdvance = m_adjustedAdvances[index].width;
            if (!index)
                adjustedAdvance += complexTextRun.initialAdvance().width;
            if (x < adjustedAdvance) {
                CFIndex hitGlyphStart = complexTextRun.indexAt(j);
                CFIndex hitGlyphEnd;
                if (m_run.ltr())
                    hitGlyphEnd = std::max<CFIndex>(hitGlyphStart, j + 1 < complexTextRun.glyphCount() ? complexTextRun.indexAt(j + 1) : static_cast<CFIndex>(complexTextRun.indexEnd()));
                else
                    hitGlyphEnd = std::max<CFIndex>(hitGlyphStart, j > 0 ? complexTextRun.indexAt(j - 1) : static_cast<CFIndex>(complexTextRun.indexEnd()));

                // FIXME: Instead of dividing the glyph's advance equally between the characters, this
                // could use the glyph's "ligature carets". However, there is no Core Text API to get the
                // ligature carets.
                CFIndex hitIndex = hitGlyphStart + (hitGlyphEnd - hitGlyphStart) * (m_run.ltr() ? x / adjustedAdvance : 1 - x / adjustedAdvance);
                int stringLength = complexTextRun.stringLength();
                TextBreakIterator* cursorPositionIterator = cursorMovementIterator(StringView(complexTextRun.characters(), stringLength));
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

// FIXME: We should consider reimplementing this function using ICU to advance by grapheme.
// The current implementation only considers explicitly emoji sequences and emoji variations.
static bool advanceByCombiningCharacterSequence(const UChar*& iterator, const UChar* end, UChar32& baseCharacter, unsigned& markCount)
{
    ASSERT(iterator < end);

    markCount = 0;

    unsigned i = 0;
    unsigned remainingCharacters = end - iterator;
    U16_NEXT(iterator, i, remainingCharacters, baseCharacter);
    iterator = iterator + i;
    if (U_IS_SURROGATE(baseCharacter))
        return false;

    // Consume marks.
    bool sawEmojiGroupCandidate = isEmojiGroupCandidate(baseCharacter);
    bool sawJoiner = false;
    while (iterator < end) {
        UChar32 nextCharacter;
        int markLength = 0;
        bool shouldContinue = false;
        U16_NEXT(iterator, markLength, end - iterator, nextCharacter);

        if (isVariationSelector(nextCharacter) || isEmojiModifier(nextCharacter))
            shouldContinue = true;

        if (sawJoiner && isEmojiGroupCandidate(nextCharacter))
            shouldContinue = true;

        sawJoiner = false;
        if (sawEmojiGroupCandidate && nextCharacter == zeroWidthJoiner) {
            sawJoiner = true;
            shouldContinue = true;
        }
        
        if (!shouldContinue && !(U_GET_GC_MASK(nextCharacter) & U_GC_M_MASK))
            break;

        markCount += markLength;
        iterator += markLength;
    }

    return true;
}

// FIXME: Capitalization is language-dependent and context-dependent and should operate on grapheme clusters instead of codepoints.
static inline Optional<UChar32> capitalized(UChar32 baseCharacter)
{
    if (U_GET_GC_MASK(baseCharacter) & U_GC_M_MASK)
        return Nullopt;

    UChar32 uppercaseCharacter = u_toupper(baseCharacter);
    ASSERT(uppercaseCharacter == baseCharacter || uppercaseCharacter <= 0xFFFF);
    if (uppercaseCharacter != baseCharacter)
        return uppercaseCharacter;
    return Nullopt;
}

void ComplexTextController::collectComplexTextRuns()
{
    if (!m_end)
        return;

    // We break up glyph run generation for the string by Font.
    const UChar* cp;

    if (m_run.is8Bit()) {
        String stringFor8BitRun = String::make16BitFrom8BitSource(m_run.characters8(), m_run.length());
        cp = stringFor8BitRun.characters16();
        m_stringsFor8BitRuns.append(stringFor8BitRun);
    } else
        cp = m_run.characters16();

    auto fontVariantCaps = m_font.fontDescription().variantCaps();
    bool engageAllSmallCapsProcessing = fontVariantCaps == FontVariantCaps::AllSmall || fontVariantCaps == FontVariantCaps::AllPetite;
    bool engageSmallCapsProcessing = engageAllSmallCapsProcessing || fontVariantCaps == FontVariantCaps::Small || fontVariantCaps == FontVariantCaps::Petite;

    if (engageAllSmallCapsProcessing || engageSmallCapsProcessing)
        m_smallCapsBuffer.resize(m_end);

    unsigned indexOfFontTransition = 0;
    const UChar* curr = cp;
    const UChar* end = cp + m_end;

    const Font* font;
    const Font* nextFont;
    const Font* synthesizedFont = nullptr;
    const Font* smallSynthesizedFont = nullptr;

    unsigned markCount;
    UChar32 baseCharacter;
    if (!advanceByCombiningCharacterSequence(curr, end, baseCharacter, markCount))
        return;

    nextFont = m_font.fontForCombiningCharacterSequence(cp, curr - cp);

    bool isSmallCaps = false;
    bool nextIsSmallCaps = false;

    auto capitalizedBase = capitalized(baseCharacter);
    if (nextFont && nextFont != Font::systemFallback() && (capitalizedBase || engageAllSmallCapsProcessing)
        && !nextFont->variantCapsSupportsCharacterForSynthesis(fontVariantCaps, baseCharacter)) {
        synthesizedFont = &nextFont->noSynthesizableFeaturesFont();
        smallSynthesizedFont = synthesizedFont->smallCapsFont(m_font.fontDescription());
        m_smallCapsBuffer[0] = capitalizedBase ? capitalizedBase.value() : cp[0];
        for (unsigned i = 1; cp + i < curr; ++i)
            m_smallCapsBuffer[i] = cp[i];
        nextIsSmallCaps = true;
    }

    while (curr < end) {
        font = nextFont;
        isSmallCaps = nextIsSmallCaps;
        unsigned index = curr - cp;

        if (!advanceByCombiningCharacterSequence(curr, end, baseCharacter, markCount))
            return;

        if (synthesizedFont) {
            if (auto capitalizedBase = capitalized(baseCharacter)) {
                m_smallCapsBuffer[index] = capitalizedBase.value();
                for (unsigned i = 0; i < markCount; ++i)
                    m_smallCapsBuffer[index + i + 1] = cp[index + i + 1];
                nextIsSmallCaps = true;
            } else {
                if (engageAllSmallCapsProcessing) {
                    for (unsigned i = 0; i < curr - cp - index; ++i)
                        m_smallCapsBuffer[index + i] = cp[index + i];
                }
                nextIsSmallCaps = engageAllSmallCapsProcessing;
            }
        }

        if (baseCharacter == zeroWidthJoiner)
            nextFont = font;
        else
            nextFont = m_font.fontForCombiningCharacterSequence(cp + index, curr - cp - index);

        capitalizedBase = capitalized(baseCharacter);
        if (!synthesizedFont && nextFont && nextFont != Font::systemFallback() && (capitalizedBase || engageAllSmallCapsProcessing)
            && !nextFont->variantCapsSupportsCharacterForSynthesis(fontVariantCaps, baseCharacter)) {
            // Rather than synthesize each character individually, we should synthesize the entire "run" if any character requires synthesis.
            synthesizedFont = &nextFont->noSynthesizableFeaturesFont();
            smallSynthesizedFont = synthesizedFont->smallCapsFont(m_font.fontDescription());
            nextIsSmallCaps = true;
            curr = cp + indexOfFontTransition;
            continue;
        }

        if (nextFont != font || nextIsSmallCaps != isSmallCaps) {
            unsigned itemLength = index - indexOfFontTransition;
            if (itemLength) {
                unsigned itemStart = indexOfFontTransition;
                if (synthesizedFont) {
                    if (isSmallCaps)
                        collectComplexTextRunsForCharacters(m_smallCapsBuffer.data() + itemStart, itemLength, itemStart, smallSynthesizedFont);
                    else
                        collectComplexTextRunsForCharacters(cp + itemStart, itemLength, itemStart, synthesizedFont);
                } else
                    collectComplexTextRunsForCharacters(cp + itemStart, itemLength, itemStart, font);
                if (nextFont != font) {
                    synthesizedFont = nullptr;
                    smallSynthesizedFont = nullptr;
                    nextIsSmallCaps = false;
                }
            }
            indexOfFontTransition = index;
        }
    }

    unsigned itemLength = m_end - indexOfFontTransition;
    if (itemLength) {
        unsigned itemStart = indexOfFontTransition;
        if (synthesizedFont) {
            if (nextIsSmallCaps)
                collectComplexTextRunsForCharacters(m_smallCapsBuffer.data() + itemStart, itemLength, itemStart, smallSynthesizedFont);
            else
                collectComplexTextRunsForCharacters(cp + itemStart, itemLength, itemStart, synthesizedFont);
        } else
            collectComplexTextRunsForCharacters(cp + itemStart, itemLength, itemStart, nextFont);
    }

    if (!m_run.ltr())
        m_complexTextRuns.reverse();
}

CFIndex ComplexTextController::ComplexTextRun::indexAt(size_t i) const
{
    ASSERT(i < m_glyphCount);

    return m_coreTextIndices[i];
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

unsigned ComplexTextController::indexOfCurrentRun(unsigned& leftmostGlyph)
{
    leftmostGlyph = 0;
    
    size_t runCount = m_complexTextRuns.size();
    if (m_currentRun >= runCount)
        return runCount;

    if (m_isLTROnly) {
        for (unsigned i = 0; i < m_currentRun; ++i)
            leftmostGlyph += m_complexTextRuns[i]->glyphCount();
        return m_currentRun;
    }

    if (m_runIndices.isEmpty()) {
        unsigned firstRun = 0;
        unsigned firstRunOffset = stringBegin(*m_complexTextRuns[0]);
        for (unsigned i = 1; i < runCount; ++i) {
            unsigned offset = stringBegin(*m_complexTextRuns[i]);
            if (offset < firstRunOffset) {
                firstRun = i;
                firstRunOffset = offset;
            }
        }
        m_runIndices.uncheckedAppend(firstRun);
    }

    while (m_runIndices.size() <= m_currentRun) {
        unsigned offset = stringEnd(*m_complexTextRuns[m_runIndices.last()]);

        for (unsigned i = 0; i < runCount; ++i) {
            if (offset == stringBegin(*m_complexTextRuns[i])) {
                m_runIndices.uncheckedAppend(i);
                break;
            }
        }
    }

    unsigned currentRunIndex = m_runIndices[m_currentRun];
    leftmostGlyph = m_glyphCountFromStartToIndex[currentRunIndex];
    return currentRunIndex;
}

unsigned ComplexTextController::incrementCurrentRun(unsigned& leftmostGlyph)
{
    if (m_isLTROnly) {
        leftmostGlyph += m_complexTextRuns[m_currentRun++]->glyphCount();
        return m_currentRun;
    }

    m_currentRun++;
    leftmostGlyph = 0;
    return indexOfCurrentRun(leftmostGlyph);
}

void ComplexTextController::advance(unsigned offset, GlyphBuffer* glyphBuffer, GlyphIterationStyle iterationStyle, HashSet<const Font*>* fallbackFonts)
{
    if (static_cast<int>(offset) > m_end)
        offset = m_end;

    if (offset <= m_currentCharacter) {
        m_runWidthSoFar = m_leadingExpansion;
        m_numGlyphsSoFar = 0;
        m_currentRun = 0;
        m_glyphInCurrentRun = 0;
        m_characterInCurrentGlyph = 0;
    }

    m_currentCharacter = offset;

    size_t runCount = m_complexTextRuns.size();

    unsigned leftmostGlyph = 0;
    unsigned currentRunIndex = indexOfCurrentRun(leftmostGlyph);
    while (m_currentRun < runCount) {
        const ComplexTextRun& complexTextRun = *m_complexTextRuns[currentRunIndex];
        bool ltr = complexTextRun.isLTR();
        size_t glyphCount = complexTextRun.glyphCount();
        unsigned g = ltr ? m_glyphInCurrentRun : glyphCount - 1 - m_glyphInCurrentRun;
        unsigned k = leftmostGlyph + g;
        if (fallbackFonts && &complexTextRun.font() != &m_font.primaryFont())
            fallbackFonts->add(&complexTextRun.font());

        // We must store the initial advance for the first glyph we are going to draw.
        // When leftmostGlyph is 0, it represents the first glyph to draw, taking into
        // account the text direction.
        if (glyphBuffer && !leftmostGlyph) {
            glyphBuffer->setInitialAdvance(complexTextRun.initialAdvance());
            glyphBuffer->setLeadingExpansion(m_leadingExpansion);
        }

        while (m_glyphInCurrentRun < glyphCount) {
            unsigned glyphStartOffset = complexTextRun.indexAt(g);
            unsigned glyphEndOffset;
            if (complexTextRun.isMonotonic()) {
                if (ltr)
                    glyphEndOffset = std::max<unsigned>(glyphStartOffset, static_cast<unsigned>(g + 1 < glyphCount ? complexTextRun.indexAt(g + 1) : complexTextRun.indexEnd()));
                else
                    glyphEndOffset = std::max<unsigned>(glyphStartOffset, static_cast<unsigned>(g > 0 ? complexTextRun.indexAt(g - 1) : complexTextRun.indexEnd()));
            } else
                glyphEndOffset = complexTextRun.endOffsetAt(g);

            CGSize adjustedAdvance = m_adjustedAdvances[k];

            if (glyphStartOffset + complexTextRun.stringLocation() >= m_currentCharacter)
                return;

            if (glyphBuffer && !m_characterInCurrentGlyph)
                glyphBuffer->add(m_adjustedGlyphs[k], &complexTextRun.font(), adjustedAdvance, complexTextRun.indexAt(m_glyphInCurrentRun));

            unsigned oldCharacterInCurrentGlyph = m_characterInCurrentGlyph;
            m_characterInCurrentGlyph = std::min(m_currentCharacter - complexTextRun.stringLocation(), glyphEndOffset) - glyphStartOffset;
            // FIXME: Instead of dividing the glyph's advance equally between the characters, this
            // could use the glyph's "ligature carets". However, there is no Core Text API to get the
            // ligature carets.
            if (glyphStartOffset == glyphEndOffset) {
                // When there are multiple glyphs per character we need to advance by the full width of the glyph.
                ASSERT(m_characterInCurrentGlyph == oldCharacterInCurrentGlyph);
                m_runWidthSoFar += adjustedAdvance.width;
            } else if (iterationStyle == ByWholeGlyphs) {
                if (!oldCharacterInCurrentGlyph)
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
        currentRunIndex = incrementCurrentRun(leftmostGlyph);
        m_glyphInCurrentRun = 0;
    }
    if (!m_run.ltr() && m_numGlyphsSoFar == m_adjustedAdvances.size())
        m_runWidthSoFar += m_finalRoundingWidth;
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
    if (isAfterExpansion)
        expandLeft = false;
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

void ComplexTextController::adjustGlyphsAndAdvances()
{
    bool afterExpansion = (m_run.expansionBehavior() & LeadingExpansionMask) == ForbidLeadingExpansion;
    CGFloat widthSinceLastCommit = 0;
    size_t runCount = m_complexTextRuns.size();
    bool hasExtraSpacing = (m_font.letterSpacing() || m_font.wordSpacing() || m_expansion) && !m_run.spacingDisabled();
    bool runForcesLeadingExpansion = (m_run.expansionBehavior() & LeadingExpansionMask) == ForceLeadingExpansion;
    bool runForcesTrailingExpansion = (m_run.expansionBehavior() & TrailingExpansionMask) == ForceTrailingExpansion;
    bool runForbidsLeadingExpansion = (m_run.expansionBehavior() & LeadingExpansionMask) == ForbidLeadingExpansion;
    bool runForbidsTrailingExpansion = (m_run.expansionBehavior() & TrailingExpansionMask) == ForbidTrailingExpansion;
    // We are iterating in glyph order, not string order. Compare this to WidthIterator::advanceInternal()
    for (size_t r = 0; r < runCount; ++r) {
        ComplexTextRun& complexTextRun = *m_complexTextRuns[r];
        unsigned glyphCount = complexTextRun.glyphCount();
        const Font& font = complexTextRun.font();

        // Represent the initial advance for a text run by adjusting the advance
        // of the last glyph of the previous text run in the glyph buffer.
        if (r && m_adjustedAdvances.size()) {
            CGSize previousAdvance = m_adjustedAdvances.last();
            previousAdvance.width += complexTextRun.initialAdvance().width;
            previousAdvance.height -= complexTextRun.initialAdvance().height;
            m_adjustedAdvances[m_adjustedAdvances.size() - 1] = previousAdvance;
        }
        widthSinceLastCommit += complexTextRun.initialAdvance().width;

        if (!complexTextRun.isLTR())
            m_isLTROnly = false;

        const CGGlyph* glyphs = complexTextRun.glyphs();
        const CGSize* advances = complexTextRun.advances();

        bool lastRun = r + 1 == runCount;
        float spaceWidth = font.spaceWidth() - font.syntheticBoldOffset();
        CGFloat roundedSpaceWidth = std::round(spaceWidth);
        const UChar* cp = complexTextRun.characters();
        CGPoint glyphOrigin = CGPointZero;
        CFIndex lastCharacterIndex = m_run.ltr() ? std::numeric_limits<CFIndex>::min() : std::numeric_limits<CFIndex>::max();
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

            bool treatAsSpace = FontCascade::treatAsSpace(ch);
            CGGlyph glyph = treatAsSpace ? font.spaceGlyph() : glyphs[i];
            CGSize advance = treatAsSpace ? CGSizeMake(spaceWidth, advances[i].height) : advances[i];

            if (ch == '\t' && m_run.allowTabs())
                advance.width = m_font.tabWidth(font, m_run.tabSize(), m_run.xPos() + m_totalWidth + widthSinceLastCommit);
            else if (FontCascade::treatAsZeroWidthSpace(ch) && !treatAsSpace) {
                advance.width = 0;
                glyph = font.spaceGlyph();
            }

            float roundedAdvanceWidth = roundf(advance.width);
            advance.width += font.syntheticBoldOffset();

 
            // We special case spaces in two ways when applying word rounding. 
            // First, we round spaces to an adjusted width in all fonts. 
            // Second, in fixed-pitch fonts we ensure that all glyphs that 
            // match the width of the space glyph have the same width as the space glyph. 
            if (m_run.applyWordRounding() && roundedAdvanceWidth == roundedSpaceWidth && (font.pitch() == FixedPitch || glyph == font.spaceGlyph()))
                advance.width = font.adjustedSpaceWidth();

            if (hasExtraSpacing) {
                // If we're a glyph with an advance, add in letter-spacing.
                // That way we weed out zero width lurkers.  This behavior matches the fast text code path.
                if (advance.width)
                    advance.width += m_font.letterSpacing();

                unsigned characterIndexInRun = characterIndex + complexTextRun.stringLocation();
                bool isFirstCharacter = !(characterIndex + complexTextRun.stringLocation());
                bool isLastCharacter = static_cast<unsigned>(characterIndexInRun + 1) == m_run.length() || (U16_IS_LEAD(ch) && static_cast<unsigned>(characterIndexInRun + 2) == m_run.length() && U16_IS_TRAIL(*(cp + characterIndex + 1)));

                bool forceLeadingExpansion = false; // On the left, regardless of m_run.ltr()
                bool forceTrailingExpansion = false; // On the right, regardless of m_run.ltr()
                bool forbidLeadingExpansion = false;
                bool forbidTrailingExpansion = false;
                if (runForcesLeadingExpansion)
                    forceLeadingExpansion = m_run.ltr() ? isFirstCharacter : isLastCharacter;
                if (runForcesTrailingExpansion)
                    forceTrailingExpansion = m_run.ltr() ? isLastCharacter : isFirstCharacter;
                if (runForbidsLeadingExpansion)
                    forbidLeadingExpansion = m_run.ltr() ? isFirstCharacter : isLastCharacter;
                if (runForbidsTrailingExpansion)
                    forbidTrailingExpansion = m_run.ltr() ? isLastCharacter : isFirstCharacter;
                // Handle justification and word-spacing.
                bool ideograph = FontCascade::isCJKIdeographOrSymbol(ch);
                if (treatAsSpace || ideograph || forceLeadingExpansion || forceTrailingExpansion) {
                    // Distribute the run's total expansion evenly over all expansion opportunities in the run.
                    if (m_expansion) {
                        bool expandLeft, expandRight;
                        std::tie(expandLeft, expandRight) = expansionLocation(ideograph, treatAsSpace, m_run.ltr(), afterExpansion, forbidLeadingExpansion, forbidTrailingExpansion, forceLeadingExpansion, forceTrailingExpansion);
                        float previousExpansion = m_expansion;
                        if (expandLeft) {
                            // Increase previous width
                            m_expansion -= m_expansionPerOpportunity;
                            float expansionAtThisOpportunity = !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                            m_totalWidth += expansionAtThisOpportunity;
                            if (m_adjustedAdvances.isEmpty())
                                m_leadingExpansion = expansionAtThisOpportunity;
                            else
                                m_adjustedAdvances.last().width += expansionAtThisOpportunity;
                            previousExpansion = m_expansion;
                        }
                        if (expandRight) {
                            m_expansion -= m_expansionPerOpportunity;
                            float expansionAtThisOpportunity = !m_run.applyWordRounding() ? m_expansionPerOpportunity : roundf(previousExpansion) - roundf(m_expansion);
                            advance.width += expansionAtThisOpportunity;
                            afterExpansion = true;
                        }
                    } else
                        afterExpansion = false;

                    // Account for word-spacing.
                    if (treatAsSpace && (ch != '\t' || !m_run.allowTabs()) && (characterIndex > 0 || r > 0) && m_font.wordSpacing())
                        advance.width += m_font.wordSpacing();
                } else
                    afterExpansion = false;
            }

            // Apply rounding hacks if needed.
            // We adjust the width of the last character of a "word" to ensure an integer width. 
            // Force characters that are used to determine word boundaries for the rounding hack 
            // to be integer width, so the following words will start on an integer boundary. 
            if (m_run.applyWordRounding() && FontCascade::isRoundingHackCharacter(ch)) 
                advance.width = std::ceil(advance.width);

            // Check to see if the next character is a "rounding hack character", if so, adjust the 
            // width so that the total run width will be on an integer boundary.
            bool needsRoundingForCharacter = m_run.applyWordRounding() && !lastGlyph && FontCascade::isRoundingHackCharacter(nextCh);
            if (needsRoundingForCharacter || (m_run.applyRunRounding() && lastGlyph)) {
                CGFloat totalWidth = widthSinceLastCommit + advance.width; 
                widthSinceLastCommit = std::ceil(totalWidth);
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
            if (m_forTextEmphasis && (!FontCascade::canReceiveTextEmphasis(ch) || (U_GET_GC_MASK(ch) & U_GC_M_MASK)))
                glyph = 0;

            advance.height *= -1;
            m_adjustedAdvances.append(advance);
            m_adjustedGlyphs.append(glyph);
            
            FloatRect glyphBounds = font.boundsForGlyph(glyph);
            glyphBounds.move(glyphOrigin.x, glyphOrigin.y);
            m_minGlyphBoundingBoxX = std::min(m_minGlyphBoundingBoxX, glyphBounds.x());
            m_maxGlyphBoundingBoxX = std::max(m_maxGlyphBoundingBoxX, glyphBounds.maxX());
            m_minGlyphBoundingBoxY = std::min(m_minGlyphBoundingBoxY, glyphBounds.y());
            m_maxGlyphBoundingBoxY = std::max(m_maxGlyphBoundingBoxY, glyphBounds.maxY());
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
