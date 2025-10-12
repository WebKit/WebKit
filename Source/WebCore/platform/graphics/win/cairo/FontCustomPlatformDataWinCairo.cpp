/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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
#include "FontCustomPlatformData.h"

#if PLATFORM(WIN) && USE(CAIRO)

#include "FontDescription.h"
#include <cairo-win32.h>

namespace WebCore {

cairo_font_face_t* createCairoDWriteFontFace(HFONT);

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext&)
{
    auto size = fontDescription.computedSize();

    LOGFONT logFont;
    memset(&logFont, 0, sizeof(LOGFONT));
    wcsncpy(logFont.lfFaceName, name.wideCharacters().data(), LF_FACESIZE - 1);

    logFont.lfHeight = -size * cWindowsFontScaleFactor;
    logFont.lfWidth = 0;
    logFont.lfEscapement = 0;
    logFont.lfOrientation = 0;
    logFont.lfUnderline = false;
    logFont.lfStrikeOut = false;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    logFont.lfQuality = CLEARTYPE_QUALITY;
    logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    logFont.lfItalic = italic;
    logFont.lfWeight = bold ? 700 : 400;

    auto hfont = adoptGDIObject(::CreateFontIndirect(&logFont));

    cairo_font_face_t* fontFace = createCairoDWriteFontFace(hfont.get());

    FontPlatformData fontPlatformData(WTFMove(hfont), fontFace, size, bold, italic, this);

    cairo_font_face_destroy(fontFace);

    return fontPlatformData;
}

} // namespace WebCore

#endif // PLATFORM(WIN) && USE(CAIRO)
