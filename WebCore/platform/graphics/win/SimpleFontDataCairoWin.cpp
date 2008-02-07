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
#include "SimpleFontData.h"

#include <windows.h>

#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "NotImplemented.h"
#include <mlang.h>
#include <tchar.h>

namespace WebCore {

void SimpleFontData::platformInit()
{
    m_scriptCache = 0;
    m_scriptFontProperties = 0;
    m_isSystemFont = false;
    
    if (m_font.useGDI()) {
        HDC hdc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(hdc, m_font.hfont());
        OUTLINETEXTMETRIC metrics;
        GetOutlineTextMetrics(hdc, sizeof(metrics), &metrics);
        TEXTMETRIC& textMetrics = metrics.otmTextMetrics;
        m_ascent = textMetrics.tmAscent;
        m_descent = textMetrics.tmDescent;
        m_lineGap = textMetrics.tmExternalLeading;
        m_lineSpacing = m_ascent + m_descent + m_lineGap;
        m_xHeight = m_ascent * 0.56f; // Best guess for xHeight if no x glyph is present.

        GLYPHMETRICS gm;
        MAT2 mat = { 1, 0, 0, 1 };
        DWORD len = GetGlyphOutline(hdc, 'x', GGO_METRICS, &gm, 0, 0, &mat);
        if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
            m_xHeight = gm.gmptGlyphOrigin.y;

        m_unitsPerEm = metrics.otmEMSquare;

        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);

        return;
    }

    // FIXME: This section should determine font dimensions (see CG implementation).
    notImplemented();
}

void SimpleFontData::platformDestroy()
{
    notImplemented();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (m_font.useGDI()) {
        HDC hdc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(hdc, m_font.hfont());
        int width;
        GetCharWidthI(hdc, glyph, 1, 0, &width);
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
        return width;
    }

    // FIXME: Flesh out with Cairo/win32 font implementation
    notImplemented();

    return 0;
}

}
