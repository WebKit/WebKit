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
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& fontDescription, bool bold, bool italic, const FontFeatureSettings& fontFaceFeatures, const FontVariantSettings& fontFaceVariantSettings)
{
    int size = fontDescription.computedPixelSize();
    FontOrientation orientation = fontDescription.orientation();
    FontWidthVariant widthVariant = fontDescription.widthVariant();
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateWithFontDescriptor(m_fontDescriptor.get(), size, nullptr));
    font = preparePlatformFont(font.get(), fontDescription.textRenderingMode(), &fontFaceFeatures, &fontFaceVariantSettings, fontDescription.featureSettings(), fontDescription.variantSettings());
    return FontPlatformData(font.get(), size, bold, italic, orientation, widthVariant, fontDescription.textRenderingMode());
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer)
{
    RetainPtr<CFDataRef> bufferData = buffer.createCFData();

    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontManagerCreateFontDescriptorFromData(bufferData.get()));
    if (!fontDescriptor)
        return nullptr;

    return std::make_unique<FontCustomPlatformData>(fontDescriptor.get());
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype")
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)
        || equalIgnoringCase(format, "woff2")
#endif
        || equalIgnoringCase(format, "woff");
}

}
