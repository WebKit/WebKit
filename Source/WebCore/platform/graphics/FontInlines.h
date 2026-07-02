/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
    Vector<size_t, inlineGlyphRunCapacity> positionsNeedingMeasurement;

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

ALWAYS_INLINE float Font::widthForGlyph(Glyph glyph, SyntheticBoldInclusion SyntheticBoldInclusion) const
{
    // The optimization of returning 0 for the zero-width-space glyph is incorrect for the LastResort font,
    // used in place of the actual font when isLoading() is true on both macOS and iOS.
    // The zero-width-space glyph in that font does not have a width of zero and, further, that glyph is used
    // for many other characters and must not be zero width when used for them.
    if (isZeroWidthSpaceGlyph(glyph) && !isInterstitial())
        return 0;

    float width = m_glyphToWidthMap.metricsForGlyph(glyph);
    if (width != cGlyphSizeUnknown)
        return width + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);

#if ENABLE(OPENTYPE_VERTICAL)
    if (m_verticalData)
        width = m_verticalData->advanceHeight(this, glyph);
    else
#endif
        width = platformWidthForGlyph(glyph);

    m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
    return width + (SyntheticBoldInclusion == SyntheticBoldInclusion::Incorporate ? syntheticBoldOffset() : 0);
}

} // namespace WebCore
