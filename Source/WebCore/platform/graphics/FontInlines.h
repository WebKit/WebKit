/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <WebCore/Font.h>

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

namespace WebCore {

#if ENABLE(OPENTYPE_VERTICAL)
inline const OpenTypeVerticalData* Font::verticalData() const { return m_verticalData.get(); }
#endif

ALWAYS_INLINE FloatRect Font::boundsForGlyph(Glyph glyph) const
{
    if (isZeroWidthSpaceGlyph(glyph))
        return FloatRect();

    FloatRect bounds;
    if (m_glyphToBoundsMap) {
        bounds = m_glyphToBoundsMap->metricsForGlyph(glyph);
        if (bounds.width() != cGlyphSizeUnknown)
            return bounds;
    }

    bounds = platformBoundsForGlyph(glyph);
    if (!m_glyphToBoundsMap)
        m_glyphToBoundsMap = makeUnique<GlyphMetricsMap<FloatRect>>();
    m_glyphToBoundsMap->setMetricsForGlyph(glyph, bounds);
    return bounds;
}

#if USE(CORE_TEXT) || USE(SKIA)
ALWAYS_INLINE Vector<FloatRect, Font::inlineGlyphRunCapacity> Font::boundsForGlyphs(std::span<const Glyph> glyphs) const
{
    const auto glyphCount = glyphs.size();
    if (!glyphCount) [[unlikely]]
        return { };

    if (glyphCount == 1) [[unlikely]]
        return { boundsForGlyph(glyphs[0]) };

    Vector<Glyph, inlineGlyphRunCapacity> glyphsNeedingMeasurement;
    Vector<uint32_t, inlineGlyphRunCapacity> positionsNeedingMeasurement;

    Vector<FloatRect, inlineGlyphRunCapacity> glyphBounds(FillWith { }, glyphCount, FloatRect());
    for (size_t glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
        const auto& glyph = glyphs[glyphIndex];
        if (isZeroWidthSpaceGlyph(glyph))
            continue;

        if (m_glyphToBoundsMap) {
            auto bounds = m_glyphToBoundsMap->metricsForGlyph(glyph);
            if (bounds.width() != cGlyphSizeUnknown) {
                glyphBounds[glyphIndex] = bounds;
                continue;
            }
        }

        glyphsNeedingMeasurement.append(glyph);
        positionsNeedingMeasurement.append(glyphIndex);
    }

    if (glyphsNeedingMeasurement.isEmpty())
        return glyphBounds;

    if (!m_glyphToBoundsMap)
        m_glyphToBoundsMap = makeUnique<GlyphMetricsMap<FloatRect>>();

    auto measuredBounds = platformBoundsForGlyphs(glyphsNeedingMeasurement);

    size_t index = 0;
    for (auto& bounds : measuredBounds) {
        const auto measuredGlyph = glyphsNeedingMeasurement[index];
        const auto measuredGlyphPosition = positionsNeedingMeasurement[index];

        m_glyphToBoundsMap->setMetricsForGlyph(measuredGlyph, bounds);
        glyphBounds[measuredGlyphPosition] = bounds;
        ++index;
    }
    return glyphBounds;
}
#endif

ALWAYS_INLINE float Font::widthForGlyph(Glyph glyph, SyntheticBoldInclusion syntheticBoldInclusion) const
{
    if (isZeroWidthSpaceGlyph(glyph) && !isInterstitial())
        return 0;

    float width = m_glyphToWidthMap.metricsForGlyph(glyph);
    if (width != cGlyphSizeUnknown)
        return width + (syntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);

#if ENABLE(OPENTYPE_VERTICAL)
    if (m_verticalData)
        width = m_verticalData->advanceHeight(this, glyph);
    else
#endif
        width = platformWidthForGlyph(glyph);

    m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
    return width + (syntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);
}

#if USE(CORE_TEXT)
ALWAYS_INLINE Vector<float, Font::inlineGlyphRunCapacity> Font::widthsForGlyphs(std::span<const Glyph> glyphs, SyntheticBoldInclusion syntheticBoldInclusion) const
{
    ASSERT(glyphs.size());

    if (glyphs.size() == 1) [[unlikely]]
        return { widthForGlyph(glyphs[0], syntheticBoldInclusion) };

    Vector<Glyph, inlineGlyphRunCapacity> glyphsNeedingMeasurement;
    Vector<uint32_t, inlineGlyphRunCapacity> positionsNeedingMeasurement;

    glyphsNeedingMeasurement.reserveInitialCapacity(glyphs.size());
    positionsNeedingMeasurement.reserveInitialCapacity(glyphs.size());

    Vector<float, inlineGlyphRunCapacity> glyphWidths(FillWith { }, glyphs.size(), 0.f);
    const bool isInterstitial = this->isInterstitial();
    const float syntheticBoldOffset = syntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? this->syntheticBoldOffset() : 0;
    for (size_t glyphIndex = 0; glyphIndex < glyphs.size(); ++glyphIndex) {
        const auto& glyph = glyphs[glyphIndex];
        if (isZeroWidthSpaceGlyph(glyph) && isInterstitial)
            continue;

        float width = m_glyphToWidthMap.metricsForGlyph(glyph);
        if (width != cGlyphSizeUnknown) {
            glyphWidths[glyphIndex] = width + syntheticBoldOffset;
            continue;
        }

        glyphsNeedingMeasurement.append(glyph);
        positionsNeedingMeasurement.append(glyphIndex);
    }

    if (glyphsNeedingMeasurement.isEmpty())
        return glyphWidths;

    auto measuredWidths = platformWidthsForGlyphs(glyphsNeedingMeasurement);

    size_t index = 0;
    for (auto& width : measuredWidths) {
        const auto measuredGlyph = glyphsNeedingMeasurement[index];
        const auto measuredGlyphPosition = positionsNeedingMeasurement[index];

        m_glyphToWidthMap.setMetricsForGlyph(measuredGlyph, width);
        glyphWidths[measuredGlyphPosition] = width + syntheticBoldOffset;
        ++index;
    }
    return glyphWidths;
}
#endif

#if USE(CORE_TEXT)
template<typename charType>
ALWAYS_INLINE std::pair<Vector<Glyph, Font::inlineGlyphRunCapacity>, Vector<float, Font::inlineGlyphRunCapacity>>
Font::glyphsAndWidthsForCharacters(std::span<charType> characters, SyntheticBoldInclusion syntheticBoldInclusion) const
{
    ASSERT(characters.size());

    Vector<Glyph, inlineGlyphRunCapacity> glyphs;
    glyphs.reserveInitialCapacity(characters.size());

    Vector<float, inlineGlyphRunCapacity> widths(FillWith { }, characters.size(), 0.f);

    Vector<Glyph, inlineGlyphRunCapacity> uncachedGlyphs;
    Vector<uint32_t, inlineGlyphRunCapacity> uncachedPositions;

    const bool isInterstitial = this->isInterstitial();
    const float syntheticBoldOffset = syntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? this->syntheticBoldOffset() : 0;

    for (size_t i = 0; i < characters.size(); ++i) {
        auto glyph = glyphForCharacter(characters[i]);
        glyphs.append(glyph);

        if (isZeroWidthSpaceGlyph(glyph) && isInterstitial)
            continue;

        float width = m_glyphToWidthMap.metricsForGlyph(glyph);
        if (width != cGlyphSizeUnknown) {
            widths[i] = width + syntheticBoldOffset;
            continue;
        }

        if (uncachedGlyphs.isEmpty()) {
            uncachedGlyphs.reserveInitialCapacity(characters.size());
            uncachedPositions.reserveInitialCapacity(characters.size());
        }
        uncachedGlyphs.append(glyph);
        uncachedPositions.append(i);
    }

    if (!uncachedGlyphs.isEmpty()) {
        auto measuredWidths = platformWidthsForGlyphs(uncachedGlyphs);
        for (size_t j = 0; j < measuredWidths.size(); ++j) {
            m_glyphToWidthMap.setMetricsForGlyph(uncachedGlyphs[j], measuredWidths[j]);
            widths[uncachedPositions[j]] = measuredWidths[j] + syntheticBoldOffset;
        }
    }

    return { WTF::move(glyphs), WTF::move(widths) };
}
#endif

} // namespace WebCore
