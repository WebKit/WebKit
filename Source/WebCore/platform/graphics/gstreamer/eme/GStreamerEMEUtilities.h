/* GStreamer EME Utilities class
 *
 * Copyright (C) 2017 Metrological
 * Copyright (C) 2017 Igalia S.L
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

#include <gst/gst.h>
#include <wtf/text/WTFString.h>

#define WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"

namespace WebCore {

class GStreamerEMEUtilities {

public:
    static const char* s_ClearKeyUUID;
    static const char* s_ClearKeyKeySystem;

    static bool isClearKeyKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_ClearKeyKeySystem);
    }

    static const char* keySystemToUuid(const String& keySystem)
    {
        if (isClearKeyKeySystem(keySystem))
            return s_ClearKeyUUID;

        ASSERT_NOT_REACHED();
        return { };
    }

    static GstElement* createDecryptor(const char* protectionSystem);
};

}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
