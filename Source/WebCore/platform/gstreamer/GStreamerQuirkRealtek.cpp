/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
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
#include "GStreamerQuirkRealtek.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/OptionSet.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_realtek_quirks_debug);
#define GST_CAT_DEFAULT webkit_realtek_quirks_debug

GStreamerQuirkRealtek::GStreamerQuirkRealtek()
{
    GST_DEBUG_CATEGORY_INIT(webkit_realtek_quirks_debug, "webkitquirksrealtek", 0, "WebKit Realtek Quirks");
    m_disallowedWebAudioDecoders = {
        "omxaacdec"_s,
        "omxac3dec"_s,
        "omxac4dec"_s,
        "omxeac3dec"_s,
        "omxflacdec"_s,
        "omxlpcmdec"_s,
        "omxmp3dec"_s,
        "omxopusdec"_s,
        "omxvorbisdec"_s,
    };
}

GstElement* GStreamerQuirkRealtek::createWebAudioSink()
{
    auto sink = makeGStreamerElement("rtkaudiosink", nullptr);
    RELEASE_ASSERT_WITH_MESSAGE(sink, "rtkaudiosink should be available in the system but it is not");
    g_object_set(sink, "media-tunnel", FALSE, "audio-service", TRUE, nullptr);
    return sink;
}

void GStreamerQuirkRealtek::configureElement(GstElement* element, const OptionSet<ElementRuntimeCharacteristics>& characteristics)
{
    if (!characteristics.contains(ElementRuntimeCharacteristics::IsMediaStream))
        return;

    if (gstObjectHasProperty(element, "media-tunnel")) {
        GST_INFO("Enable 'immediate-output' in rtkaudiosink");
        g_object_set(element, "media-tunnel", FALSE, "audio-service", TRUE, "lowdelay-sync-mode", TRUE, nullptr);
    }

    if (gstObjectHasProperty(element, "lowdelay-mode")) {
        GST_INFO("Enable 'lowdelay-mode' in rtk omx decoder");
        g_object_set(element, "lowdelay-mode", TRUE, nullptr);
    }
}

std::optional<bool> GStreamerQuirkRealtek::isHardwareAccelerated(GstElementFactory* factory)
{
    if (g_str_has_prefix(GST_OBJECT_NAME(factory), "omx"))
        return true;

    return std::nullopt;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
