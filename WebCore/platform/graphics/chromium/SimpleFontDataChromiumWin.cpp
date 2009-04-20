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

#include "ChromiumBridge.h"
#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>

#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <objidl.h>
#include <mlang.h>

namespace WebCore {

static inline float scaleEmToUnits(float x, int unitsPerEm)
{
    return unitsPerEm ? x / static_cast<float>(unitsPerEm) : x;
}

void SimpleFontData::platformInit()
{
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    TEXTMETRIC textMetric = {0};
    if (!GetTextMetrics(dc, &textMetric)) {
        if (ChromiumBridge::ensureFontLoaded(m_font.hfont())) {
            // Retry GetTextMetrics.
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetTextMetrics(dc, &textMetric))
                ASSERT_NOT_REACHED();
        }
    }

    m_avgCharWidth = textMetric.tmAveCharWidth;
    m_maxCharWidth = textMetric.tmMaxCharWidth;

    m_ascent = textMetric.tmAscent;
    m_descent = textMetric.tmDescent;
    m_lineGap = textMetric.tmExternalLeading;
    m_xHeight = m_ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.

    OUTLINETEXTMETRIC outlineTextMetric;
    if (GetOutlineTextMetrics(dc, sizeof(outlineTextMetric), &outlineTextMetric) > 0) {
        // This is a TrueType font.  We might be able to get an accurate xHeight.
        GLYPHMETRICS glyphMetrics = {0};
        MAT2 identityMatrix = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        DWORD len = GetGlyphOutlineW(dc, 'x', GGO_METRICS, &glyphMetrics, 0, 0, &identityMatrix);
        if (len != GDI_ERROR && glyphMetrics.gmBlackBoxY > 0)
            m_xHeight = static_cast<float>(glyphMetrics.gmBlackBoxY);
    }

    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
}

void SimpleFontData::platformCharWidthInit()
{
    // charwidths are set in platformInit.
}

void SimpleFontData::platformDestroy()
{
    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;
    m_smallCapsFontData = 0;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        LOGFONT winFont;
        GetObject(m_font.hfont(), sizeof(LOGFONT), &winFont);
        float smallCapsSize = 0.70f * fontDescription.computedSize();
        // Unlike WebKit trunk, we don't multiply the size by 32.  That seems
        // to be some kind of artifact of their CG backend, or something.
        winFont.lfHeight = -lroundf(smallCapsSize);
        HFONT hfont = CreateFontIndirect(&winFont);
        m_smallCapsFontData =
            new SimpleFontData(FontPlatformData(hfont, smallCapsSize));
    }
    return m_smallCapsFontData;
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
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC textMetric = {0};
    if (!GetTextMetrics(dc, &textMetric)) {
        if (ChromiumBridge::ensureFontLoaded(m_font.hfont())) {
            // Retry GetTextMetrics.
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetTextMetrics(dc, &textMetric))
                ASSERT_NOT_REACHED();
        }
    }

    m_treatAsFixedPitch = ((textMetric.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());

    int width = 0;
    if (!GetCharWidthI(dc, glyph, 1, 0, &width)) {
        // Ask the browser to preload the font and retry.
        if (ChromiumBridge::ensureFontLoaded(m_font.hfont())) {
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetCharWidthI(dc, glyph, 1, 0, &width))
                ASSERT_NOT_REACHED();
        }
    }

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);

    return static_cast<float>(width);
}

}  // namespace WebCore
