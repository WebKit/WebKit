/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/MathExtras.h>
// Unicode.h needs to be included before fontprops.h for UChar* to be defined.
// FIXME: This is wrong, fontprops.h should just forward-declare UChar.
#include <wtf/unicode/Unicode.h>

#include "fontprops.h"
#include "math.h"

#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/wx.h>
#include "wx/msw/private.h"

#include <mlang.h>
#include <usp10.h>

inline long  my_round(double x)
{
    return (long)(x < 0 ? x - 0.5 : x + 0.5);
}

wxFontProperties::wxFontProperties(wxFont* font):
m_ascent(0), m_descent(0), m_lineGap(0), m_lineSpacing(0), m_xHeight(0)
{
    HDC dc = GetDC(0);
    WXHFONT hFont = font->GetHFONT();
    ::SelectObject(dc, hFont);
    if (font){
       
        int height = font->GetPointSize();

        TEXTMETRIC tm;
        GetTextMetrics(dc, &tm);
        m_ascent = lroundf(tm.tmAscent);
        m_descent = lroundf(tm.tmDescent);
        m_xHeight = m_ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.
        m_lineGap = lroundf(tm.tmExternalLeading);
        m_lineSpacing = m_lineGap + m_ascent + m_descent;
    }
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

bool wxFontContainsCharacters(void* font, const UChar* characters, int length)
{
    // FIXME: Microsoft documentation seems to imply that characters can be output using a given font and DC
    // merely by testing code page intersection.  This seems suspect though.  Can't a font only partially
    // cover a given code page?
    static IMultiLanguage *multiLanguage;
    if (!multiLanguage) {
        if (CoCreateInstance(CLSID_CMultiLanguage, 0, CLSCTX_ALL, IID_IMultiLanguage, (void**)&multiLanguage) != S_OK)
            return true;
    }

    static IMLangFontLink2* langFontLink;
    if (!langFontLink) {
        if (multiLanguage->QueryInterface(&langFontLink) != S_OK)
            return true;
    }

    HDC dc = GetDC(0);
    
    DWORD acpCodePages;
    langFontLink->CodePageToCodePages(CP_ACP, &acpCodePages);

    DWORD fontCodePages;
    langFontLink->GetFontCodePages(dc, (HFONT)font, &fontCodePages);

    DWORD actualCodePages;
    long numCharactersProcessed;
    long offset = 0;
    while (offset < length) {
        langFontLink->GetStrCodePages(characters, length, acpCodePages, &actualCodePages, &numCharactersProcessed);
        if ((actualCodePages & fontCodePages))
            return false;
        offset += numCharactersProcessed;
    }

    ReleaseDC(0, dc);

    return true;
}

void GetTextExtent( const wxFont& font, const wxString& str, wxCoord *width, wxCoord *height,
                            wxCoord *descent, wxCoord *externalLeading )
{
    HDC dc = GetDC(0);
    WXHFONT hFont = font.GetHFONT();
    ::SelectObject(dc, hFont);

    HFONT hfontOld;
    if ( font != wxNullFont )
    {
        wxASSERT_MSG( font.Ok(), _T("invalid font in wxDC::GetTextExtent") );

        hfontOld = (HFONT)::SelectObject(dc, hFont);
    }
    else // don't change the font
    {
        hfontOld = 0;
    }

    SIZE sizeRect;
    const size_t len = str.length();
    if ( !::GetTextExtentPoint32(dc, str, len, &sizeRect) )
    {
        wxLogLastError(_T("GetTextExtentPoint32()"));
    }

#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)
    // the result computed by GetTextExtentPoint32() may be too small as it
    // accounts for under/overhang of the first/last character while we want
    // just the bounding rect for this string so adjust the width as needed
    // (using API not available in 2002 SDKs of WinCE)
    if ( len > 1 )
    {
        ABC width;
        const wxChar chFirst = *str.begin();
        if ( ::GetCharABCWidths(dc, chFirst, chFirst, &width) )
        {
            if ( width.abcA < 0 )
                sizeRect.cx -= width.abcA;

            if ( len > 1 )
            {
                const wxChar chLast = *str.rbegin();
                ::GetCharABCWidths(dc, chLast, chLast, &width);
            }
            //else: we already have the width of the last character

            if ( width.abcC < 0 )
                sizeRect.cx -= width.abcC;
        }
        //else: GetCharABCWidths() failed, not a TrueType font?
    }
#endif // !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)

    TEXTMETRIC tm;
    ::GetTextMetrics(dc, &tm);

    if (width)
        *width = sizeRect.cx;
    if (height)
        *height = sizeRect.cy;
    if (descent)
        *descent = tm.tmDescent;
    if (externalLeading)
        *externalLeading = tm.tmExternalLeading;

    if ( hfontOld )
    {
        ::SelectObject(dc, hfontOld);
    }
    
    ReleaseDC(0, dc);
}
