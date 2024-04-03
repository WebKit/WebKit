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
#include "GStreamerQuirkBcmNexus.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/OptionSet.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_bcmnexus_quirks_debug);
#define GST_CAT_DEFAULT webkit_bcmnexus_quirks_debug

GStreamerQuirkBcmNexus::GStreamerQuirkBcmNexus()
{
    GST_DEBUG_CATEGORY_INIT(webkit_bcmnexus_quirks_debug, "webkitquirksbcmnexus", 0, "WebKit BcmNexus Quirks");
    m_disallowedWebAudioDecoders = { "brcmaudfilter"_s };

    auto registry = gst_registry_get();
    auto brcmaudfilter = adoptGRef(gst_registry_lookup_feature(registry, "brcmaudfilter"));
    auto mpegaudioparse = adoptGRef(gst_registry_lookup_feature(registry, "mpegaudioparse"));

    if (brcmaudfilter && mpegaudioparse) {
        GST_INFO("Overriding mpegaudioparse rank with brcmaudfilter rank + 1");
        gst_plugin_feature_set_rank(mpegaudioparse.get(), gst_plugin_feature_get_rank(brcmaudfilter.get()) + 1);
    }
}

std::optional<bool> GStreamerQuirkBcmNexus::isHardwareAccelerated(GstElementFactory* factory)
{
    if (g_str_has_prefix(GST_OBJECT_NAME(factory), "brcm"))
        return true;

    return std::nullopt;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
