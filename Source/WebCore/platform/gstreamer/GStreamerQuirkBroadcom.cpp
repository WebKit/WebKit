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
#include "GStreamerQuirkBroadcom.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/OptionSet.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_broadcom_quirks_debug);
#define GST_CAT_DEFAULT webkit_broadcom_quirks_debug

GStreamerQuirkBroadcom::GStreamerQuirkBroadcom()
{
    GST_DEBUG_CATEGORY_INIT(webkit_broadcom_quirks_debug, "webkitquirksbroadcom", 0, "WebKit Broadcom Quirks");
    m_disallowedWebAudioDecoders = { "brcmaudfilter"_s };
}

bool GStreamerQuirkBroadcom::isPlatformSupported() const
{
    auto broadcomFactory = adoptGRef(gst_element_factory_find("brcmaudiosink"));
    return broadcomFactory;
}

void GStreamerQuirkBroadcom::configureElement(GstElement* element, const OptionSet<ElementRuntimeCharacteristics>& characteristics)
{
    auto objectType = CStringView::unsafeFromUTF8(G_OBJECT_TYPE_NAME(element));
    if (objectType == "Gstbrcmaudiosink"_s)
        g_object_set(G_OBJECT(element), "async", TRUE, nullptr);
    else {
        auto elementName = CStringView::unsafeFromUTF8(GST_ELEMENT_NAME(element));
        if (startsWith(elementName.span(), "brcmaudiodecoder"_s)) {
            // Limit BCM audio decoder buffering to 1sec so live progressive playback can start faster.
            if (characteristics.contains(ElementRuntimeCharacteristics::IsLiveStream))
                g_object_set(G_OBJECT(element), "limit_buffering_ms", 1000, nullptr);
        }
    }

    if (!characteristics.contains(ElementRuntimeCharacteristics::IsMediaStream))
        return;

    if (objectType == "GstBrcmPCMSink"_s && gstObjectHasProperty(element, "low_latency"_s)) {
        GST_DEBUG("Set 'low_latency' in brcmpcmsink");
        g_object_set(element, "low_latency", TRUE, "low_latency_max_queued_ms", 60, nullptr);
    }
}

std::optional<bool> GStreamerQuirkBroadcom::isHardwareAccelerated(GstElementFactory* factory)
{
    auto view = StringView::fromLatin1(GST_OBJECT_NAME(factory));
    if (view.startsWith("brcm"_s))
        return true;

    return std::nullopt;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
