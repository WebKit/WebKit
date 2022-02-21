/*
 * Copyright (C) 2007-2021 Apple Inc.
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
#include <CoreFoundation/CFBase.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

typedef struct CGFont* CGFontRef;
typedef const struct __CTFontDescriptor* CTFontDescriptorRef;

namespace WebCore {

class FontCreationContext;
class FontDescription;
struct FontSelectionSpecifiedCapabilities;
class FragmentedSharedBuffer;

template <typename T> class FontTaggedSettings;
typedef FontTaggedSettings<int> FontFeatureSettings;

struct FontCustomPlatformData {
    WTF_MAKE_NONCOPYABLE(FontCustomPlatformData); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FontCustomPlatformData(CTFontDescriptorRef fontDescriptor, FontPlatformData::CreationData&& creationData)
        : fontDescriptor(fontDescriptor)
        , creationData(WTFMove(creationData))
    {
    }

    WEBCORE_EXPORT ~FontCustomPlatformData();

    FontPlatformData fontPlatformData(const FontDescription&, bool bold, bool italic, const FontCreationContext&);

    static bool supportsFormat(const String&);

    RetainPtr<CTFontDescriptorRef> fontDescriptor;
    FontPlatformData::CreationData creationData;
};

WEBCORE_EXPORT std::unique_ptr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer&, const String&);

}

#endif
