/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
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

#include "FontCache.h"
#include "FontCacheCoreText.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <pal/spi/cocoa/CoreTextSPI.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& fontDescription, bool bold, bool italic, const FontFeatureSettings& fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
{
    auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    addAttributesForWebFonts(attributes.get(), fontDescription.shouldAllowUserInstalledFonts());
    auto modifiedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(m_fontDescriptor.get(), attributes.get()));
    ASSERT(modifiedFontDescriptor);

    int size = fontDescription.computedPixelSize();
    FontOrientation orientation = fontDescription.orientation();
    FontWidthVariant widthVariant = fontDescription.widthVariant();
    auto font = adoptCF(CTFontCreateWithFontDescriptor(modifiedFontDescriptor.get(), size, nullptr));
    font = preparePlatformFont(font.get(), fontDescription, &fontFaceFeatures, fontFaceCapabilities);
    ASSERT(font);
    return FontPlatformData(font.get(), size, bold, italic, orientation, widthVariant, fontDescription.textRenderingMode());
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& itemInCollection)
{
    RetainPtr<CFDataRef> bufferData = buffer.createCFData();

    RetainPtr<CTFontDescriptorRef> fontDescriptor;
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
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
#else
    UNUSED_PARAM(itemInCollection);
    fontDescriptor = adoptCF(CTFontManagerCreateFontDescriptorFromData(bufferData.get()));
    if (!fontDescriptor)
        return nullptr;
#endif

    return makeUnique<FontCustomPlatformData>(fontDescriptor.get());
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype")
        || equalLettersIgnoringASCIICase(format, "opentype")
        || equalLettersIgnoringASCIICase(format, "woff2")
        || equalLettersIgnoringASCIICase(format, "woff2-variations")
        || equalLettersIgnoringASCIICase(format, "woff-variations")
        || equalLettersIgnoringASCIICase(format, "truetype-variations")
        || equalLettersIgnoringASCIICase(format, "opentype-variations")
        || equalLettersIgnoringASCIICase(format, "woff");
}

}
