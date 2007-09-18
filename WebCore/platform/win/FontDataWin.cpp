/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "FontData.h"
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

static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return unitsPerEm ? x / (float)unitsPerEm : x; }

void FontData::platformInit()
{    
    HDC dc = GetDC(0);
    SaveDC(dc);

    SelectObject(dc, m_font.hfont());

    int faceLength = GetTextFace(dc, 0, 0);
    Vector<TCHAR> faceName(faceLength);
    GetTextFace(dc, faceLength, faceName.data());

    m_syntheticBoldOffset = m_font.syntheticBold() ? 1.0f : 0.f;

    CGFontRef font = m_font.cgFont();
    int iAscent = CGFontGetAscent(font);
    int iDescent = CGFontGetDescent(font);
    int iLineGap = CGFontGetLeading(font);
    unsigned unitsPerEm = CGFontGetUnitsPerEm(font);
    float pointSize = m_font.size();
    float fAscent = scaleEmToUnits(iAscent, unitsPerEm) * pointSize;
    float fDescent = -scaleEmToUnits(iDescent, unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, unitsPerEm) * pointSize;

    m_isSystemFont = !_tcscmp(faceName.data(), _T("Lucida Grande"));
    
    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard. The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    if (!_tcscmp(faceName.data(), _T("Times")) || !_tcscmp(faceName.data(), _T("Helvetica")) || !_tcscmp(faceName.data(), _T("Courier")))
        fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);

    m_ascent = lroundf(fAscent);
    m_descent = lroundf(fDescent);
    m_lineGap = lroundf(fLineGap);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    Glyph xGlyph = GlyphPageTreeNode::getRootChild(this, 0)->page()->glyphDataForCharacter('x').glyph;
    if (xGlyph) {
        CGRect xBox;
        CGFontGetGlyphBBoxes(font, &xGlyph, 1, &xBox);
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        m_xHeight = scaleEmToUnits(max(CGRectGetMaxX(xBox), CGRectGetMaxY(xBox)), unitsPerEm) * pointSize;
    } else {
        int iXHeight = CGFontGetXHeight(font);
        m_xHeight = scaleEmToUnits(iXHeight, unitsPerEm) * pointSize;
    }

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    m_scriptCache = 0;
    m_scriptFontProperties = 0;
}

void FontData::platformDestroy()
{
    CGFontRelease(m_font.cgFont());
    DeleteObject(m_font.hfont());

    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;

    ScriptFreeCache(&m_scriptCache);
    delete m_scriptFontProperties;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        LOGFONT winfont;
        GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
        int smallCapsHeight = lroundf(0.70f * fontDescription.computedSize());
        winfont.lfHeight = -smallCapsHeight * 32;
        HFONT hfont = CreateFontIndirect(&winfont);
        m_smallCapsFontData = new FontData(FontPlatformData(hfont, smallCapsHeight, fontDescription.bold(), fontDescription.italic()));
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: Microsoft documentation seems to imply that characters can be output using a given font and DC
    // merely by testing code page intersection.  This seems suspect though.  Can't a font only partially
    // cover a given code page?
    IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
    if (!langFontLink)
        return false;

    HDC dc = GetDC((HWND)0);
    
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
    CGFontRef font = m_font.cgFont();
    float pointSize = m_font.size();
    CGSize advance;
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    // FIXME: Need to add real support for printer fonts.
    bool isPrinterFont = false;
    wkGetGlyphAdvances(font, m, m_isSystemFont, isPrinterFont, glyph, advance);
    return advance.width + m_syntheticBoldOffset;
}

SCRIPT_FONTPROPERTIES* FontData::scriptFontProperties() const
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
