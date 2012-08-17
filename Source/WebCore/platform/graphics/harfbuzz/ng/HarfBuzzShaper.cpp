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

void HarfBuzzShaper::HarfBuzzRun::setGlyphAndAdvance(unsigned index, uint16_t glyphId, float advance)
{
    m_glyphs[index] = glyphId;
    m_advances[index] = advance;
}

int HarfBuzzShaper::HarfBuzzRun::characterIndexForXPosition(float targetX)
{
    ASSERT(targetX <= m_width);
    float currentX = 0;
    float prevAdvance = 0;
    for (unsigned i = 0; i < m_numGlyphs; ++i) {
        float currentAdvance = m_advances[i] / 2.0;
        float nextX = currentX + prevAdvance + currentAdvance;
        if (currentX <= targetX && targetX <= nextX)
            return m_glyphToCharacterIndex[i] + (rtl() ? 1 : 0);
        currentX = nextX;
        prevAdvance = currentAdvance;
    }

    return rtl() ? 0 : m_numCharacters;
}

float HarfBuzzShaper::HarfBuzzRun::xPositionForOffset(unsigned offset)
{
    ASSERT(offset < m_numCharacters);
    unsigned glyphIndex = m_logClusters[offset];
    ASSERT(glyphIndex <= m_numGlyphs);
    float position = 0;
    for (unsigned i = 0; i < glyphIndex; ++i)
        position += m_advances[i];
    if (rtl())
        position += m_advances[glyphIndex];
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
        if (Font::treatAsSpace(character))
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
    normalizeCharacters(m_run.characters(), m_normalizedBuffer.get(), m_normalizedBufferLength);
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

bool HarfBuzzShaper::shouldDrawCharacterAt(int index)
{
    return m_fromIndex <= index && index < m_toIndex;
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
    if (!shapeHarfBuzzRuns(glyphBuffer))
        return false;
    m_totalWidth = roundf(m_totalWidth);
    return true;
}

FloatPoint HarfBuzzShaper::adjustStartPoint(const FloatPoint& point)
{
    return point + m_startOffset;
}

bool HarfBuzzShaper::collectHarfBuzzRuns()
{
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
        const SimpleFontData* currentFontData = nextFontData;
        UScriptCode currentScript = nextScript;

        for (iterator.advance(clusterLength); iterator.consume(character, clusterLength); iterator.advance(clusterLength)) {
            nextFontData = m_font->glyphDataForCharacter(character, false).fontData;
            if (nextFontData != currentFontData)
                break;
            nextScript = uscript_getScript(character, &errorCode);
            if (U_FAILURE(errorCode))
                return false;
            if ((currentScript != nextScript) && (currentScript != USCRIPT_INHERITED))
                break;
        }
        unsigned numCharactersOfCurrentRun = iterator.currentCharacter() - startIndexOfCurrentRun;
        m_harfbuzzRuns.append(HarfBuzzRun::create(currentFontData, startIndexOfCurrentRun, numCharactersOfCurrentRun, m_run.direction()));
        currentFontData = nextFontData;
        startIndexOfCurrentRun = iterator.currentCharacter();
    } while (iterator.consume(character, clusterLength));

    return !m_harfbuzzRuns.isEmpty();
}

bool HarfBuzzShaper::shapeHarfBuzzRuns(GlyphBuffer* glyphBuffer)
{
    HarfBuzzScopedPtr<hb_buffer_t> harfbuzzBuffer(hb_buffer_create(), hb_buffer_destroy);
    float pendingGlyphAdvanceX = 0;
    float pendingGlyphAdvanceY = 0;

    hb_buffer_set_unicode_funcs(harfbuzzBuffer.get(), hb_icu_get_unicode_funcs());
    if (m_run.directionalOverride())
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
        setGlyphPositionsForHarfBuzzRun(currentRun, i, harfbuzzBuffer.get(), glyphBuffer, pendingGlyphAdvanceX, pendingGlyphAdvanceY);

        hb_buffer_reset(harfbuzzBuffer.get());
        if (m_run.directionalOverride())
            hb_buffer_set_direction(harfbuzzBuffer.get(), m_run.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    }
    return true;
}

void HarfBuzzShaper::setGlyphPositionsForHarfBuzzRun(HarfBuzzRun* currentRun, unsigned runIndexInVisualOrder, hb_buffer_t* harfbuzzBuffer, GlyphBuffer* glyphBuffer, float& pendingGlyphAdvanceX, float& pendingGlyphAdvanceY)
{
    const SimpleFontData* currentFontData = currentRun->fontData();
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(harfbuzzBuffer, 0);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(harfbuzzBuffer, 0);

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

        unsigned currentCharacterIndex = currentRun->startIndex() + glyphInfos[i].cluster;
        bool isClusterEnd = runEnd || glyphInfos[i].cluster != glyphInfos[i + 1].cluster;
        float spacing = isClusterEnd ? m_letterSpacing : 0;

        if (isClusterEnd && isWordEnd(currentCharacterIndex))
            spacing += determineWordBreakSpacing();

        if (currentFontData->isZeroWidthSpaceGlyph(glyph)) {
            currentRun->setGlyphAndAdvance(i, glyph, 0);
            if (glyphBuffer && shouldDrawCharacterAt(currentCharacterIndex))
                glyphBuffer->add(glyph, currentFontData, createGlyphBufferAdvance(0, 0));
            continue;
        }

        advance += spacing;

        currentRun->setGlyphAndAdvance(i, glyph, advance);
        if (glyphBuffer) {
            if (!i && !runIndexInVisualOrder)
                m_startOffset.set(offsetX, offsetY);
            float glyphAdvanceX = advance + nextOffsetX - offsetX;
            float glyphAdvanceY = nextOffsetY - offsetY;
            if (shouldDrawCharacterAt(currentCharacterIndex)) {
                glyphAdvanceX += pendingGlyphAdvanceX;
                glyphAdvanceY += pendingGlyphAdvanceY;
                pendingGlyphAdvanceX = 0;
                pendingGlyphAdvanceY = 0;
                glyphBuffer->add(glyph, currentFontData, createGlyphBufferAdvance(glyphAdvanceX, glyphAdvanceY));
            } else {
                pendingGlyphAdvanceX += glyphAdvanceX;
                pendingGlyphAdvanceY += glyphAdvanceY;
            }
        }

        totalAdvance += advance;
    }
    currentRun->setWidth(totalAdvance > 0.0 ? totalAdvance : 0.0);
    m_totalWidth += currentRun->width();
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
