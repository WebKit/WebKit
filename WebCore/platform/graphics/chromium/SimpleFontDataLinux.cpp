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

#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "Logging.h"
#include "NotImplemented.h"

#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTime.h"

namespace WebCore {

// Smallcaps versions of fonts are 70% the size of the normal font.
static const float smallCapsFraction = 0.7f;

void SimpleFontData::platformInit()
{
    SkPaint paint;
    SkPaint::FontMetrics metrics;

    m_font.setupPaint(&paint);
    paint.getFontMetrics(&metrics);

    // Beware those who step here: This code is designed to match Win32 font
    // metrics *exactly*.
    if (metrics.fVDMXMetricsValid) {
        m_ascent = metrics.fVDMXAscent;
        m_descent = metrics.fVDMXDescent;
    } else {
        m_ascent = SkScalarRound(-metrics.fAscent);
        m_descent = SkScalarRound(metrics.fHeight) - m_ascent;
    }

    if (metrics.fXHeight)
        m_xHeight = metrics.fXHeight;
    else {
        // hack taken from the Windows port
        m_xHeight = static_cast<float>(m_ascent) * 0.56;
    }

    m_lineGap = SkScalarRound(metrics.fLeading);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
    // calculated for us, but we need to calculate m_maxCharWidth and
    // m_avgCharWidth in order for text entry widgets to be sized correctly.

    m_maxCharWidth = SkScalarRound(metrics.fXRange * SkScalarRound(m_font.size()));

    if (metrics.fAvgCharWidth)
        m_avgCharWidth = SkScalarRound(metrics.fAvgCharWidth);
    else {
        m_avgCharWidth = m_xHeight;

        GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();

        if (glyphPageZero) {
            static const UChar32 x_char = 'x';
            const Glyph xGlyph = glyphPageZero->glyphDataForCharacter(x_char).glyph;

            if (xGlyph)
                m_avgCharWidth = widthForGlyph(xGlyph);
        }
    }
}

void SimpleFontData::platformDestroy()
{
    delete m_smallCapsFontData;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        const float smallCapsSize = lroundf(fontDescription.computedSize() * smallCapsFraction);
        m_smallCapsFontData = new SimpleFontData(FontPlatformData(m_font, smallCapsSize));
    }

    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    SkPaint paint;
    static const unsigned maxBufferCount = 64;
    uint16_t glyphs[maxBufferCount];

    m_font.setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    while (length > 0) {
        int n = SkMin32(length, SK_ARRAY_COUNT(glyphs));

        // textToGlyphs takes a byte count so we double the character count.
        int count = paint.textToGlyphs(characters, n * 2, glyphs);
        for (int i = 0; i < count; i++) {
            if (0 == glyphs[i])
                return false;       // missing glyph
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

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    SkASSERT(sizeof(glyph) == 2);   // compile-time assert

    SkPaint paint;

    m_font.setupPaint(&paint);

    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    SkScalar width = paint.measureText(&glyph, 2);
    
    return SkScalarToFloat(width);
}

}  // namespace WebCore
