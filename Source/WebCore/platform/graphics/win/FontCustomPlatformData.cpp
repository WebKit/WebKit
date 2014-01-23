/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

#include "FontPlatformData.h"
#include "OpenTypeUtilities.h"
#include "SharedBuffer.h"
#include <ApplicationServices/ApplicationServices.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/Base64.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

using namespace std;

FontCustomPlatformData::~FontCustomPlatformData()
{
    if (m_fontReference)
            RemoveFontMemResourceEx(m_fontReference);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontOrientation, FontWidthVariant, FontRenderingMode renderingMode)
{
    ASSERT(m_fontReference);

    LOGFONT& logFont = *static_cast<LOGFONT*>(malloc(sizeof(LOGFONT)));
    memcpy(logFont.lfFaceName, m_name.charactersWithNullTermination().data(), sizeof(logFont.lfFaceName[0]) * std::min<size_t>(static_cast<size_t>(LF_FACESIZE), 1 + m_name.length()));

    logFont.lfHeight = -size;
    if (renderingMode == NormalRenderingMode)
        logFont.lfHeight *= 32;
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

    RetainPtr<CGFontRef> cgFont = adoptCF(CGFontCreateWithPlatformFont(&logFont));
    return FontPlatformData(std::move(hfont), cgFont.get(), size, bold, italic, renderingMode == AlternateRenderingMode);
}

// Creates a unique and unpredictable font name, in order to avoid collisions and to
// not allow access from CSS.
static String createUniqueFontName()
{
    GUID fontUuid;
    CoCreateGuid(&fontUuid);

    String fontName = base64Encode(reinterpret_cast<char*>(&fontUuid), sizeof(fontUuid));
    ASSERT(fontName.length() < LF_FACESIZE);
    return fontName;
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer)
{
    String fontName = createUniqueFontName();
    HANDLE fontReference;
    fontReference = renameAndActivateFont(buffer, fontName);
    if (!fontReference)
        return nullptr;
    return std::make_unique<FontCustomPlatformData>(fontReference, fontName);
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || equalIgnoringCase(format, "woff");
}

}
