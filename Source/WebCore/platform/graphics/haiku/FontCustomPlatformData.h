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

#ifndef FontCustomPlatformData_h
#define FontCustomPlatformData_h

#include "FontDescription.h"
#include "TextFlags.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

    class FontPlatformData;
    class SharedBuffer;

    struct FontCustomPlatformData {
        WTF_MAKE_NONCOPYABLE(FontCustomPlatformData); WTF_MAKE_FAST_ALLOCATED;
    public:
        FontCustomPlatformData() { }
        ~FontCustomPlatformData();

        FontPlatformData fontPlatformData(const FontDescription& description, bool& bold, bool& italic, const FontFeatureSettings&, WebCore::FontSelectionSpecifiedCapabilities);
	    static bool supportsFormat(const String&);
    };

    std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer&, const String&);

}

#endif
