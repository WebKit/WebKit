/*
 * Copyright (C) 2007-2008, 2016 Apple Inc. All rights reserved.
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

#include "FontPlatformData.h"
#include "TextFlags.h"
#include <windows.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if USE(CORE_TEXT)
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

typedef struct CGFont* CGFontRef;

namespace WebCore {

class FontDescription;
class FontMemoryResource;
class SharedBuffer;
struct FontSelectionSpecifiedCapabilities;
struct FontVariantSettings;

template <typename T> class FontTaggedSettings;
typedef FontTaggedSettings<int> FontFeatureSettings;

struct FontCustomPlatformData {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(FontCustomPlatformData);
public:
    FontCustomPlatformData(const String& name, FontPlatformData::CreationData&&);
    ~FontCustomPlatformData();

    FontPlatformData fontPlatformData(const FontDescription&, bool bold, bool italic, const FontFeatureSettings&, FontSelectionSpecifiedCapabilities);

    static bool supportsFormat(const String&);

    String name;
#if USE(CORE_TEXT)
    RetainPtr<CTFontDescriptorRef> fontDescriptor;
#endif
    FontPlatformData::CreationData creationData;
};

std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer&, const String&);

}

#endif
