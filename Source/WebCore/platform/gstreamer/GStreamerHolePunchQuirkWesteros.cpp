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
#include "GStreamerHolePunchQuirkWesteros.h"
#include "MediaPlayerPrivateGStreamer.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

GstElement* GStreamerHolePunchQuirkWesteros::createHolePunchVideoSink(bool isLegacyPlaybin, const MediaPlayer* player)
{
    AtomString val;
    bool isPIPRequested = player && player->doesHaveAttribute("pip"_s, &val) && equalLettersIgnoringASCIICase(val, "true"_s);
    if (isLegacyPlaybin && !isPIPRequested)
        return nullptr;

    // Westeros using holepunch.
    GstElement* videoSink = makeGStreamerElement("westerossink", "WesterosVideoSink");
    g_object_set(videoSink, "zorder", 0.0f, nullptr);
    if (isPIPRequested) {
        g_object_set(videoSink, "res-usage", 0u, nullptr);
        // Set context for pipelines that use ERM in decoder elements.
        auto context = adoptGRef(gst_context_new("erm", FALSE));
        auto contextStructure = gst_context_writable_structure(context.get());
        gst_structure_set(contextStructure, "res-usage", G_TYPE_UINT, 0x0u, nullptr);
        auto playerPrivate = reinterpret_cast<const MediaPlayerPrivateGStreamer*>(player->playerPrivate());
        gst_element_set_context(playerPrivate->pipeline(), context.get());
    }
    return videoSink;
}

bool GStreamerHolePunchQuirkWesteros::setHolePunchVideoRectangle(GstElement* videoSink, const IntRect& rect)
{
    if (UNLIKELY(!gstObjectHasProperty(videoSink, "rectangle")))
        return false;

    auto rectString = makeString(rect.x(), ',', rect.y(), ',', rect.width(), ',', rect.height());
    g_object_set(videoSink, "rectangle", rectString.ascii().data(), nullptr);
    return true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
