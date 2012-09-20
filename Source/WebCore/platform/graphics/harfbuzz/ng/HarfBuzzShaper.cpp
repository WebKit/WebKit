/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HarfBuzzShaper.h"

#include "Font.h"
#include "HarfBuzzNGFace.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include "hb-icu.h"
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

template<typename T>
class HarfBuzzScopedPtr {
public:
    typedef void (*DestroyFunction)(T*);

    HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
        : m_ptr(ptr)
        , m_destroy(destroy)
    {
        ASSERT(m_destroy);
    }
    ~HarfBuzzScopedPtr()
    {
        if (m_ptr)
            (*m_destroy)(m_ptr);
    }

    T* get() { return m_ptr; }
private:
    T* m_ptr;
    DestroyFunction m_destroy;
};

static inline float harfbuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

HarfBuzzShaper::HarfBuzzRun::HarfBuzzRun(const SimpleFontData* fontData, unsigned startIndex, unsigned numCharacters, TextDirection direction)
    : m_fontData(fontData)
    , m_startIndex(startIndex)
    , m_numCharacters(numCharacters)
    , m_direction(direction)
{
}

void HarfBuzzShaper::HarfBuzzRun::applyShapeResult(hb_buffer_t* harfbuzzBuffer)
{
    m_numGlyphs = hb_buffer_get_length(harfbuzzBuffer);
    m_glyphs.resize(m_numGlyphs);
    m_advances.resize(m_numGlyphs);
    m_glyphToCharacterIndexes.resize(m_numGlyphs);
    m_offsets.resize(m_numGlyphs);

    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(harfbuzzBuffer, 0);
    for (unsigned i = 0; i < m_numGlyphs; ++i)
        m_glyphToCharacterIndexes[i] = infos[i].cluster;
}

void HarfBuzzShaper::HarfBuzzRun::setGlyphAndPositions(unsigned index, uint16_t glyphId, float advance, float offsetX, float offsetY)
{
    m_glyphs[index] = glyphId;
    m_advances[index] = advance;
    m_offsets[index] = FloatPoint(offsetX, offsetY);
}

int HarfBuzzShaper::HarfBuzzRun::characterIndexForXPosition(float targetX)
{
    ASSERT(targetX <= m_width);
    float currentX = 0;
    float currentAdvance = m_advances[0];
    unsigned glyphIndex = 0;

    // Sum up advances that belong to a character.
    while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1])
        currentAdvance += m_advances[++glyphIndex];
    currentAdvance = currentAdvance / 2.0;
    if (targetX <= currentAdvance)
        return rtl() ? m_numCharacters : 0;

    ++glyphIndex;
    while (glyphIndex < m_numGlyphs) {
        unsigned prevCharacterIndex = m_glyphToCharacterIndexes[glyphIndex - 1];
        float prevAdvance = currentAdvance;
        currentAdvance = m_advances[glyphIndex];
        while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1])
            currentAdvance += m_advances[++glyphIndex];
        currentAdvance = currentAdvance / 2.0;
        float nextX = currentX + prevAdvance + currentAdvance;
        if (currentX <= targetX && targetX <= nextX)
            return rtl() ? prevCharacterIndex : m_glyphToCharacterIndexes[glyphIndex];
        currentX = nextX;
        prevAdvance = currentAdvance;
        ++glyphIndex;
    }

    return rtl() ? 0 : m_numCharacters;
}

float HarfBuzzShaper::HarfBuzzRun::xPositionForOffset(unsigned offset)
{
    ASSERT(offset < m_numCharacters);
    unsigned glyphIndex = 0;
    float position = 0;
    if (rtl()) {
        while (glyphIndex < m_numGlyphs && m_glyphToCharacterIndexes[glyphIndex] > offset) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
        // For RTL, we need to return the right side boundary of the character.
        // Add advance of glyphs which are part of the character.
        while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1]) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
        position += m_advances[glyphIndex];
    } else {
        while (glyphIndex < m_numGlyphs && m_glyphToCharacterIndexes[glyphIndex] < offset) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
    }
    return position;
}

static void normalizeCharacters(const UChar* source, UChar* destination, int length)
{
    int position = 0;
    bool error = false;
    while (position < length) {
        UChar32 character;
        int nextPosition = position;
        U16_NEXT(source, nextPosition, length, character);
        // Don't normalize tabs as they are not treated as spaces for word-end.
        if (Font::treatAsSpace(character) && character != '\t')
            character = ' ';
        else if (Font::treatAsZeroWidthSpaceInComplexScript(character))
            character = zeroWidthSpace;
        U16_APPEND(destination, position, length, character, error);
        ASSERT_UNUSED(error, !error);
        position = nextPosition;
    }
}

HarfBuzzShaper::HarfBuzzShaper(const Font* font, const TextRun& run)
    : HarfBuzzShaperBase(font, run)
    , m_fromIndex(0)
    , m_toIndex(m_run.length())
{
    m_normalizedBuffer = adoptArrayPtr(new UChar[m_run.length() + 1]);
    m_normalizedBufferLength = m_run.length();
    normalizeCharacters(m_run.characters16(), m_normalizedBuffer.get(), m_normalizedBufferLength);
    setPadding(m_run.expansion());
    setFontFeatures();
}

HarfBuzzShaper::~HarfBuzzShaper()
{
}

void HarfBuzzShaper::setDrawRange(int from, int to)
{
    ASSERT(from >= 0);
    ASSERT(to <= m_run.length());
    m_fromIndex = from;
    m_toIndex = to;
}

void HarfBuzzShaper::setFontFeatures()
{
    FontFeatureSettings* settings = m_font->fontDescription().featureSettings();
    if (!settings)
        return;

    unsigned numFeatures = settings->size();
    m_features.resize(numFeatures);
    for (unsigned i = 0; i < numFeatures; ++i) {
        const UChar* tag = settings->at(i).tag().characters();
        m_features[i].tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
        m_features[i].value = settings->at(i).value();
        m_features[i].start = 0;
        m_features[i].end = static_cast<unsigned>(-1);
    }
}

bool HarfBuzzShaper::shape(GlyphBuffer* glyphBuffer)
{
    if (!collectHarfBuzzRuns())
        return false;

    m_totalWidth = 0;
    if (!shapeHarfBuzzRuns())
        return false;
    m_totalWidth = roundf(m_totalWidth);

    if (glyphBuffer)
        fillGlyphBuffer(glyphBuffer);

    return true;
}

FloatPoint HarfBuzzShaper::adjustStartPoint(const FloatPoint& point)
{
    return point + m_startOffset;
}

static const SimpleFontData* fontDataForCombiningCharacterSequence(const Font* font, const UChar* characters, size_t length)
{
    UErrorCode error = U_ZERO_ERROR;
    Vector<UChar, 4> normalizedCharacters(length);
    int32_t normalizedLength = unorm_normalize(characters, length, UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], length, &error);
    // Should fallback if we have an error or no composition occurred.
    if (U_FAILURE(error) || (static_cast<size_t>(normalizedLength) == length))
        return 0;
    UChar32 normalizedCharacter;
    size_t index = 0;
    U16_NEXT(&normalizedCharacters[0], index, static_cast<size_t>(normalizedLength), normalizedCharacter);
    return font->glyphDataForCharacter(normalizedCharacter, false).fontData;
}

bool HarfBuzzShaper::collectHarfBuzzRuns()
{
    const UChar* normalizedBufferEnd = m_normalizedBuffer.get() + m_normalizedBufferLength;
    SurrogatePairAwareTextIterator iterator(m_normalizedBuffer.get(), 0, m_normalizedBufferLength, m_normalizedBufferLength);
    UChar32 character;
    unsigned clusterLength = 0;
    unsigned startIndexOfCurrentRun = 0;
    if (!iterator.consume(character, clusterLength))
        return false;

    const SimpleFontData* nextFontData = m_font->glyphDataForCharacter(character, false).fontData;
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode nextScript = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return false;

    do {
        const UChar* currentCharacterPosition = iterator.characters();
        const SimpleFontData* currentFontData = nextFontData;
        UScriptCode currentScript = nextScript;

        for (iterator.advance(clusterLength); iterator.consume(character, clusterLength); iterator.advance(clusterLength)) {
            if (Font::treatAsZeroWidthSpace(character))
                continue;
            if (U_GET_GC_MASK(character) & U_GC_M_MASK) {
                int markLength = clusterLength;
                const UChar* markCharactersEnd = iterator.characters() + clusterLength;
                while (markCharactersEnd < normalizedBufferEnd) {
                    UChar32 nextCharacter;
                    int nextCharacterLength = 0;
                    U16_NEXT(markCharactersEnd, nextCharacterLength, normalizedBufferEnd - markCharactersEnd, nextCharacter);
                    if (!(U_GET_GC_MASK(nextCharacter) & U_GC_M_MASK))
                        break;
                    markLength += nextCharacterLength;
                    markCharactersEnd += nextCharacterLength;
                }
                nextFontData = fontDataForCombiningCharacterSequence(m_font, currentCharacterPosition, markCharactersEnd - currentCharacterPosition);
                if (nextFontData)
                    clusterLength = markLength;
                else
                    nextFontData = m_font->glyphDataForCharacter(character, false).fontData;
            } else
                nextFontData = m_font->glyphDataForCharacter(character, false).fontData;

            nextScript = uscript_getScript(character, &errorCode);
            if (U_FAILURE(errorCode))
                return false;
            if ((nextFontData != currentFontData) || ((currentScript != nextScript) && (nextScript != USCRIPT_INHERITED)))
                break;
            if (nextScript == USCRIPT_INHERITED)
                nextScript = currentScript;
        }
        unsigned numCharactersOfCurrentRun = iterator.currentCharacter() - startIndexOfCurrentRun;
        m_harfbuzzRuns.append(HarfBuzzRun::create(currentFontData, startIndexOfCurrentRun, numCharactersOfCurrentRun, m_run.direction()));
        currentFontData = nextFontData;
        startIndexOfCurrentRun = iterator.currentCharacter();
    } while (iterator.consume(character, clusterLength));

    return !m_harfbuzzRuns.isEmpty();
}

bool HarfBuzzShaper::shapeHarfBuzzRuns()
{
    HarfBuzzScopedPtr<hb_buffer_t> harfbuzzBuffer(hb_buffer_create(), hb_buffer_destroy);

    hb_buffer_set_unicode_funcs(harfbuzzBuffer.get(), hb_icu_get_unicode_funcs());
    if (m_run.rtl() || m_run.directionalOverride())
        hb_buffer_set_direction(harfbuzzBuffer.get(), m_run.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);

    for (unsigned i = 0; i < m_harfbuzzRuns.size(); ++i) {
        unsigned runIndex = m_run.rtl() ? m_harfbuzzRuns.size() - i - 1 : i;
        HarfBuzzRun* currentRun = m_harfbuzzRuns[runIndex].get();
        const SimpleFontData* currentFontData = currentRun->fontData();

        if (m_font->isSmallCaps() && u_islower(m_normalizedBuffer[currentRun->startIndex()])) {
            String upperText = String(m_normalizedBuffer.get() + currentRun->startIndex(), currentRun->numCharacters());
            upperText.makeUpper();
            currentFontData = m_font->glyphDataForCharacter(upperText[0], false, SmallCapsVariant).fontData;
            hb_buffer_add_utf16(harfbuzzBuffer.get(), upperText.characters(), currentRun->numCharacters(), 0, currentRun->numCharacters());
        } else
            hb_buffer_add_utf16(harfbuzzBuffer.get(), m_normalizedBuffer.get() + currentRun->startIndex(), currentRun->numCharacters(), 0, currentRun->numCharacters());

        FontPlatformData* platformData = const_cast<FontPlatformData*>(&currentFontData->platformData());
        HarfBuzzNGFace* face = platformData->harfbuzzFace();
        if (!face)
            return false;
        HarfBuzzScopedPtr<hb_font_t> harfbuzzFont(face->createFont(), hb_font_destroy);
        hb_shape(harfbuzzFont.get(), harfbuzzBuffer.get(), m_features.isEmpty() ? 0 : m_features.data(), m_features.size());

        currentRun->applyShapeResult(harfbuzzBuffer.get());
        setGlyphPositionsForHarfBuzzRun(currentRun, harfbuzzBuffer.get());

        hb_buffer_reset(harfbuzzBuffer.get());
        if (m_run.rtl() || m_run.directionalOverride())
            hb_buffer_set_direction(harfbuzzBuffer.get(), m_run.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    }

    return true;
}

void HarfBuzzShaper::setGlyphPositionsForHarfBuzzRun(HarfBuzzRun* currentRun, hb_buffer_t* harfbuzzBuffer)
{
    const SimpleFontData* currentFontData = currentRun->fontData();
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(harfbuzzBuffer, 0);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(harfbuzzBuffer, 0);

    unsigned numGlyphs = currentRun->numGlyphs();
    float totalAdvance = 0;

    // HarfBuzz returns the shaping result in visual order. We need not to flip for RTL.
    for (size_t i = 0; i < numGlyphs; ++i) {
        bool runEnd = i + 1 == numGlyphs;
        uint16_t glyph = glyphInfos[i].codepoint;
        float offsetX = harfbuzzPositionToFloat(glyphPositions[i].x_offset);
        float offsetY = -harfbuzzPositionToFloat(glyphPositions[i].y_offset);
        float advance = harfbuzzPositionToFloat(glyphPositions[i].x_advance);

        unsigned currentCharacterIndex = currentRun->startIndex() + glyphInfos[i].cluster;
        bool isClusterEnd = runEnd || glyphInfos[i].cluster != glyphInfos[i + 1].cluster;
        float spacing = 0;
        if (isClusterEnd && !Font::treatAsZeroWidthSpace(m_normalizedBuffer[currentCharacterIndex]))
            spacing += m_letterSpacing;

        if (isClusterEnd && isWordEnd(currentCharacterIndex))
            spacing += determineWordBreakSpacing();

        if (currentFontData->isZeroWidthSpaceGlyph(glyph)) {
            currentRun->setGlyphAndPositions(i, glyph, 0, 0, 0);
            continue;
        }

        advance += spacing;
        if (m_run.rtl()) {
            // In RTL, spacing should be added to left side of glyphs.
            offsetX += spacing;
            if (!isClusterEnd)
                offsetX += m_letterSpacing;
        }

        currentRun->setGlyphAndPositions(i, glyph, advance, offsetX, offsetY);

        totalAdvance += advance;
    }
    currentRun->setWidth(totalAdvance > 0.0 ? totalAdvance : 0.0);
    m_totalWidth += currentRun->width();
}

void HarfBuzzShaper::fillGlyphBufferFromHarfBuzzRun(GlyphBuffer* glyphBuffer, HarfBuzzRun* currentRun, FloatPoint& firstOffsetOfNextRun)
{
    FloatPoint* offsets = currentRun->offsets();
    uint16_t* glyphs = currentRun->glyphs();
    float* advances = currentRun->advances();
    unsigned numGlyphs = currentRun->numGlyphs();
    uint16_t* glyphToCharacterIndexes = currentRun->glyphToCharacterIndexes();

    for (unsigned i = 0; i < numGlyphs; ++i) {
        uint16_t currentCharacterIndex = currentRun->startIndex() + glyphToCharacterIndexes[i];
        FloatPoint& currentOffset = offsets[i];
        FloatPoint& nextOffset = (i == numGlyphs - 1) ? firstOffsetOfNextRun : offsets[i + 1];
        float glyphAdvanceX = advances[i] + nextOffset.x() - currentOffset.x();
        float glyphAdvanceY = nextOffset.y() - currentOffset.y();
        if (m_run.rtl()) {
            if (currentCharacterIndex > m_toIndex)
                m_startOffset.move(glyphAdvanceX, glyphAdvanceY);
            else if (currentCharacterIndex >= m_fromIndex)
                glyphBuffer->add(glyphs[i], currentRun->fontData(), createGlyphBufferAdvance(glyphAdvanceX, glyphAdvanceY));
        } else {
            if (currentCharacterIndex < m_fromIndex)
                m_startOffset.move(glyphAdvanceX, glyphAdvanceY);
            else if (currentCharacterIndex < m_toIndex)
                glyphBuffer->add(glyphs[i], currentRun->fontData(), createGlyphBufferAdvance(glyphAdvanceX, glyphAdvanceY));
        }
    }
}

void HarfBuzzShaper::fillGlyphBuffer(GlyphBuffer* glyphBuffer)
{
    unsigned numRuns = m_harfbuzzRuns.size();
    if (m_run.rtl()) {
        m_startOffset = m_harfbuzzRuns.last()->offsets()[0];
        for (int runIndex = numRuns - 1; runIndex >= 0; --runIndex) {
            HarfBuzzRun* currentRun = m_harfbuzzRuns[runIndex].get();
            FloatPoint firstOffsetOfNextRun = !runIndex ? FloatPoint() : m_harfbuzzRuns[runIndex - 1]->offsets()[0];
            fillGlyphBufferFromHarfBuzzRun(glyphBuffer, currentRun, firstOffsetOfNextRun);
        }
    } else {
        m_startOffset = m_harfbuzzRuns.first()->offsets()[0];
        for (unsigned runIndex = 0; runIndex < numRuns; ++runIndex) {
            HarfBuzzRun* currentRun = m_harfbuzzRuns[runIndex].get();
            FloatPoint firstOffsetOfNextRun = runIndex == numRuns - 1 ? FloatPoint() : m_harfbuzzRuns[runIndex + 1]->offsets()[0];
            fillGlyphBufferFromHarfBuzzRun(glyphBuffer, currentRun, firstOffsetOfNextRun);
        }
    }
}

int HarfBuzzShaper::offsetForPosition(float targetX)
{
    int charactersSoFar = 0;
    float currentX = 0;

    if (m_run.rtl()) {
        charactersSoFar = m_normalizedBufferLength;
        for (int i = m_harfbuzzRuns.size() - 1; i >= 0; --i) {
            charactersSoFar -= m_harfbuzzRuns[i]->numCharacters();
            float nextX = currentX + m_harfbuzzRuns[i]->width();
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_harfbuzzRuns[i]->width()) {
                // The x value in question is within this script run.
                const unsigned index = m_harfbuzzRuns[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            currentX = nextX;
        }
    } else {
        for (unsigned i = 0; i < m_harfbuzzRuns.size(); ++i) {
            float nextX = currentX + m_harfbuzzRuns[i]->width();
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_harfbuzzRuns[i]->width()) {
                const unsigned index = m_harfbuzzRuns[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            charactersSoFar += m_harfbuzzRuns[i]->numCharacters();
            currentX = nextX;
        }
    }

    return charactersSoFar;
}

FloatRect HarfBuzzShaper::selectionRect(const FloatPoint& point, int height, int from, int to)
{
    float currentX = 0;
    float fromX = 0;
    float toX = 0;
    bool foundFromX = false;
    bool foundToX = false;

    if (m_run.rtl())
        currentX = m_totalWidth;
    for (unsigned i = 0; i < m_harfbuzzRuns.size(); ++i) {
        if (m_run.rtl())
            currentX -= m_harfbuzzRuns[i]->width();
        int numCharacters = m_harfbuzzRuns[i]->numCharacters();
        if (!foundFromX && from >= 0 && from < numCharacters) {
            fromX = m_harfbuzzRuns[i]->xPositionForOffset(from) + currentX;
            foundFromX = true;
        } else
            from -= numCharacters;

        if (!foundToX && to >= 0 && to < numCharacters) {
            toX = m_harfbuzzRuns[i]->xPositionForOffset(to) + currentX;
            foundToX = true;
        } else
            to -= numCharacters;

        if (foundFromX && foundToX)
            break;
        if (!m_run.rtl())
            currentX += m_harfbuzzRuns[i]->width();
    }

    // The position in question might be just after the text.
    if (!foundFromX)
        fromX = 0;
    if (!foundToX)
        toX = m_run.rtl() ? 0 : m_totalWidth;

    // Using floorf() and roundf() as the same as mac port.
    if (fromX < toX)
        return FloatRect(floorf(point.x() + fromX), point.y(), roundf(toX - fromX), height);
    return FloatRect(floorf(point.x() + toX), point.y(), roundf(fromX - toX), height);
}

} // namespace WebCore
