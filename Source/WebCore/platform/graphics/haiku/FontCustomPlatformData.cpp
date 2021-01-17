/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
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

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
}

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, bool& bold, bool& italic, const FontFeatureSettings&, WebCore::FontSelectionSpecifiedCapabilities)
{
    return FontPlatformData(description.computedSize(), bold, italic);
}

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String&)
{
    // FIXME: We need support in Haiku to read fonts from memory to implement this.
    return 0;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringASCIICase(format, "truetype");
}

}
