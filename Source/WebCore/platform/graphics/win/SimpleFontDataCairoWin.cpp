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
    m_scriptCache = 0;
    m_scriptFontProperties = 0;

    if (m_platformData.useGDI())
       return initGDIFont();

    if (!m_platformData.size()) {
        m_fontMetrics.reset();
        m_avgCharWidth = 0;
        m_maxCharWidth = 0;
        m_scriptCache = 0;
        m_scriptFontProperties = 0;
        return;
    }

    HWndDC dc(0);
    SaveDC(dc);

    cairo_scaled_font_t* scaledFont = m_platformData.scaledFont();
    const double metricsMultiplier = cairo_win32_scaled_font_get_metrics_factor(scaledFont) * m_platformData.size();

    cairo_win32_scaled_font_select_font(scaledFont, dc);

    // FIXME: Needs to take OS/2 USE_TYPO_METRICS flag into account
    // https://bugs.webkit.org/show_bug.cgi?id=199186
    TEXTMETRIC textMetrics;
    GetTextMetrics(dc, &textMetrics);
    float ascent = textMetrics.tmAscent * metricsMultiplier;
    float descent = textMetrics.tmDescent * metricsMultiplier;
    float lineGap = textMetrics.tmExternalLeading * metricsMultiplier;

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));
    m_avgCharWidth = textMetrics.tmAveCharWidth * metricsMultiplier;
    m_maxCharWidth = textMetrics.tmMaxCharWidth * metricsMultiplier;

    cairo_text_extents_t extents;
    cairo_scaled_font_text_extents(scaledFont, "x", &extents);
    float xHeight = -extents.y_bearing;

    m_fontMetrics.setXHeight(xHeight);
    cairo_win32_scaled_font_done_font(scaledFont);

    m_scriptCache = 0;
    m_scriptFontProperties = 0;

    RestoreDC(dc, -1);
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

bool Font::platformSupportsCodePoint(UChar32 character, std::optional<UChar32> variation) const
{
    return variation ? false : glyphForCharacter(character);
}

}
