/*
 * Copyright (C) 2007-2023 Apple Inc.
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

#include "CSSFontFaceSrcValue.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontMemoryResource.h"
#include "FontPlatformData.h"
#include "OpenTypeUtilities.h"
#include "SharedBuffer.h"
#include <cairo-win32.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/Base64.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

cairo_font_face_t* createCairoDWriteFontFace(HFONT);

FontCustomPlatformData::FontCustomPlatformData(const String& name, FontPlatformData::CreationData&& creationData)
    : name(name)
    , creationData(WTFMove(creationData))
    , m_renderingResourceIdentifier(RenderingResourceIdentifier::generate())
{
}

FontCustomPlatformData::~FontCustomPlatformData() = default;

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

static String createUniqueFontName()
{
    GUID fontUuid;
    CoCreateGuid(&fontUuid);

    auto fontName = base64EncodeToString(reinterpret_cast<char*>(&fontUuid), sizeof(fontUuid));
    ASSERT(fontName.length() < LF_FACESIZE);
    return fontName;
}

RefPtr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& itemInCollection)
{
    String fontName = createUniqueFontName();
    auto fontResource = renameAndActivateFont(buffer, fontName);

    if (!fontResource)
        return nullptr;

    FontPlatformData::CreationData creationData = { buffer, itemInCollection, fontResource.releaseNonNull() };
    return adoptRef(new FontCustomPlatformData(fontName, WTFMove(creationData)));
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype"_s)
        || equalLettersIgnoringASCIICase(format, "opentype"_s)
#if USE(WOFF2)
        || equalLettersIgnoringASCIICase(format, "woff2"_s)
#endif
        || equalLettersIgnoringASCIICase(format, "woff"_s)
        || equalLettersIgnoringASCIICase(format, "svg"_s);
}

bool FontCustomPlatformData::supportsTechnology(const FontTechnology&)
{
    // FIXME: define supported technologies for this platform (webkit.org/b/256310).
    return true;
}

}
