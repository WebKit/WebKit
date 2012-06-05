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
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include "hb-icu.h"
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

static inline float harfbuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

HarfBuzzShaper::HarfBuzzRun::HarfBuzzRun(unsigned numCharacters, TextDirection direction, hb_buffer_t* harfbuzzBuffer)
    : m_numCharacters(numCharacters)
    , m_direction(direction)
{
    m_numGlyphs = hb_buffer_get_length(harfbuzzBuffer);
    m_glyphs.resize(m_numGlyphs);
    m_advances.resize(m_numGlyphs);
    m_offsets.resize(m_numGlyphs);
    m_glyphToCharacterIndex.resize(m_numGlyphs);
    m_logClusters.resize(m_numCharacters);

    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(harfbuzzBuffer, 0);
    for (unsigned i = 0; i < m_numGlyphs; ++i)
        m_glyphToCharacterIndex[i] = infos[i].cluster;

    // Fill logical clusters
    unsigned index = 0;
    while (index < m_numGlyphs) {
        unsigned nextIndex = index + 1;
        while (nextIndex < m_numGlyphs && infos[index].cluster == infos[nextIndex].cluster)
            ++nextIndex;
        if (rtl()) {
            int nextCluster = nextIndex < m_numGlyphs ? infos[nextIndex].cluster : -1;
            for (int j = infos[index].cluster; j > nextCluster; --j)
                m_logClusters[j] = index;
        } else {
            unsigned nextCluster = nextIndex < m_numGlyphs ? infos[nextIndex].cluster : m_numCharacters;
            for (unsigned j = infos[index].cluster; j < nextCluster; ++j)
                m_logClusters[j] = index;
        }
        index = nextIndex;
    }
}

void HarfBuzzShaper::HarfBuzzRun::setGlyphAndPositions(unsigned index, uint16_t glyphId, float x, float y, float advance)
{
    m_glyphs[index] = glyphId;
    m_offsets[index].set(x, y);
    m_advances[index] = advance;
}

int HarfBuzzShaper::HarfBuzzRun::characterIndexForXPosition(int targetX)
{
    ASSERT(static_cast<unsigned>(targetX) <= m_width);
    int currentX = 0;
    float prevAdvance = 0;
    for (unsigned i = 0; i < m_numGlyphs; ++i) {
        float currentAdvance = m_advances[i] / 2.0;
        int nextX = currentX + roundf(prevAdvance + currentAdvance);
        if (currentX <= targetX && targetX <= nextX)
            return m_glyphToCharacterIndex[i] + (rtl() ? 1 : 0);
        currentX = nextX;
        prevAdvance = currentAdvance;
    }

    return rtl() ? 0 : m_numCharacters;
}

int HarfBuzzShaper::HarfBuzzRun::xPositionForOffset(unsigned offset)
{
    ASSERT(offset < m_numCharacters);
    unsigned glyphIndex = m_logClusters[offset];
    ASSERT(glyphIndex < m_numGlyphs);
    float position = m_offsets[glyphIndex].x();
    if (rtl())
        position += m_advances[glyphIndex];
    return roundf(position);
}

HarfBuzzShaper::HarfBuzzShaper(const Font* font, const TextRun& run)
    : HarfBuzzShaperBase(font, run)
    , m_startIndexOfCurrentRun(0)
    , m_numCharactersOfCurrentRun(0)
    , m_harfbuzzBuffer(0)
{
    setNormalizedBuffer();
    setFontFeatures();
}

HarfBuzzShaper::~HarfBuzzShaper()
{
    if (m_harfbuzzBuffer)
        hb_buffer_destroy(m_harfbuzzBuffer);
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
    m_totalWidth = 0;
    while (setupHarfBuzzRun()) {
        if (!shapeHarfBuzzRun())
            return false;
        setGlyphPositionsForHarfBuzzRun(glyphBuffer);
    }

    if (!m_harfbuzzRuns.size())
        return false;

    return true;
}

bool HarfBuzzShaper::setupHarfBuzzRun()
{
    m_startIndexOfCurrentRun += m_numCharactersOfCurrentRun;

    // Iterate through the text to take the largest range that stays within
    // a single font.
    int endOfRunIndex = m_normalizedBufferLength - m_startIndexOfCurrentRun;
    SurrogatePairAwareTextIterator iterator(m_normalizedBuffer.get() + m_startIndexOfCurrentRun, 0, endOfRunIndex, endOfRunIndex);
    UChar32 character;
    unsigned clusterLength = 0;
    if (!iterator.consume(character, clusterLength))
        return false;

    m_currentFontData = m_font->glyphDataForCharacter(character, false).fontData;
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode currentScript = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return false;
    if (currentScript == USCRIPT_INHERITED)
        currentScript = USCRIPT_COMMON;
    for (iterator.advance(clusterLength); iterator.consume(character, clusterLength); iterator.advance(clusterLength)) {
        const SimpleFontData* nextFontData = m_font->glyphDataForCharacter(character, false).fontData;
        if (nextFontData != m_currentFontData)
            break;
        UScriptCode nextScript = uscript_getScript(character, &errorCode);
        if (U_FAILURE(errorCode))
            return false;
        if (currentScript == nextScript || nextScript == USCRIPT_INHERITED || nextScript == USCRIPT_COMMON)
            continue;
        if (currentScript == USCRIPT_COMMON)
            currentScript = nextScript;
        else
            break;
    }
    m_numCharactersOfCurrentRun = iterator.currentCharacter();

    if (!m_harfbuzzBuffer) {
        m_harfbuzzBuffer = hb_buffer_create();
        hb_buffer_set_unicode_funcs(m_harfbuzzBuffer, hb_icu_get_unicode_funcs());
    } else
        hb_buffer_reset(m_harfbuzzBuffer);
    hb_buffer_set_script(m_harfbuzzBuffer, hb_icu_script_to_script(currentScript));

    // WebKit always sets direction to LTR during width calculation.
    // We only set direction when direction is explicitly set to RTL so that
    // preventng wrong width calculation.
    if (m_run.rtl())
        hb_buffer_set_direction(m_harfbuzzBuffer, HB_DIRECTION_RTL);

    // Determine whether this run needs to be converted to small caps.
    // nextScriptRun() will always send us a run of the same case, because a
    // case change while in small-caps mode always results in different
    // FontData, so we only need to check the first character's case.
    if (m_font->isSmallCaps() && u_islower(m_normalizedBuffer[m_startIndexOfCurrentRun])) {
        String upperText = String(m_normalizedBuffer.get() + m_startIndexOfCurrentRun, m_numCharactersOfCurrentRun);
        upperText.makeUpper();
        m_currentFontData = m_font->glyphDataForCharacter(upperText[0], false, SmallCapsVariant).fontData;
        hb_buffer_add_utf16(m_harfbuzzBuffer, upperText.characters(), m_numCharactersOfCurrentRun, 0, m_numCharactersOfCurrentRun);
    } else
        hb_buffer_add_utf16(m_harfbuzzBuffer, m_normalizedBuffer.get() + m_startIndexOfCurrentRun, m_numCharactersOfCurrentRun, 0, m_numCharactersOfCurrentRun);

    return true;
}

bool HarfBuzzShaper::shapeHarfBuzzRun()
{
    FontPlatformData* platformData = const_cast<FontPlatformData*>(&m_currentFontData->platformData());
    HarfBuzzFace* face = platformData->harfbuzzFace();
    if (!face)
        return false;
    hb_font_t* harfbuzzFont = face->createFont();
    hb_shape(harfbuzzFont, m_harfbuzzBuffer, m_features.size() > 0 ? m_features.data() : 0, m_features.size());
    hb_font_destroy(harfbuzzFont);
    m_harfbuzzRuns.append(HarfBuzzRun::create(m_numCharactersOfCurrentRun, m_run.direction(), m_harfbuzzBuffer));
    return true;
}

void HarfBuzzShaper::setGlyphPositionsForHarfBuzzRun(GlyphBuffer* glyphBuffer)
{
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(m_harfbuzzBuffer, 0);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(m_harfbuzzBuffer, 0);
    HarfBuzzRun* currentRun = m_harfbuzzRuns.last().get();

    unsigned numGlyphs = currentRun->numGlyphs();
    float totalAdvance = 0;
    float nextOffsetX = harfbuzzPositionToFloat(glyphPositions[0].x_offset);
    float nextOffsetY = -harfbuzzPositionToFloat(glyphPositions[0].y_offset);
    // HarfBuzz returns the shaping result in visual order. We need not to flip them for RTL.
    for (size_t i = 0; i < numGlyphs; ++i) {
        bool runEnd = i + 1 == numGlyphs;
        uint16_t glyph = glyphInfos[i].codepoint;
        float offsetX = nextOffsetX;
        float offsetY = nextOffsetY;
        float advance = harfbuzzPositionToFloat(glyphPositions[i].x_advance);
        nextOffsetX = runEnd ? 0 : harfbuzzPositionToFloat(glyphPositions[i + 1].x_offset);
        nextOffsetY = runEnd ? 0 : -harfbuzzPositionToFloat(glyphPositions[i + 1].y_offset);

        unsigned currentCharacterIndex = m_startIndexOfCurrentRun + glyphInfos[i].cluster;
        bool isClusterEnd = runEnd || glyphInfos[i].cluster != glyphInfos[i + 1].cluster;
        float spacing = isClusterEnd ? m_letterSpacing : 0;

        if (isClusterEnd && isWordEnd(currentCharacterIndex))
            spacing += determineWordBreakSpacing();

        if (m_currentFontData->isZeroWidthSpaceGlyph(glyph)) {
            currentRun->setGlyphAndPositions(i, glyph, 0, 0, 0);
            if (glyphBuffer)
                glyphBuffer->add(glyph, m_currentFontData, createGlyphBufferAdvance(0, 0));
            continue;
        }

        advance += spacing;
        currentRun->setGlyphAndPositions(i, glyph, totalAdvance + offsetX, offsetY, advance);
        if (glyphBuffer) {
            float glyphAdvanceX = advance + nextOffsetX - offsetX;
            float glyphAdvanceY = nextOffsetY - offsetY;
            glyphBuffer->add(glyph, m_currentFontData, createGlyphBufferAdvance(glyphAdvanceX, glyphAdvanceY));
        }

        totalAdvance += advance;
    }
    currentRun->setWidth(totalAdvance > 0.0 ? totalAdvance : 0.0);
    m_totalWidth += currentRun->width();
}

int HarfBuzzShaper::offsetForPosition(float targetX)
{
    int charactersSoFar = 0;
    int currentX = 0;

    if (m_run.rtl()) {
        charactersSoFar = m_normalizedBufferLength;
        for (int i = m_harfbuzzRuns.size() - 1; i >= 0; --i) {
            charactersSoFar -= m_harfbuzzRuns[i]->numCharacters();
            int nextX = currentX + m_harfbuzzRuns[i]->width();
            if (currentX <= targetX && targetX <= nextX) {
                // The x value in question is within this script run.
                const unsigned index = m_harfbuzzRuns[i]->characterIndexForXPosition(targetX - currentX);
                return charactersSoFar + index;
            }
            currentX = nextX;
        }
    } else {
        for (unsigned i = 0; i < m_harfbuzzRuns.size(); ++i) {
            int nextX = currentX + m_harfbuzzRuns[i]->width();
            if (currentX <= targetX && targetX <= nextX) {
                const unsigned index = m_harfbuzzRuns[i]->characterIndexForXPosition(targetX - currentX);
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
    int fromX = -1, toX = -1;
    int currentX = 0;
    // Iterate through the script runs in logical order, searching for the run covering the positions of interest.
    for (unsigned i = 0; i < m_harfbuzzRuns.size(); ++i) {
        int numCharacters = m_harfbuzzRuns[i]->numCharacters();
        if (fromX == -1 && from >= 0 && from < numCharacters)
            fromX = m_harfbuzzRuns[i]->xPositionForOffset(from) + currentX;
        else
            from -= numCharacters;

        if (toX == -1 && to >= 0 && to < numCharacters)
            toX = m_harfbuzzRuns[i]->xPositionForOffset(to) + currentX;
        else
            to -= numCharacters;

        if (fromX != -1 && toX != -1)
            break;
        currentX += m_harfbuzzRuns[i]->width();
    }

    // The position in question might be just after the text.
    if (fromX == -1)
        fromX = 0;
    if (toX == -1)
        toX = m_run.rtl() ? 0 : m_totalWidth;

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);
    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

} // namespace WebCore
