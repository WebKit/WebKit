/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
 */

#include "config.h"
#include "Font.h"

#include <windows.h>

#include "FontCache.h"
#include "FontDescription.h"
#include "HWndDC.h"
#include <cairo.h>
#include <cairo-win32.h>
#include <mlang.h>
#include <wtf/MathExtras.h>

namespace WebCore {

void Font::platformInit()
{
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;

    if (!m_platformData.size()) {
        m_fontMetrics.reset();
        m_avgCharWidth = 0;
        m_maxCharWidth = 0;
        return;
    }

    HWndDC dc(0);
    SaveDC(dc);

    const double metricsMultiplier = 1. / cWindowsFontScaleFactor;
    HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());

    wchar_t faceName[LF_FACESIZE];
    GetTextFace(dc, LF_FACESIZE, faceName);

    OUTLINETEXTMETRIC metrics;
    if (!GetOutlineTextMetrics(dc, sizeof(metrics), &metrics))
        return;

    float xHeight = metrics.otmTextMetrics.tmAscent * 0.56f; // Best guess for xHeight if no x glyph is present.
    GLYPHMETRICS gm;
    static const MAT2 identity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    DWORD len = GetGlyphOutline(dc, 'x', GGO_METRICS, &gm, 0, 0, &identity);
    if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
        xHeight = gm.gmptGlyphOrigin.y;

    SelectObject(dc, oldFont);
    RestoreDC(dc, -1);

    // Disable antialiasing when rendering with Ahem because many tests require this.
    if (!_wcsicmp(faceName, L"Ahem"))
        m_allowsAntialiasing = false;

    // FIXME: Needs to take OS/2 USE_TYPO_METRICS flag into account
    // https://bugs.webkit.org/show_bug.cgi?id=199186
    float ascent = metrics.otmTextMetrics.tmAscent * metricsMultiplier;
    float descent = metrics.otmTextMetrics.tmDescent * metricsMultiplier;
    float capHeight = (metrics.otmTextMetrics.tmAscent - metrics.otmTextMetrics.tmInternalLeading) * metricsMultiplier;
    float lineGap = metrics.otmTextMetrics.tmExternalLeading * metricsMultiplier;

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setCapHeight(capHeight);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));
    m_fontMetrics.setUnitsPerEm(metrics.otmEMSquare);
    m_fontMetrics.setXHeight(xHeight * metricsMultiplier);
    m_avgCharWidth = metrics.otmTextMetrics.tmAveCharWidth * metricsMultiplier;
    m_maxCharWidth = metrics.otmTextMetrics.tmMaxCharWidth * metricsMultiplier;
}

void Font::determinePitch()
{
    if (origin() == Origin::Remote) {
        m_treatAsFixedPitch = false;
        return;
    }

    // TEXTMETRICS have this. Set m_treatAsFixedPitch based off that.
    HWndDC dc(0);
    SaveDC(dc);
    SelectObject(dc, m_platformData.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch. Unbelievable but true!
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_treatAsFixedPitch = !(tm.tmPitchAndFamily & TMPF_FIXED_PITCH);

    RestoreDC(dc, -1);
}

bool Font::platformSupportsCodePoint(char32_t character, std::optional<char32_t> variation) const
{
    return variation ? false : glyphForCharacter(character);
}

}
