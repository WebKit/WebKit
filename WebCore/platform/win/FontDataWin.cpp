/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
 */

#include "config.h"
#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <mlang.h>

namespace WebCore
{

void FontData::platformInit()
{    
    m_isMLangFont = false;

    HDC dc = GetDC(0);
    SaveDC(dc);

    cairo_scaled_font_t* scaledFont = m_font.scaledFont();
    const double metricsMultiplier = cairo_win32_scaled_font_get_metrics_factor(scaledFont) * m_font.size();

    cairo_win32_scaled_font_select_font(scaledFont, dc);

    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_ascent = lroundf(tm.tmAscent * metricsMultiplier);
    m_descent = lroundf(tm.tmDescent * metricsMultiplier);
    m_xHeight = m_ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.
    m_lineGap = lroundf(tm.tmExternalLeading * metricsMultiplier);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    OUTLINETEXTMETRIC otm;
    if (GetOutlineTextMetrics(dc, sizeof(otm), &otm) > 0) {
        // This is a TrueType font.  We might be able to get an accurate xHeight.
        GLYPHMETRICS gm;
        MAT2 mat = { 1, 0, 0, 1 }; // The identity matrix.
        DWORD len = GetGlyphOutlineW(dc, 'x', GGO_METRICS, &gm, 0, 0, &mat);
        if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
            m_xHeight = gm.gmptGlyphOrigin.y * metricsMultiplier;
    }

    cairo_win32_scaled_font_done_font(scaledFont);
    
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

void FontData::platformDestroy()
{
    cairo_font_face_destroy(m_font.fontFace());
    cairo_scaled_font_destroy(m_font.scaledFont());

    if (m_isMLangFont) {
        // We have to release the font instead of just deleting it, since we didn't make it.
        IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
        if (langFontLink)
            langFontLink->ReleaseFont(m_font.hfont());
    } else
        DeleteObject(m_font.hfont());

    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        LOGFONT winfont;
        GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
        int smallCapsHeight = lroundf(0.70f * fontDescription.computedSize());
        winfont.lfHeight = -smallCapsHeight;
        HFONT hfont = CreateFontIndirect(&winfont);
        m_smallCapsFontData = new FontData(FontPlatformData(hfont, smallCapsHeight));
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: Need to check character ranges.
    // IMLangFontLink2::GetFontUnicodeRanges does what we want. 
    return false;
}

void FontData::determinePitch()
{
    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC((HWND)0);
    SaveDC(dc);
    SelectObject(dc, m_font.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_treatAsFixedPitch = ((tm.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
    HDC dc = GetDC(0);
    SaveDC(dc);

    cairo_scaled_font_t* scaledFont = m_font.scaledFont();
    cairo_win32_scaled_font_select_font(scaledFont, dc);

    int width;
    GetCharWidthI(dc, glyph, 1, 0, &width);

    cairo_win32_scaled_font_done_font(scaledFont);

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    const double metricsMultiplier = cairo_win32_scaled_font_get_metrics_factor(scaledFont) * m_font.size();
    return width * metricsMultiplier;
}

}
