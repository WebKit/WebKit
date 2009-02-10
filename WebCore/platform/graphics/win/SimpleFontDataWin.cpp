/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <ApplicationServices/ApplicationServices.h>
#include <mlang.h>
#include <tchar.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

namespace WebCore {

using std::max;

const float cSmallCapsFontSizeMultiplier = 0.7f;

static bool g_shouldApplyMacAscentHack;

void SimpleFontData::setShouldApplyMacAscentHack(bool b)
{
    g_shouldApplyMacAscentHack = b;
}

bool SimpleFontData::shouldApplyMacAscentHack()
{
    return g_shouldApplyMacAscentHack;
}

void SimpleFontData::initGDIFont()
{
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

void SimpleFontData::platformCommonDestroy()
{
    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;
    m_smallCapsFontData = 0;

    ScriptFreeCache(&m_scriptCache);
    delete m_scriptFontProperties;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        float smallCapsHeight = cSmallCapsFontSizeMultiplier * m_font.size();
        if (isCustomFont()) {
            FontPlatformData smallCapsFontData(m_font);
            smallCapsFontData.setSize(smallCapsHeight);
            m_smallCapsFontData = new SimpleFontData(smallCapsFontData, true, false);
        } else {
            LOGFONT winfont;
            GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
            winfont.lfHeight = -lroundf(smallCapsHeight * (m_font.useGDI() ? 1 : 32));
            HFONT hfont = CreateFontIndirect(&winfont);
            m_smallCapsFontData = new SimpleFontData(FontPlatformData(hfont, smallCapsHeight, m_font.syntheticBold(), m_font.syntheticOblique(), m_font.useGDI()));
        }
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: Support custom fonts.
    if (isCustomFont())
        return false;

    // FIXME: Microsoft documentation seems to imply that characters can be output using a given font and DC
    // merely by testing code page intersection.  This seems suspect though.  Can't a font only partially
    // cover a given code page?
    IMLangFontLink2* langFontLink = fontCache()->getFontLinkInterface();
    if (!langFontLink)
        return false;

    HDC dc = GetDC(0);
    
    DWORD acpCodePages;
    langFontLink->CodePageToCodePages(CP_ACP, &acpCodePages);

    DWORD fontCodePages;
    langFontLink->GetFontCodePages(dc, m_font.hfont(), &fontCodePages);

    DWORD actualCodePages;
    long numCharactersProcessed;
    long offset = 0;
    while (offset < length) {
        langFontLink->GetStrCodePages(characters, length, acpCodePages, &actualCodePages, &numCharactersProcessed);
        if ((actualCodePages & fontCodePages) == 0)
            return false;
        offset += numCharactersProcessed;
    }

    ReleaseDC(0, dc);

    return true;
}

void SimpleFontData::determinePitch()
{
    if (isCustomFont()) {
        m_treatAsFixedPitch = false;
        return;
    }

    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC(0);
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

float SimpleFontData::widthForGDIGlyph(Glyph glyph) const
{
    HDC hdc = GetDC(0);
    SetGraphicsMode(hdc, GM_ADVANCED);
    HGDIOBJ oldFont = SelectObject(hdc, m_font.hfont());
    int width;
    GetCharWidthI(hdc, glyph, 1, 0, &width);
    SelectObject(hdc, oldFont);
    ReleaseDC(0, hdc);
    return width + m_syntheticBoldOffset;
}

SCRIPT_FONTPROPERTIES* SimpleFontData::scriptFontProperties() const
{
    if (!m_scriptFontProperties) {
        m_scriptFontProperties = new SCRIPT_FONTPROPERTIES;
        memset(m_scriptFontProperties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_scriptFontProperties->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        HRESULT result = ScriptGetFontProperties(0, scriptCache(), m_scriptFontProperties);
        if (result == E_PENDING) {
            HDC dc = GetDC(0);
            SaveDC(dc);
            SelectObject(dc, m_font.hfont());
            ScriptGetFontProperties(dc, scriptCache(), m_scriptFontProperties);
            RestoreDC(dc, -1);
            ReleaseDC(0, dc);
        }
    }
    return m_scriptFontProperties;
}

}
