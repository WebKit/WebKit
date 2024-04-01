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
#include "GStreamerQuirkAmLogic.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/OptionSet.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_amlogic_quirks_debug);
#define GST_CAT_DEFAULT webkit_amlogic_quirks_debug

GStreamerQuirkAmLogic::GStreamerQuirkAmLogic()
{
    GST_DEBUG_CATEGORY_INIT(webkit_amlogic_quirks_debug, "webkitquirksamlogic", 0, "WebKit AmLogic Quirks");
}

GstElement* GStreamerQuirkAmLogic::createWebAudioSink()
{
    // autoaudiosink changes child element state to READY internally in auto detection phase
    // that causes resource acquisition in some cases interrupting any playback already running.
    // On Amlogic we need to set direct-mode=false prop before changing state to READY
    // but this is not possible with autoaudiosink.
    auto sink = makeGStreamerElement("amlhalasink", nullptr);
    RELEASE_ASSERT_WITH_MESSAGE(sink, "amlhalasink should be available in the system but it is not");
    g_object_set(sink, "direct-mode", FALSE, nullptr);
    return sink;
}

void GStreamerQuirkAmLogic::configureElement(GstElement* element, const OptionSet<ElementRuntimeCharacteristics>& characteristics)
{
    if (gstObjectHasProperty(element, "disable-xrun")) {
        GST_INFO("Set property disable-xrun to TRUE");
        g_object_set(element, "disable-xrun", TRUE, nullptr);
    }

    if (characteristics.contains(ElementRuntimeCharacteristics::HasVideo) && gstObjectHasProperty(element, "wait-video")) {
        GST_INFO("Set property wait-video to TRUE");
        g_object_set(element, "wait-video", TRUE, nullptr);
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
