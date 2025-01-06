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

#if PLATFORM(WIN)

#include "CSSFontFaceSrcValue.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontMemoryResource.h"
#include "FontPlatformData.h"
#include "OpenTypeUtilities.h"
#include "SharedBuffer.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/Base64.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(const String& name, FontPlatformData::CreationData&& creationData)
    : name(name)
    , creationData(WTFMove(creationData))
    , m_renderingResourceIdentifier(RenderingResourceIdentifier::generate())
{
}

FontCustomPlatformData::~FontCustomPlatformData() = default;

static String createUniqueFontName()
{
    GUID fontUuid;
    CoCreateGuid(&fontUuid);

    auto fontName = base64EncodeToString({ reinterpret_cast<const uint8_t*>(&fontUuid), sizeof(fontUuid) });
    ASSERT(fontName.length() < LF_FACESIZE);
    return fontName;
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::create(SharedBuffer& buffer, const String& itemInCollection)
{
    String fontName = createUniqueFontName();
    auto fontResource = renameAndActivateFont(buffer, fontName);

    if (!fontResource)
        return nullptr;

    FontPlatformData::CreationData creationData = { buffer, itemInCollection, fontResource.releaseNonNull() };
    return adoptRef(new FontCustomPlatformData(fontName, WTFMove(creationData)));
}

RefPtr<FontCustomPlatformData> FontCustomPlatformData::createMemorySafe(SharedBuffer&, const String&)
{
    return nullptr;
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

bool FontCustomPlatformData::supportsTechnology(const FontTechnology& tech)
{
    switch (tech) {
    case FontTechnology::ColorCbdt:
    case FontTechnology::ColorColrv0:
    case FontTechnology::ColorSbix:
    case FontTechnology::ColorSvg:
    case FontTechnology::FeaturesOpentype:
        return true;
    default:
        return false;
    }
}

std::optional<Ref<FontCustomPlatformData>> FontCustomPlatformData::tryMakeFromSerializationData(FontCustomPlatformSerializedData&& data, bool)
{
    RefPtr fontCustomPlatformData = FontCustomPlatformData::create(WTFMove(data.fontFaceData), data.itemInCollection);
    if (!fontCustomPlatformData)
        return std::nullopt;
    fontCustomPlatformData->m_renderingResourceIdentifier = data.renderingResourceIdentifier;
    return fontCustomPlatformData.releaseNonNull();
}

FontCustomPlatformSerializedData FontCustomPlatformData::serializedData() const
{
    return FontCustomPlatformSerializedData { creationData.fontFaceData, creationData.itemInCollection, m_renderingResourceIdentifier };
}

} // namespace WebCore

#endif // PLATFORM(WIN)
