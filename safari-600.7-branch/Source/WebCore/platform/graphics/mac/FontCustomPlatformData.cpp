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

#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include <CoreGraphics/CoreGraphics.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant, FontRenderingMode)
{
    return FontPlatformData(m_cgFont.get(), size, bold, italic, orientation, widthVariant);
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer)
{
    ATSFontContainerRef containerRef = 0;

    RetainPtr<CFDataRef> bufferData = buffer.createCFData();
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(bufferData.get()));

    RetainPtr<CGFontRef> cgFontRef = adoptCF(CGFontCreateWithDataProvider(dataProvider.get()));
    if (!cgFontRef)
        return nullptr;

    return std::make_unique<FontCustomPlatformData>(containerRef, cgFontRef.get());
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || equalIgnoringCase(format, "woff");
}

}
