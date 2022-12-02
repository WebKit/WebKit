/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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

#include "Font.h"
#include "FontCache.h"
#include "FontCacheCoreText.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "StyleFontSizeFunctions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    addAttributesForWebFonts(attributes.get(), fontDescription.shouldAllowUserInstalledFonts());
    auto modifiedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), attributes.get()));
    ASSERT(modifiedFontDescriptor);

    int size = fontDescription.computedPixelSize();
    FontOrientation orientation = fontDescription.orientation();
    FontWidthVariant widthVariant = fontDescription.widthVariant();

    auto font = adoptCF(CTFontCreateWithFontDescriptor(modifiedFontDescriptor.get(), size, nullptr));
    font = preparePlatformFont(font.get(), fontDescription, fontCreationContext);
    ASSERT(font);
    FontPlatformData platformData(font.get(), size, bold, italic, orientation, widthVariant, fontDescription.textRenderingMode(), &creationData);

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust());
    return platformData;
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& itemInCollection)
{
    RetainPtr<CFDataRef> bufferData = buffer.createCFData();

    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    auto array = adoptCF(CTFontManagerCreateFontDescriptorsFromData(bufferData.get()));
    if (!array)
        return nullptr;
    auto length = CFArrayGetCount(array.get());
    if (length <= 0)
        return nullptr;
    if (!itemInCollection.isNull()) {
        if (auto desiredName = itemInCollection.createCFString()) {
            for (CFIndex i = 0; i < length; ++i) {
                auto candidate = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(array.get(), i));
                auto postScriptName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(candidate, kCTFontNameAttribute)));
                if (CFStringCompare(postScriptName.get(), desiredName.get(), 0) == kCFCompareEqualTo) {
                    fontDescriptor = candidate;
                    break;
                }
            }
        }
    }
    if (!fontDescriptor)
        fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(array.get(), 0));

    FontPlatformData::CreationData creationData = { buffer, itemInCollection };
    return makeUnique<FontCustomPlatformData>(fontDescriptor.get(), WTFMove(creationData));
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
        || equalLettersIgnoringASCIICase(format, "woff"_s);
}

}
