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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"
#include <ApplicationServices/ApplicationServices.h>

using std::min;

namespace WebCore {

static const int Bold = (1 << 0);
static const int Italic = (1 << 1);
static const int BoldOblique = (1 << 2);

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

FontPlatformData::FontPlatformData(HFONT font, int size, bool bold, bool oblique)
    : m_font(font)
    , m_size(size)
    , m_cgFont(0)
    , m_syntheticBold(false)
    , m_syntheticOblique(false)
{
    HDC hdc = GetDC(0);
    SaveDC(hdc);
    
    SelectObject(hdc, font);
    UINT bufferSize = GetOutlineTextMetrics(hdc, 0, NULL);

    ASSERT_WITH_MESSAGE(bufferSize != 0, "Bitmap fonts not supported with CoreGraphics.");

    if (bufferSize != 0)
    {
        OUTLINETEXTMETRICW* metrics = (OUTLINETEXTMETRICW*)malloc(bufferSize);

        GetOutlineTextMetricsW(hdc, bufferSize, metrics);
        WCHAR* faceName = (WCHAR*)((uintptr_t)metrics + (uintptr_t)metrics->otmpFaceName);

        if (bold || oblique) {
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

        CFStringRef cfName = CFStringCreateWithCharacters(NULL, (const UniChar*)faceName, wcslen(faceName));
        m_cgFont = CGFontCreateWithFontName(cfName);
        ASSERT(m_cgFont);
        CFRelease(cfName);
        
        free(metrics);
    }

    RestoreDC(hdc, -1);
    ReleaseDC(0, hdc);
}

FontPlatformData::~FontPlatformData()
{
}

}
