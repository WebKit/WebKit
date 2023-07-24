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

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_media_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_media_debug

namespace WebCore {

RealtimeIncomingSourceGStreamer::RealtimeIncomingSourceGStreamer(const CaptureDevice& device)
    : RealtimeMediaSource(device)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_media_debug, "webkitwebrtcincomingmedia", 0, "WebKit WebRTC incoming media");
    });
    m_bin = gst_bin_new(nullptr);
    m_valve = gst_element_factory_make("valve", nullptr);
    m_tee = gst_element_factory_make("tee", nullptr);
    g_object_set(m_tee.get(), "allow-not-linked", TRUE, nullptr);

    auto* parsebin = makeGStreamerElement("parsebin", nullptr);

    g_signal_connect(parsebin, "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        if (gstObjectHasProperty(element, "request-keyframe"))
            g_object_set(element, "request-keyframe", TRUE, nullptr);
        if (gstObjectHasProperty(element, "wait-for-keyframe"))
            g_object_set(element, "wait-for-keyframe", TRUE, nullptr);
    }), nullptr);

    g_signal_connect_swapped(parsebin, "pad-added", G_CALLBACK(+[](RealtimeIncomingSourceGStreamer* source, GstPad* pad) {
        auto sinkPad = adoptGRef(gst_element_get_static_pad(source->m_tee.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());

        gst_bin_sync_children_states(GST_BIN_CAST(source->m_bin.get()));
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(source->m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME(source->m_bin.get()));
    }), this);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_valve.get(), parsebin, m_tee.get(), nullptr);
    gst_element_link(m_valve.get(), parsebin);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(m_valve.get(), "sink"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));
}

void RealtimeIncomingSourceGStreamer::startProducingData()
{
    GST_DEBUG_OBJECT(bin(), "Starting data flow");
    if (m_valve)
        g_object_set(m_valve.get(), "drop", FALSE, nullptr);
}

void RealtimeIncomingSourceGStreamer::stopProducingData()
{
    GST_DEBUG_OBJECT(bin(), "Stopping data flow");
    if (m_valve)
        g_object_set(m_valve.get(), "drop", TRUE, nullptr);
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingSourceGStreamer::capabilities()
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

int RealtimeIncomingSourceGStreamer::registerClient(GRefPtr<GstElement>&& appsrc)
{
    static Atomic<int> counter = 1;
    auto clientId = counter.exchangeAdd(1);

    auto* queue = gst_element_factory_make("queue", makeString("queue-"_s, clientId).ascii().data());
    auto* sink = makeGStreamerElement("appsink", makeString("sink-"_s, clientId).ascii().data());
    g_object_set(sink, "enable-last-sample", FALSE, nullptr);

    if (!m_clientQuark)
        m_clientQuark = g_quark_from_static_string("client-id");
    g_object_set_qdata(G_OBJECT(sink), m_clientQuark, GINT_TO_POINTER(clientId));
    GST_DEBUG_OBJECT(m_bin.get(), "Client %" GST_PTR_FORMAT " associated to new sink %" GST_PTR_FORMAT, appsrc.get(), sink);
    m_clients.add(clientId, WTFMove(appsrc));

    static GstAppSinkCallbacks callbacks = {
        nullptr, // eos
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto* self = reinterpret_cast<RealtimeIncomingSourceGStreamer*>(userData);
            auto sample = adoptGRef(gst_app_sink_pull_preroll(sink));
            self->dispatchSample(WTFMove(sample));
            return GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto* self = reinterpret_cast<RealtimeIncomingSourceGStreamer*>(userData);
            auto sample = adoptGRef(gst_app_sink_pull_sample(sink));
            self->dispatchSample(WTFMove(sample));
            return GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> gboolean {
            auto* self = reinterpret_cast<RealtimeIncomingSourceGStreamer*>(userData);
            auto event = adoptGRef(GST_EVENT_CAST(gst_app_sink_pull_object(sink)));
            switch (GST_EVENT_TYPE(event.get())) {
            case GST_EVENT_STREAM_START:
            case GST_EVENT_CAPS:
            case GST_EVENT_SEGMENT:
            case GST_EVENT_STREAM_COLLECTION:
                return false;
            case GST_EVENT_LATENCY: {
                GstClockTime minLatency, maxLatency;
                if (gst_base_sink_query_latency(GST_BASE_SINK(sink), nullptr, nullptr, &minLatency, &maxLatency)) {
                    if (int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), self->m_clientQuark))) {
                        GST_DEBUG_OBJECT(sink, "Setting client latency to min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(minLatency), GST_TIME_ARGS(maxLatency));
                        auto appsrc = self->m_clients.get(clientId);
                        g_object_set(appsrc, "min-latency", minLatency, "max-latency", maxLatency, nullptr);
                    }
                }
                return false;
            }
            default:
                break;
            }

            if (int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), self->m_clientQuark))) {
                GST_DEBUG_OBJECT(sink, "Forwarding event %" GST_PTR_FORMAT " to client", event.get());
                auto appsrc = self->m_clients.get(clientId);
                auto pad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
                gst_pad_push_event(pad.get(), event.leakRef());
            }

            return false;
        },
#if GST_CHECK_VERSION(1, 23, 0)
        // propose_allocation
        nullptr,
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, RealtimeIncomingSourceGStreamer* self) -> GstPadProbeReturn {
        auto sink = adoptGRef(gst_pad_get_parent_element(pad));
        int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink.get()), self->m_clientQuark));
        if (!clientId)
            return GST_PAD_PROBE_OK;

        auto appsrc = self->m_clients.get(clientId);
        auto srcSrcPad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
        if (gst_pad_peer_query(srcSrcPad.get(), GST_QUERY_CAST(info->data)))
            return GST_PAD_PROBE_HANDLED;

        return GST_PAD_PROBE_OK;
    }), this, nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), queue, sink, nullptr);
    gst_element_link_many(m_tee.get(), queue, sink, nullptr);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME(m_bin.get()));
    return clientId;
}

void RealtimeIncomingSourceGStreamer::unregisterClient(int clientId)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Unregistering client %d", clientId);
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("sink-", clientId).ascii().data()));
    auto queue = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("queue-", clientId).ascii().data()));
    auto queueSinkPad = adoptGRef(gst_element_get_static_pad(queue.get(), "sink"));
    auto teeSrcPad = adoptGRef(gst_pad_get_peer(queueSinkPad.get()));

    gst_element_set_locked_state(m_bin.get(), TRUE);
    gst_element_set_state(queue.get(), GST_STATE_NULL);
    gst_element_set_state(sink.get(), GST_STATE_NULL);
    gst_element_unlink_many(m_tee.get(), queue.get(), sink.get(), nullptr);
    gst_bin_remove_many(GST_BIN_CAST(m_bin.get()), queue.get(), sink.get(), nullptr);
    gst_element_release_request_pad(m_tee.get(), teeSrcPad.get());
    gst_element_set_locked_state(m_bin.get(), FALSE);
    m_clients.remove(clientId);
}

void RealtimeIncomingSourceGStreamer::handleUpstreamEvent(GRefPtr<GstEvent>&& event, int clientId)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Handling %" GST_PTR_FORMAT, event.get());
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("sink-", clientId).ascii().data()));
    auto pad = adoptGRef(gst_element_get_static_pad(sink.get(), "sink"));
    gst_pad_push_event(pad.get(), event.leakRef());
}

bool RealtimeIncomingSourceGStreamer::handleUpstreamQuery(GstQuery* query, int clientId)
{
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("sink-", clientId).ascii().data()));
    auto pad = adoptGRef(gst_element_get_static_pad(sink.get(), "sink"));
    return gst_pad_peer_query(pad.get(), query);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
