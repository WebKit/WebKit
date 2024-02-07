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

#include "CSSFontFaceSrcValue.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCacheCoreText.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "StyleFontSizeFunctions.h"
#include "UnrealizedCoreTextFont.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    auto size = fontDescription.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    UnrealizedCoreTextFont unrealizedFont = { RetainPtr { fontDescriptor } };
    unrealizedFont.setSize(size);
    unrealizedFont.modify([&](CFMutableDictionaryRef attributes) {
        addAttributesForWebFonts(attributes, fontDescription.shouldAllowUserInstalledFonts());
    });

    FontOrientation orientation = fontDescription.orientation();
    FontWidthVariant widthVariant = fontDescription.widthVariant();

    auto font = preparePlatformFont(WTFMove(unrealizedFont), fontDescription, fontCreationContext);
    ASSERT(font);
    FontPlatformData platformData(font.get(), size, bold, italic, orientation, widthVariant, fontDescription.textRenderingMode(), this);

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust(), fontDescription.computedSize());
    return platformData;
}

RefPtr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& itemInCollection)
{
    RetainPtr<CFDataRef> bufferData = buffer.createCFData();

    FPFontRef font = nullptr;
    auto array = adoptCF(FPFontCreateFontsFromData(bufferData.get()));
    if (!array)
        return nullptr;
    auto length = CFArrayGetCount(array.get());
    if (length <= 0)
        return nullptr;
    if (!itemInCollection.isNull()) {
        if (auto desiredName = itemInCollection.createCFString()) {
            for (CFIndex i = 0; i < length; ++i) {
                auto candidate = static_cast<FPFontRef>(CFArrayGetValueAtIndex(array.get(), i));
                auto postScriptName = adoptCF(FPFontCopyPostScriptName(candidate));
                if (CFStringCompare(postScriptName.get(), desiredName.get(), 0) == kCFCompareEqualTo) {
                    font = candidate;
                    break;
                }
            }
        }
    }
    if (!font)
        font = static_cast<FPFontRef>(CFArrayGetValueAtIndex(array.get(), 0));

    // Retain the extracted font contents, so the GPU process doesn't have to extract it a second time later.
    // This is a power optimization.
    auto extractedData = adoptCF(FPFontCopySFNTData(font));
    if (!extractedData) {
        // Something is wrong with the font.
        return nullptr;
    }
    auto fontDescriptor = adoptCF(CTFontManagerCreateFontDescriptorFromData(extractedData.get()));
    auto protectedBuffer = SharedBuffer::create(extractedData.get());

    FontPlatformData::CreationData creationData = { WTFMove(protectedBuffer), itemInCollection };
    return adoptRef(new FontCustomPlatformData(fontDescriptor.get(), WTFMove(creationData)));
}

std::optional<Ref<FontCustomPlatformData>> FontCustomPlatformData::tryMakeFromSerializationData(FontCustomPlatformSerializedData&& data)
{
    auto buffer = SharedBuffer::create(WTFMove(data.fontFaceData));
    auto fontCustomPlatformData = createFontCustomPlatformData(buffer, data.itemInCollection);
    if (!fontCustomPlatformData)
        return std::nullopt;
    fontCustomPlatformData->m_renderingResourceIdentifier = data.renderingResourceIdentifier;
    return fontCustomPlatformData.releaseNonNull();
}

FontCustomPlatformSerializedData FontCustomPlatformData::serializedData() const
{
    return FontCustomPlatformSerializedData { { creationData.fontFaceData->dataAsSpanForContiguousData() }, creationData.itemInCollection, m_renderingResourceIdentifier };
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype"_s)
        || equalLettersIgnoringASCIICase(format, "opentype"_s)
        || equalLettersIgnoringASCIICase(format, "woff2"_s)
        || equalLettersIgnoringASCIICase(format, "woff2-variations"_s)
        || equalLettersIgnoringASCIICase(format, "woff-variations"_s)
        || equalLettersIgnoringASCIICase(format, "truetype-variations"_s)
        || equalLettersIgnoringASCIICase(format, "opentype-variations"_s)
        || equalLettersIgnoringASCIICase(format, "woff"_s)
        || equalLettersIgnoringASCIICase(format, "svg"_s);
}

bool FontCustomPlatformData::supportsTechnology(const FontTechnology& tech)
{
    switch (tech) {
    case FontTechnology::ColorColrv0:
    case FontTechnology::ColorSbix:
    case FontTechnology::ColorSvg:
    case FontTechnology::FeaturesAat:
    case FontTechnology::FeaturesOpentype:
    case FontTechnology::Palettes:
    case FontTechnology::Variations:
        return true;
    default:
        return false;
    }
}

}
