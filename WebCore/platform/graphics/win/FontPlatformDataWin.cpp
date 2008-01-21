/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006, 2007 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"

#include "PlatformString.h"
#include "StringHash.h"
#include <ApplicationServices/ApplicationServices.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

using std::min;

namespace WebCore {

static const int Bold = (1 << 0);
static const int Italic = (1 << 1);
static const int BoldOblique = (1 << 2);

static inline USHORT readBigEndianWord(const BYTE* word) { return (word[0] << 8) | word[1]; }

static CFStringRef getPostScriptName(CFStringRef faceName, HDC dc)
{
    const DWORD cMaxNameTableSize = 1024 * 1024;

    static HashMap<String, RetainPtr<CFStringRef> > nameMap;

    // Check our hash first.
    String faceString(faceName);
    RetainPtr<CFStringRef> result = nameMap.get(faceString);
    if (result)
        return result.get();

    // We need to obtain the PostScript name from the name table and use it instead,.
    DWORD bufferSize = GetFontData(dc, 'eman', 0, NULL, 0); // "name" backwards
    if (bufferSize == 0 || bufferSize == GDI_ERROR || bufferSize > cMaxNameTableSize)
        return NULL;
   
    Vector<BYTE> bufferVector(bufferSize);
    BYTE* buffer = bufferVector.data();
    if (GetFontData(dc, 'eman', 0, buffer, bufferSize) == GDI_ERROR)
        return NULL;

    if (bufferSize < 6)
        return NULL;

    USHORT numberOfRecords = readBigEndianWord(buffer + 2);
    UINT stringsOffset = readBigEndianWord(buffer + 4);
    if (bufferSize < stringsOffset)
        return NULL;

    BYTE* strings = buffer + stringsOffset;

    // Now walk each name record looking for a Mac or Windows PostScript name.
    UINT offset = 6;
    for (int i = 0; i < numberOfRecords; i++) {
        if (bufferSize < offset + 12)
            return NULL;

        USHORT platformID = readBigEndianWord(buffer + offset);
        USHORT encodingID = readBigEndianWord(buffer + offset + 2);
        USHORT languageID = readBigEndianWord(buffer + offset + 4);
        USHORT nameID = readBigEndianWord(buffer + offset + 6);
        USHORT length = readBigEndianWord(buffer + offset + 8);
        USHORT nameOffset = readBigEndianWord(buffer + offset + 10);

        if (platformID == 3 && encodingID == 1 && languageID == 0x409 && nameID == 6) {
            // This is a Windows PostScript name and is therefore UTF-16.
            // Pass Big Endian as the encoding.
            if (bufferSize < stringsOffset + nameOffset + length)
                return NULL;
            result.adoptCF(CFStringCreateWithBytes(NULL, strings + nameOffset, length, kCFStringEncodingUTF16BE, false));
            break;
        } else if (platformID == 1 && encodingID == 0 && languageID == 0 && nameID == 6) {
            // This is a Mac PostScript name and is therefore ASCII.
            // See http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
            if (bufferSize < stringsOffset + nameOffset + length)
                return NULL;
            result.adoptCF(CFStringCreateWithBytes(NULL, strings + nameOffset, length, kCFStringEncodingASCII, false));
            break;
        }

        offset += 12;
    }

    if (result)
        nameMap.set(faceString, result);
    return result.get();
}

static int CALLBACK enumStylesCallback(const LOGFONT* logFont, const TEXTMETRIC* metrics, DWORD fontType, LPARAM lParam)
{
    int *style = reinterpret_cast<int*>(lParam);

    // FIXME: In order to accommodate Lucida we go ahead and consider a weight of 600 to be bold.
    // This does mean we'll consider demibold and semibold fonts on windows to also be bold.  This
    // is rare enough that it seems like an ok hack for now.
    if (logFont->lfWeight >= 600) {
        if (logFont->lfItalic)
            *style |= BoldOblique;
        else
            *style |= Bold;
    } else if (logFont->lfItalic)
        *style |= Italic;

    return 1;
}

FontPlatformData::FontPlatformData(HFONT font, float size, bool bold, bool oblique, bool useGDI)
    : m_font(font)
    , m_size(size)
    , m_cgFont(0)
    , m_syntheticBold(false)
    , m_syntheticOblique(false)
    , m_useGDI(useGDI)
{
    HDC hdc = GetDC(0);
    SaveDC(hdc);
    
    SelectObject(hdc, font);
    UINT bufferSize = GetOutlineTextMetrics(hdc, 0, NULL);

    ASSERT_WITH_MESSAGE(bufferSize != 0, "Bitmap fonts not supported with CoreGraphics.");

    if (bufferSize != 0) {
        OUTLINETEXTMETRICW* metrics = (OUTLINETEXTMETRICW*)malloc(bufferSize);

        GetOutlineTextMetricsW(hdc, bufferSize, metrics);
        WCHAR* faceName = (WCHAR*)((uintptr_t)metrics + (uintptr_t)metrics->otmpFaceName);

        if (!useGDI && (bold || oblique)) {
            LOGFONT logFont;

            int len = min((int)wcslen(faceName), LF_FACESIZE - 1);
            memcpy(logFont.lfFaceName, faceName, len * sizeof(WORD));
            logFont.lfFaceName[len] = '\0';
            logFont.lfCharSet = metrics->otmTextMetrics.tmCharSet;
            logFont.lfPitchAndFamily = 0;

            int styles = 0;
            EnumFontFamiliesEx(hdc, &logFont, enumStylesCallback, reinterpret_cast<LPARAM>(&styles), 0);

            // Check if we need to synthesize bold or oblique. The rule that complicates things here
            // is that if the requested font is bold and oblique, and both a bold font and an oblique font
            // exist, the bold font should be used, and oblique synthesized.
            if (bold && oblique) {
                if (styles == 0) {
                    m_syntheticBold = true;
                    m_syntheticOblique = true;
                } else if (styles & Bold)
                    m_syntheticOblique = true;
                else if (styles & Italic)
                    m_syntheticBold = true;
            } else if (bold && (!(styles & Bold)))
                    m_syntheticBold = true;
              else if (oblique && !(styles & Italic))
                    m_syntheticOblique = true;
        }

        // Try the face name first.  Windows may end up localizing this name, and CG doesn't know about
        // the localization.  If the create fails, we'll try the PostScript name.
        RetainPtr<CFStringRef> fullName(AdoptCF, CFStringCreateWithCharacters(NULL, (const UniChar*)faceName, wcslen(faceName)));
        m_cgFont = CGFontCreateWithFontName(fullName.get());
        if (!m_cgFont) {
            CFStringRef postScriptName = getPostScriptName(fullName.get(), hdc);
            if (postScriptName) {
                m_cgFont = CGFontCreateWithFontName(postScriptName);
                ASSERT(m_cgFont);
            }
        }
        free(metrics);
    }

    RestoreDC(hdc, -1);
    ReleaseDC(0, hdc);
}

FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_font(0)
    , m_size(size)
    , m_cgFont(0)
    , m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
    , m_useGDI(false)
{
}

FontPlatformData::FontPlatformData(CGFontRef font, float size, bool bold, bool oblique)
    : m_font(0)
    , m_size(size)
    , m_cgFont(font)
    , m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
    , m_useGDI(false)
{
}

FontPlatformData::~FontPlatformData()
{
}

}
