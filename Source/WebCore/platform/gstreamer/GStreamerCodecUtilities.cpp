/*
 * Copyright (C) 2022 Igalia S.L
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerCodecUtilities.h"

#if USE(GSTREAMER)

#include <gst/pbutils/codec-utils.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY(webkit_gst_codec_utilities_debug);
#define GST_CAT_DEFAULT webkit_gst_codec_utilities_debug

namespace WebCore {

static void ensureDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_gst_codec_utilities_debug, "webkitcodec", 0, "WebKit Codecs Utilities");
    });
}

std::pair<const char*, const char*> GStreamerCodecUtilities::parseH264ProfileAndLevel(const String& codec)
{
    ensureDebugCategoryInitialized();

    auto components = codec.split('.');
    long int spsAsInteger = strtol(components[1].utf8().data(), nullptr, 16);
    uint8_t sps[3];
    sps[0] = spsAsInteger >> 16;
    sps[1] = spsAsInteger >> 8;
    sps[2] = spsAsInteger;

    const char* profile = gst_codec_utils_h264_get_profile(sps, 3);
    const char* level = gst_codec_utils_h264_get_level(sps, 3);

    // To avoid going through a class hierarchy for such a simple
    // string conversion, we use a little trick here: See
    // https://bugs.webkit.org/show_bug.cgi?id=201870.
    char levelAsStringFallback[2] = { '\0', '\0' };
    if (!level && sps[2] > 0 && sps[2] <= 5) {
        levelAsStringFallback[0] = static_cast<char>('0' + sps[2]);
        level = levelAsStringFallback;
    }

    GST_DEBUG("Codec %s translates to H.264 profile %s and level %s", codec.utf8().data(), GST_STR_NULL(profile), GST_STR_NULL(level));
    return { profile, level };
}

uint8_t GStreamerCodecUtilities::parseVP9Profile(const String& codec)
{
    ensureDebugCategoryInitialized();

    auto components = codec.split('.');
    auto profile = parseInteger<uint8_t>(components[1]).value_or(0);
    GST_DEBUG("Codec %s translates to VP9 profile %u", codec.utf8().data(), profile);
    return profile;
}

} // namespace WebCore

#endif // USE(GSTREAMER)
