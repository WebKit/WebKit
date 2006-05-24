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
#include "FontCache.h"
#include "FontPlatformData.h"
#include "Font.h"
#include <windows.h>
#include <mlang.h>

namespace WebCore
{

void FontCache::platformInit()
{
    // Not needed on Windows.
}

IMLangFontLink2* FontCache::getFontLinkInterface()
{
    static IMultiLanguage *multiLanguage;
    if (!multiLanguage) {
        if (CoCreateInstance(CLSID_CMultiLanguage, 0, CLSCTX_ALL, IID_IMultiLanguage, (void**)&multiLanguage) != S_OK)
            return 0;
    }

    static IMLangFontLink2* langFontLink;
    if (!langFontLink) {
        if (multiLanguage->QueryInterface(IID_IMLangFontLink2, (void**)&langFontLink) != S_OK)
            return 0;
    }

    return langFontLink;
}

const FontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    // IMLangFontLink::MapFont Method does what we want.
    IMLangFontLink2* langFontLink = getFontLinkInterface();
    if (!langFontLink)
        return 0;

    FontData* fontData = 0;
    HDC hdc = GetDC(0);
    DWORD fontCodePages;
    langFontLink->GetFontCodePages(hdc, font.primaryFont()->m_font.hfont(), &fontCodePages);

    DWORD actualCodePages;
    long cchActual;
    langFontLink->GetStrCodePages(characters, length, fontCodePages, &actualCodePages, &cchActual);
    if (cchActual) {
        HFONT result;
        if (langFontLink->MapFont(hdc, actualCodePages, characters[0], &result) == S_OK) {
            fontData = new FontData(FontPlatformData(result, font.fontDescription().computedPixelSize()));
            fontData->setIsMLangFont();
        }
    }

    ReleaseDC(0, hdc);
    return fontData;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const Font& font)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    static AtomicString timesStr("Times New Roman");
    return getCachedFontPlatformData(font.fontDescription(), timesStr);
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    LOGFONT winfont;

    // The size here looks unusual.  The negative number is intentional.  The logical size constant is 32.
    winfont.lfHeight = WIN32_FONT_LOGICAL_SCALE * -fontDescription.computedPixelSize();
    winfont.lfWidth = 0;
    winfont.lfEscapement = 0;
    winfont.lfOrientation = 0;
    winfont.lfUnderline = false;
    winfont.lfStrikeOut = false;
    winfont.lfCharSet = DEFAULT_CHARSET;
    winfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    winfont.lfQuality = 5; // Force cleartype.
    winfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont.lfItalic = fontDescription.italic();
    winfont.lfWeight = fontDescription.bold() ? 700 : 400; // FIXME: Support weights for real.
    int len = min(family.length(), LF_FACESIZE - 1);
    memcpy(winfont.lfFaceName, family.characters(), len * sizeof(WORD));
    winfont.lfFaceName[len] = '\0';

    HFONT hfont = CreateFontIndirect(&winfont);
    // Windows will always give us a valid pointer here, even if the face name is non-existent.  We have to double-check
    // and see if the family name was really used.
    HDC dc = GetDC((HWND)0);
    SaveDC(dc);
    SelectObject(dc, hfont);
    WCHAR name[LF_FACESIZE];
    unsigned resultLength = GetTextFace(dc, LF_FACESIZE, name);
    if (resultLength > 0)
        resultLength--; // ignore the null terminator
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
    if (!equalIgnoringCase(family, String(name, resultLength))) {
        DeleteObject(hfont);
        return 0;
    }
    
    return new FontPlatformData(hfont, fontDescription.computedPixelSize());
}

}