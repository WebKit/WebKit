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
#include "FontCache.h"
#include "Font.h"
#include "SimpleFontData.h"
#include "StringHash.h"
#include <windows.h>
#include <mlang.h>
#include <ApplicationServices/ApplicationServices.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

using std::min;

namespace WebCore
{

void FontCache::platformInit()
{
    wkSetUpFontCache(1536 * 1024 * 4); // This size matches Mac.
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
        if (multiLanguage->QueryInterface(&langFontLink) != S_OK)
            return 0;
    }

    return langFontLink;
}

static int CALLBACK metaFileEnumProc(HDC hdc, HANDLETABLE* table, CONST ENHMETARECORD* record, int tableEntries, LPARAM logFont)
{
    if (record->iType == EMR_EXTCREATEFONTINDIRECTW) {
        const EMREXTCREATEFONTINDIRECTW* createFontRecord = reinterpret_cast<const EMREXTCREATEFONTINDIRECTW*>(record);
        *reinterpret_cast<LOGFONT*>(logFont) = createFontRecord->elfw.elfLogFont;
    }
    return true;
}

static int CALLBACK linkedFontEnumProc(CONST LOGFONT* logFont, CONST TEXTMETRIC* metrics, DWORD fontType, LPARAM hfont)
{
    *reinterpret_cast<HFONT*>(hfont) = CreateFontIndirect(logFont);
    return false;
}

static const Vector<String>* getLinkedFonts(String& family)
{
    static HashMap<String, Vector<String>*> systemLinkMap;
    Vector<String>* result = systemLinkMap.get(family);
    if (result)
        return result;

    result = new Vector<String>;
    systemLinkMap.set(family, result);
    HKEY fontLinkKey;
    if (FAILED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink", 0, KEY_READ, &fontLinkKey)))
        return result;

    DWORD linkedFontsBufferSize = 0;
    RegQueryValueEx(fontLinkKey, family.charactersWithNullTermination(), 0, NULL, NULL, &linkedFontsBufferSize);
    WCHAR* linkedFonts = reinterpret_cast<WCHAR*>(malloc(linkedFontsBufferSize));
    if (SUCCEEDED(RegQueryValueEx(fontLinkKey, family.charactersWithNullTermination(), 0, NULL, reinterpret_cast<BYTE*>(linkedFonts), &linkedFontsBufferSize))) {
        unsigned i = 0;
        unsigned length = linkedFontsBufferSize / sizeof(*linkedFonts);
        while (i < length) {
            while (i < length && linkedFonts[i] != ',')
                i++;
            i++;
            unsigned j = i;
            while (j < length && linkedFonts[j])
                j++;
            result->append(String(linkedFonts + i, j - i));
            i = j + 1;
        }
    }
    free(linkedFonts);
    RegCloseKey(fontLinkKey);
    return result;
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    SimpleFontData* fontData = 0;
    HDC hdc = GetDC(0);
    HFONT primaryFont = font.primaryFont()->fontDataForCharacter(characters[0])->m_font.hfont();
    HGDIOBJ oldFont = SelectObject(hdc, primaryFont);
    HFONT hfont = 0;

    // Special-case characters in the range U+2000..U+200F (spaces, zero width joiner and non-joiner,
    // right to left and left to right marks). Uniscribe does not tell us what font it will use for these
    // (presumably because it never renders their glyphs), so we use MapFont to find a font that contains
    // them.
    if (characters[0] >= 0x2000 && characters[0] <= 0x200F) {
        IMLangFontLink2* langFontLink = getFontLinkInterface();
        if (langFontLink) {
            HFONT mLangFont;
            langFontLink->MapFont(hdc, 0, characters[0], &mLangFont);
            LOGFONT lf;
            GetObject(mLangFont, sizeof(LOGFONT), &lf);
            langFontLink->ReleaseFont(mLangFont);
            hfont = CreateFontIndirect(&lf);
        }
    } else {
        // To find out what font Uniscribe would use, we make it draw into a metafile and intercept
        // calls to CreateFontIndirect().
        HDC metaFileDc = CreateEnhMetaFile(hdc, NULL, NULL, NULL);
        SelectObject(metaFileDc, primaryFont);

        bool scriptStringOutSucceeded = false;
        SCRIPT_STRING_ANALYSIS ssa;

        // FIXME: If length is greater than 1, we actually return the font for the last character.
        // This function should be renamed getFontDataForCharacter and take a single 32-bit character.
        if (SUCCEEDED(ScriptStringAnalyse(metaFileDc, characters, length, 0, -1, SSA_METAFILE | SSA_FALLBACK | SSA_GLYPHS | SSA_LINK,
            0, NULL, NULL, NULL, NULL, NULL, &ssa))) {
            scriptStringOutSucceeded = SUCCEEDED(ScriptStringOut(ssa, 0, 0, 0, NULL, 0, 0, FALSE));
            ScriptStringFree(&ssa);
        }
        HENHMETAFILE metaFile = CloseEnhMetaFile(metaFileDc);
        if (scriptStringOutSucceeded) {
            LOGFONT logFont;
            logFont.lfFaceName[0] = 0;
            EnumEnhMetaFile(0, metaFile, metaFileEnumProc, &logFont, NULL);
            if (logFont.lfFaceName[0])
                hfont = CreateFontIndirect(&logFont);
        }
        DeleteEnhMetaFile(metaFile);
    }

    String familyName;
    const Vector<String>* linkedFonts = 0;
    unsigned linkedFontIndex = 0;
    while (hfont) {
        SelectObject(hdc, hfont);
        WCHAR name[LF_FACESIZE];
        GetTextFace(hdc, LF_FACESIZE, name);
        familyName = name;
        // Check if the font contains the desired character.
        static Vector<char, 512> glyphsetBuffer;
        glyphsetBuffer.resize(GetFontUnicodeRanges(hdc, NULL));
        GLYPHSET* glyphset = reinterpret_cast<GLYPHSET*>(glyphsetBuffer.data());
        GetFontUnicodeRanges(hdc, glyphset);
        // FIXME: Change this to a binary search.
        unsigned i = 0;
        while (i < glyphset->cRanges && glyphset->ranges[i].wcLow <= characters[0])
            i++;
        if (i && glyphset->ranges[i - 1].wcLow + glyphset->ranges[i - 1].cGlyphs > characters[0])
            break;

        if (!linkedFonts)
            linkedFonts = getLinkedFonts(familyName);
        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
        hfont = 0;

        if (linkedFonts->size() <= linkedFontIndex)
            break;

        LOGFONT logFont;
        logFont.lfCharSet = DEFAULT_CHARSET;
        memcpy(logFont.lfFaceName, linkedFonts->at(linkedFontIndex).characters(), linkedFonts->at(linkedFontIndex).length() * sizeof(WCHAR));
        logFont.lfFaceName[linkedFonts->at(linkedFontIndex).length()] = 0;
        EnumFontFamiliesEx(hdc, &logFont, linkedFontEnumProc, reinterpret_cast<LPARAM>(&hfont), 0);
        linkedFontIndex++;
    }
    
    if (hfont) {
        if (!familyName.isEmpty()) {
            FontPlatformData* result = getCachedFontPlatformData(font.fontDescription(), familyName);
            if (result)
                fontData = getCachedFontData(result);
        }

        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
    }

    ReleaseDC(0, hdc);
    return fontData;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.  For now we'll pick
    // the default that the user would get without changing any prefs.
    static AtomicString timesStr("Times New Roman");
    return getCachedFontPlatformData(fontDescription, timesStr);
}

bool FontCache::fontExists(const FontDescription& fontDescription, const AtomicString& family)
{
    LOGFONT winfont;

    // The size here looks unusual.  The negative number is intentional.  The logical size constant is 32.
    winfont.lfHeight = -fontDescription.computedPixelSize() * 32;
    winfont.lfWidth = 0;
    winfont.lfEscapement = 0;
    winfont.lfOrientation = 0;
    winfont.lfUnderline = false;
    winfont.lfStrikeOut = false;
    winfont.lfCharSet = DEFAULT_CHARSET;
#if PLATFORM(CG)
    winfont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
#else
    winfont.lfOutPrecision = OUT_TT_PRECIS;
#endif
    winfont.lfQuality = 5; // Force cleartype.
    winfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont.lfItalic = fontDescription.italic();

    // FIXME: Support weights for real.  Do our own enumeration of the available weights.
    // We can't rely on Windows here, since we need to follow the CSS2 algorithm for how to fill in
    // gaps in the weight list.
    // FIXME: Hardcoding Lucida Grande for now.  It uses different weights than typical Win32 fonts
    // (500/600 instead of 400/700).
    static AtomicString lucidaStr("Lucida Grande");
    if (equalIgnoringCase(family, lucidaStr))
        winfont.lfWeight = fontDescription.bold() ? 600 : 500;
    else
        winfont.lfWeight = fontDescription.bold() ? 700 : 400;
    int len = min(family.length(), (unsigned int)LF_FACESIZE - 1);
    memcpy(winfont.lfFaceName, family.characters(), len * sizeof(WORD));
    winfont.lfFaceName[len] = '\0';

    HFONT hfont = CreateFontIndirect(&winfont);
    // Windows will always give us a valid pointer here, even if the face name is non-existent.  We have to double-check
    // and see if the family name was really used.
    HDC dc = GetDC(0);
    SaveDC(dc);
    SelectObject(dc, hfont);
    WCHAR name[LF_FACESIZE];
    GetTextFace(dc, LF_FACESIZE, name);
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    DeleteObject(hfont);

    return !wcsicmp(winfont.lfFaceName, name);
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    bool isLucidaGrande = false;
    static AtomicString lucidaStr("Lucida Grande");
    if (equalIgnoringCase(family, lucidaStr))
        isLucidaGrande = true;

    bool useGDI = fontDescription.renderingMode() == AlternateRenderingMode && !isLucidaGrande;

    LOGFONT winfont;

    // The size here looks unusual.  The negative number is intentional.  The logical size constant is 32. We do this
    // for subpixel precision when rendering using Uniscribe.  This masks rounding errors related to the HFONT metrics being
    // different from the CGFont metrics.
    // FIXME: We will eventually want subpixel precision for GDI mode, but the scaled rendering doesn't look as nice.  That may be solvable though.
    winfont.lfHeight = -fontDescription.computedPixelSize() * (useGDI ? 1 : 32);
    winfont.lfWidth = 0;
    winfont.lfEscapement = 0;
    winfont.lfOrientation = 0;
    winfont.lfUnderline = false;
    winfont.lfStrikeOut = false;
    winfont.lfCharSet = DEFAULT_CHARSET;
    winfont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    winfont.lfQuality = DEFAULT_QUALITY;
    winfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    winfont.lfItalic = fontDescription.italic();

    // FIXME: Support weights for real.  Do our own enumeration of the available weights.
    // We can't rely on Windows here, since we need to follow the CSS2 algorithm for how to fill in
    // gaps in the weight list.
    // FIXME: Hardcoding Lucida Grande for now.  It uses different weights than typical Win32 fonts
    // (500/600 instead of 400/700).
    if (isLucidaGrande) {
        winfont.lfWeight = fontDescription.bold() ? 600 : 500;
        useGDI = false; // Never use GDI for Lucida Grande.
    } else
        winfont.lfWeight = fontDescription.bold() ? 700 : 400;
    int len = min(family.length(), (unsigned int)LF_FACESIZE - 1);
    memcpy(winfont.lfFaceName, family.characters(), len * sizeof(WORD));
    winfont.lfFaceName[len] = '\0';

    HFONT hfont = CreateFontIndirect(&winfont);
    // Windows will always give us a valid pointer here, even if the face name is non-existent.  We have to double-check
    // and see if the family name was really used.
    HDC dc = GetDC(0);
    SaveDC(dc);
    SelectObject(dc, hfont);
    WCHAR name[LF_FACESIZE];
    GetTextFace(dc, LF_FACESIZE, name);
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    if (_wcsicmp(winfont.lfFaceName, name)) {
        DeleteObject(hfont);
        return 0;
    }
    
    FontPlatformData* result = new FontPlatformData(hfont, fontDescription.computedPixelSize(),
                                                    fontDescription.bold(), fontDescription.italic(), useGDI);
    if (!result->cgFont()) {
        // The creation of the CGFontRef failed for some reason.  We already asserted in debug builds, but to make
        // absolutely sure that we don't use this font, go ahead and return 0 so that we can fall back to the next
        // font.
        delete result;
        DeleteObject(hfont);
        return 0;
    }        

    return result;
}

}

