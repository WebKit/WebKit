/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "SimpleFontData.h"

#include "FloatRect.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "Logging.h"
#include "SkFontHost.h"
#include "SkPaint.h"
#include "SkTime.h"
#include "SkTypeface.h"
#include "SkTypes.h"
#include "VDMXParser.h"

namespace WebCore {

// Smallcaps versions of fonts are 70% the size of the normal font.
static const float smallCapsFraction = 0.7f;
static const float emphasisMarkFraction = .5;
// This is the largest VDMX table which we'll try to load and parse.
static const size_t maxVDMXTableSize = 1024 * 1024; // 1 MB

void SimpleFontData::platformInit()
{
    if (!m_platformData.size()) {
        m_fontMetrics.reset();
        m_avgCharWidth = 0;
        m_maxCharWidth = 0;
        return;
    }

    SkPaint paint;
    SkPaint::FontMetrics metrics;

    m_platformData.setupPaint(&paint);
    paint.getFontMetrics(&metrics);
    const SkFontID fontID = m_platformData.uniqueID();

    static const uint32_t vdmxTag = SkSetFourByteTag('V', 'D', 'M', 'X');
    int pixelSize = m_platformData.size() + 0.5;
    int vdmxAscent, vdmxDescent;
    bool isVDMXValid = false;

    size_t vdmxSize = SkFontHost::GetTableSize(fontID, vdmxTag);
    if (vdmxSize && vdmxSize < maxVDMXTableSize) {
        uint8_t* vdmxTable = (uint8_t*) fastMalloc(vdmxSize);
        if (vdmxTable
            && SkFontHost::GetTableData(fontID, vdmxTag, 0, vdmxSize, vdmxTable) == vdmxSize
            && parseVDMX(&vdmxAscent, &vdmxDescent, vdmxTable, vdmxSize, pixelSize))
            isVDMXValid = true;
        fastFree(vdmxTable);
    }

    float ascent;
    float descent;

    // Beware those who step here: This code is designed to match Win32 font
    // metrics *exactly*.
    if (isVDMXValid) {
        ascent = vdmxAscent;
        descent = -vdmxDescent;
    } else {
        SkScalar height = -metrics.fAscent + metrics.fDescent + metrics.fLeading;
        ascent = SkScalarRound(-metrics.fAscent);
        descent = SkScalarRound(height) - ascent;
    }

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);

    float xHeight;
    if (metrics.fXHeight)
        xHeight = metrics.fXHeight;
    else {
        // hack taken from the Windows port
        xHeight = ascent * 0.56f;
    }

    float lineGap = SkScalarToFloat(metrics.fLeading);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));

    if (platformData().orientation() == Vertical && !isTextOrientationFallback()) {
        static const uint32_t vheaTag = SkSetFourByteTag('v', 'h', 'e', 'a');
        static const uint32_t vorgTag = SkSetFourByteTag('V', 'O', 'R', 'G');
        size_t vheaSize = SkFontHost::GetTableSize(fontID, vheaTag);
        size_t vorgSize = SkFontHost::GetTableSize(fontID, vorgTag);
        if ((vheaSize > 0) || (vorgSize > 0))
            m_hasVerticalGlyphs = true;
    }

    // In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
    // calculated for us, but we need to calculate m_maxCharWidth and
    // m_avgCharWidth in order for text entry widgets to be sized correctly.

    SkScalar xRange = metrics.fXMax - metrics.fXMin;
    m_maxCharWidth = SkScalarRound(xRange * SkScalarRound(m_platformData.size()));

    if (metrics.fAvgCharWidth)
        m_avgCharWidth = SkScalarRound(metrics.fAvgCharWidth);
    else {
        m_avgCharWidth = xHeight;

        GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();

        if (glyphPageZero) {
            static const UChar32 xChar = 'x';
            const Glyph xGlyph = glyphPageZero->glyphDataForCharacter(xChar).glyph;

            if (xGlyph) {
                // In widthForGlyph(), xGlyph will be compared with
                // m_zeroWidthSpaceGlyph, which isn't initialized yet here.
                // Initialize it with zero to make sure widthForGlyph() returns
                // the right width.
                m_zeroWidthSpaceGlyph = 0;
                m_avgCharWidth = widthForGlyph(xGlyph);
            }
        }
    }
}

void SimpleFontData::platformCharWidthInit()
{
    // charwidths are set in platformInit.
}

void SimpleFontData::platformDestroy()
{
}

PassOwnPtr<SimpleFontData> SimpleFontData::createScaledFontData(const FontDescription& fontDescription, float scaleFactor) const
{
    const float scaledSize = lroundf(fontDescription.computedSize() * scaleFactor);
    return adoptPtr(new SimpleFontData(FontPlatformData(m_platformData, scaledSize), isCustomFont(), false));
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->smallCaps)
        m_derivedFontData->smallCaps = createScaledFontData(fontDescription, smallCapsFraction);

    return m_derivedFontData->smallCaps.get();
}

SimpleFontData* SimpleFontData::emphasisMarkFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->emphasisMark)
        m_derivedFontData->emphasisMark = createScaledFontData(fontDescription, emphasisMarkFraction);

    return m_derivedFontData->emphasisMark.get();
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    SkPaint paint;
    static const unsigned maxBufferCount = 64;
    uint16_t glyphs[maxBufferCount];

    m_platformData.setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    while (length > 0) {
        int n = SkMin32(length, SK_ARRAY_COUNT(glyphs));

        // textToGlyphs takes a byte count so we double the character count.
        int count = paint.textToGlyphs(characters, n * 2, glyphs);
        for (int i = 0; i < count; i++) {
            if (!glyphs[i])
                return false; // missing glyph
        }

        characters += n;
        length -= n;
    }

    return true;
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = platformData().isFixedPitch();
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph) const
{
    return FloatRect();
}
    
float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    SkASSERT(sizeof(glyph) == 2); // compile-time assert

    SkPaint paint;

    m_platformData.setupPaint(&paint);

    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    SkScalar width = paint.measureText(&glyph, 2);

    // Though WebKit supports non-integral advances, Skia only supports them
    // for "subpixel" (distinct from LCD subpixel antialiasing) text, which
    // we don't use.
    return round(SkScalarToFloat(width));
}

} // namespace WebCore
