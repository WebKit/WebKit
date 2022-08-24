/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(GSTREAMER_WEBRTC)
#include "RealtimeIncomingSourceGStreamer.h"

#include "GStreamerCommon.h"
#include <gst/app/gstappsink.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

RealtimeIncomingSourceGStreamer::RealtimeIncomingSourceGStreamer()
{
    m_bin = gst_bin_new(nullptr);
    m_valve = gst_element_factory_make("valve", nullptr);
    m_tee = gst_element_factory_make("tee", nullptr);
    g_object_set(m_tee.get(), "allow-not-linked", true, nullptr);

    auto* queue = gst_element_factory_make("queue", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_valve.get(), queue, m_tee.get(), nullptr);

    gst_element_link_many(m_valve.get(), queue, m_tee.get(), nullptr);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(m_valve.get(), "sink"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));
}

void RealtimeIncomingSourceGStreamer::closeValve() const
{
    GST_DEBUG_OBJECT(m_bin.get(), "Closing valve");
    if (m_valve)
        g_object_set(m_valve.get(), "drop", true, nullptr);
}

void RealtimeIncomingSourceGStreamer::openValve() const
{
    GST_DEBUG_OBJECT(m_bin.get(), "Opening valve");
    if (m_valve)
        g_object_set(m_valve.get(), "drop", false, nullptr);
}

void RealtimeIncomingSourceGStreamer::registerClient()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Registering new client");
    auto* queue = gst_element_factory_make("queue", nullptr);
    auto* sink = makeGStreamerElement("appsink", nullptr);
    g_object_set(sink, "enable-last-sample", FALSE, "emit-signals", TRUE, "max-buffers", 1, nullptr);
    g_signal_connect_swapped(sink, "new-sample", G_CALLBACK(+[](RealtimeIncomingSourceGStreamer* self, GstElement* sink) -> GstFlowReturn {
        auto sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        self->dispatchSample(WTFMove(sample));
        return GST_FLOW_OK;
    }), this);

    g_signal_connect_swapped(sink, "new-preroll", G_CALLBACK(+[](RealtimeIncomingSourceGStreamer* self, GstElement* sink) -> GstFlowReturn {
        auto sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(sink)));
        self->dispatchSample(WTFMove(sample));
        return GST_FLOW_OK;
    }), this);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), queue, sink, nullptr);
    gst_element_link_many(m_tee.get(), queue, sink, nullptr);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME(m_bin.get()));
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
