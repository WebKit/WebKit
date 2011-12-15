/*
 * Copyright (C) 2006, 2007 Apple Inc. All Rights Reserved.
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
#include "PlatformSupport.h"
#include <wtf/MathExtras.h>

#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <objidl.h>
#include <mlang.h>

namespace WebCore {

void SimpleFontData::platformInit()
{
    if (!m_platformData.size()) {
        m_fontMetrics.reset();
        m_avgCharWidth = 0;
        m_maxCharWidth = 0;
        return;
    }

    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());

    TEXTMETRIC textMetric = {0};
    if (!GetTextMetrics(dc, &textMetric)) {
        if (PlatformSupport::ensureFontLoaded(m_platformData.hfont())) {
            // Retry GetTextMetrics.
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetTextMetrics(dc, &textMetric))
                LOG_ERROR("Unable to get the text metrics after second attempt");
        }
    }

    m_avgCharWidth = textMetric.tmAveCharWidth;
    m_maxCharWidth = textMetric.tmMaxCharWidth;

    // FIXME: Access ascent/descent/lineGap with floating point precision.
    float ascent = textMetric.tmAscent;
    float descent = textMetric.tmDescent;
    float lineGap = textMetric.tmExternalLeading;
    float xHeight = ascent * 0.56f; // Best guess for xHeight for non-Truetype fonts.

    OUTLINETEXTMETRIC outlineTextMetric;
    if (GetOutlineTextMetrics(dc, sizeof(outlineTextMetric), &outlineTextMetric) > 0) {
        // This is a TrueType font.  We might be able to get an accurate xHeight.
        GLYPHMETRICS glyphMetrics = {0};
        MAT2 identityMatrix = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        DWORD len = GetGlyphOutlineW(dc, 'x', GGO_METRICS, &glyphMetrics, 0, 0, &identityMatrix);
        if (len != GDI_ERROR && glyphMetrics.gmBlackBoxY > 0)
            xHeight = static_cast<float>(glyphMetrics.gmBlackBoxY);
    }

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setXHeight(xHeight);
    m_fontMetrics.setLineSpacing(ascent + descent + lineGap);

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
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
    LOGFONT winFont;
    GetObject(m_platformData.hfont(), sizeof(LOGFONT), &winFont);
    float scaledSize = scaleFactor * fontDescription.computedSize();
    winFont.lfHeight = -lroundf(scaledSize);
    HFONT hfont = CreateFontIndirect(&winFont);
    return adoptPtr(new SimpleFontData(FontPlatformData(hfont, scaledSize), isCustomFont(), false));
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->smallCaps)
        m_derivedFontData->smallCaps = createScaledFontData(fontDescription, .7);

    return m_derivedFontData->smallCaps.get();
}

SimpleFontData* SimpleFontData::emphasisMarkFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->emphasisMark)
        m_derivedFontData->emphasisMark = createScaledFontData(fontDescription, .5);

    return m_derivedFontData->emphasisMark.get();
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
  // This used to be implemented with IMLangFontLink2, but since that code has
  // been disabled, this would always return false anyway.
  return false;
}

void SimpleFontData::determinePitch()
{
    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC textMetric = {0};
    if (!GetTextMetrics(dc, &textMetric)) {
        if (PlatformSupport::ensureFontLoaded(m_platformData.hfont())) {
            // Retry GetTextMetrics.
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetTextMetrics(dc, &textMetric))
                LOG_ERROR("Unable to get the text metrics after second attempt");
        }
    }

    m_treatAsFixedPitch = ((textMetric.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph) const
{
    return FloatRect();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());

    int width = 0;
    if (!GetCharWidthI(dc, glyph, 1, 0, &width)) {
        // Ask the browser to preload the font and retry.
        if (PlatformSupport::ensureFontLoaded(m_platformData.hfont())) {
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetCharWidthI(dc, glyph, 1, 0, &width))
                LOG_ERROR("Unable to get the char width after second attempt");
        }
    }

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);

    return static_cast<float>(width);
}

void SimpleFontData::updateGlyphWithVariationSelector(UChar32 character, UChar32 selector, Glyph& glyph) const
{
    // FIXME: Implement.
}

}  // namespace WebCore
