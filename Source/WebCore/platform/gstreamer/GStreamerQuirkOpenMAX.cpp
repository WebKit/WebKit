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
#include "GStreamerQuirkOpenMAX.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_openmax_quirks_debug);
#define GST_CAT_DEFAULT webkit_openmax_quirks_debug

GStreamerQuirkOpenMAX::GStreamerQuirkOpenMAX()
{
    GST_DEBUG_CATEGORY_INIT(webkit_openmax_quirks_debug, "webkitquirksopenmax", 0, "WebKit OpenMAX Quirks");
}

bool GStreamerQuirkOpenMAX::isPlatformSupported() const
{
    auto registry = gst_registry_get();
    auto feature = adoptGRef(gst_registry_lookup_feature(registry, "omx"));
    return feature;
}

bool GStreamerQuirkOpenMAX::processWebAudioSilentBuffer(GstBuffer* buffer) const
{
    GST_TRACE("Force disabling GAP buffer flag");
    GST_BUFFER_FLAG_UNSET(buffer, GST_BUFFER_FLAG_GAP);
    GST_BUFFER_FLAG_UNSET(buffer, GST_BUFFER_FLAG_DROPPABLE);
    return true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
