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
#include "NotImplemented.h"
#include <gst/app/gstappsink.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_media_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_media_debug

namespace WebCore {

RealtimeIncomingSourceGStreamer::RealtimeIncomingSourceGStreamer(const CaptureDevice& device)
    : RealtimeMediaSource(device)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_media_debug, "webkitwebrtcincoming", 0, "WebKit WebRTC incoming media");
    });
    m_bin = gst_bin_new(nullptr);
}

void RealtimeIncomingSourceGStreamer::setUpstreamBin(const GRefPtr<GstElement>& bin)
{
    m_upstreamBin = bin;
    m_tee = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_upstreamBin.get()), "tee"));
}

void RealtimeIncomingSourceGStreamer::startProducingData()
{
    GST_DEBUG_OBJECT(bin(), "Starting data flow");
    m_isStarted = true;
}

void RealtimeIncomingSourceGStreamer::stopProducingData()
{
    GST_DEBUG_OBJECT(bin(), "Stopping data flow");
    m_isStarted = false;
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingSourceGStreamer::capabilities()
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

bool RealtimeIncomingSourceGStreamer::hasClient(const GRefPtr<GstElement>& appsrc)
{
    Locker lock { m_clientLock };
    for (auto& client : m_clients.values()) {
        if (client == appsrc)
            return true;
    }
    return false;
}

void RealtimeIncomingSourceGStreamer::configureAppSink(GstElement* sink)
{
    static GstAppSinkCallbacks callbacks = {
        nullptr, // eos
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
            auto strongSource = source->get();
            if (!strongSource)
                return GST_FLOW_OK;

            auto sample = adoptGRef(gst_app_sink_pull_preroll(sink));
            // dispatchSample might trigger RealtimeMediaSource::notifySettingsDidChangeObservers()
            // which expects to run in the main thread.
            callOnMainThread([source = ThreadSafeWeakPtr { *strongSource.get() }, sample = WTFMove(sample)]() mutable {
                auto strongSource = source.get();
                if (!strongSource)
                    return;

                strongSource->dispatchSample(WTFMove(sample));
            });
            return GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
            auto strongSource = source->get();
            if (!strongSource)
                return GST_FLOW_OK;

            auto sample = adoptGRef(gst_app_sink_pull_sample(sink));
            // dispatchSample might trigger RealtimeMediaSource::notifySettingsDidChangeObservers()
            // which expects to run in the main thread.
            callOnMainThread([source = ThreadSafeWeakPtr { *strongSource.get() }, sample = WTFMove(sample)]() mutable {
                auto strongSource = source.get();
                if (!strongSource)
                    return;

                strongSource->dispatchSample(WTFMove(sample));
            });
            return GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> gboolean {
            auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
            auto strongSource = source->get();
            if (!strongSource)
                return false;

            auto event = adoptGRef(GST_EVENT_CAST(gst_app_sink_pull_object(sink)));
            strongSource->handleDownstreamEvent(GST_ELEMENT_CAST(sink), WTFMove(event));
            return false;
        },
#if GST_CHECK_VERSION(1, 24, 0)
        [](GstAppSink*, GstQuery* query, gpointer userData) -> gboolean {
            auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
            auto strongSource = source->get();
            if (!strongSource)
                return false;

            if (strongSource->isIncomingVideoSource() && strongSource->m_isUpstreamDecoding) {
                gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
                return true;
            }
            return false;
        },
#endif
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, new ThreadSafeWeakPtr { *this }, [](void* data) {
        delete static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(data);
    });
}

void RealtimeIncomingSourceGStreamer::configureFakeVideoSink(GstElement* sink)
{
    g_object_set(sink, "signal-handoffs", TRUE, nullptr);

    auto handoffCallback = G_CALLBACK(+[](GstElement*, GstBuffer* buffer, GstPad* pad, gpointer userData) {
        auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
        auto strongSource = source->get();
        if (!strongSource)
            return;

        auto caps = adoptGRef(gst_pad_get_current_caps(pad));
        auto sample = adoptGRef(gst_sample_new(buffer, caps.get(), nullptr, nullptr));
        // dispatchSample might trigger RealtimeMediaSource::notifySettingsDidChangeObservers()
        // which expects to run in the main thread.
        callOnMainThread([source = ThreadSafeWeakPtr { *strongSource.get() }, sample = WTFMove(sample)]() mutable {
            auto strongSource = source.get();
            if (!strongSource)
                return;

            strongSource->dispatchSample(WTFMove(sample));
        });
    });
    g_signal_connect_data(sink, "preroll-handoff", handoffCallback, new ThreadSafeWeakPtr { *this },
        [](void* data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(data);
        }, static_cast<GConnectFlags>(0));
    g_signal_connect_data(sink, "handoff", handoffCallback, new ThreadSafeWeakPtr { *this },
        [](void* data, GClosure*) {
            delete static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(data);
        }, static_cast<GConnectFlags>(0));

    auto pad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto source = reinterpret_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(userData);
        auto strongSource = source->get();
        if (!strongSource)
            return GST_PAD_PROBE_OK;
        GRefPtr event = GST_PAD_PROBE_INFO_EVENT(info);
        auto sink = adoptGRef(gst_pad_get_parent_element(pad));
        strongSource->handleDownstreamEvent(sink.get(), WTFMove(event));
        return GST_PAD_PROBE_OK;
    }), new ThreadSafeWeakPtr { *this }, [](void* data) {
        delete static_cast<ThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>*>(data);
    });
}

void RealtimeIncomingSourceGStreamer::handleDownstreamEvent(GstElement* sink, GRefPtr<GstEvent>&& event)
{
    switch (GST_EVENT_TYPE(event.get())) {
    case GST_EVENT_STREAM_START:
    case GST_EVENT_CAPS:
    case GST_EVENT_SEGMENT:
    case GST_EVENT_STREAM_COLLECTION:
        return;
    case GST_EVENT_LATENCY: {
        GstClockTime minLatency, maxLatency;
        if (gst_base_sink_query_latency(GST_BASE_SINK(sink), nullptr, nullptr, &minLatency, &maxLatency)) {
            if (int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), m_clientQuark))) {
                GST_DEBUG_OBJECT(sink, "Setting client latency to min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(minLatency), GST_TIME_ARGS(maxLatency));
                auto appsrc = m_clients.get(clientId);
                g_object_set(appsrc, "min-latency", minLatency, "max-latency", maxLatency, nullptr);
            }
        }
        return;
    }
    default:
        break;
    }

    if (int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink), m_clientQuark))) {
        GST_DEBUG_OBJECT(sink, "Forwarding event %" GST_PTR_FORMAT " to client", event.get());
        auto appsrc = m_clients.get(clientId);
        auto pad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
        gst_pad_push_event(pad.get(), event.leakRef());
    }
}

std::optional<int> RealtimeIncomingSourceGStreamer::registerClient(GRefPtr<GstElement>&& appsrc)
{
    if (!m_tee)
        return std::nullopt;

    Locker lock { m_clientLock };
    static Atomic<int> counter = 1;
    auto clientId = counter.exchangeAdd(1);

    auto* queue = gst_element_factory_make("queue", makeString("queue-"_s, clientId).ascii().data());

    ASCIILiteral sinkName = "appsink"_s;
#if !GST_CHECK_VERSION(1, 24, 0)
    if (isIncomingVideoSource() && m_isUpstreamDecoding)
        sinkName = "fakevideosink"_s;
#endif
    auto* sink = makeGStreamerElement(sinkName.characters(), makeString("sink-"_s, clientId).ascii().data());
    g_object_set(sink, "enable-last-sample", FALSE, nullptr);

    if (!m_clientQuark)
        m_clientQuark = g_quark_from_static_string("client-id");
    g_object_set_qdata(G_OBJECT(sink), m_clientQuark, GINT_TO_POINTER(clientId));
    GST_DEBUG_OBJECT(m_bin.get(), "Client %" GST_PTR_FORMAT " with id %d associated to new sink %" GST_PTR_FORMAT, appsrc.get(), clientId, sink);
    m_clients.add(clientId, WTFMove(appsrc));

    if (GST_IS_APP_SINK(sink))
        configureAppSink(sink);
    else
        configureFakeVideoSink(sink);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(sink, "sink"));
    gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, RealtimeIncomingSourceGStreamer* self) -> GstPadProbeReturn {
        auto query = GST_QUERY_CAST(info->data);
        if (self->isIncomingVideoSource() && self->m_isUpstreamDecoding && GST_QUERY_TYPE(query) == GST_QUERY_ALLOCATION) {
            // Let fakevideosink handle the allocation query.
            return GST_PAD_PROBE_OK;
        }

        auto sink = adoptGRef(gst_pad_get_parent_element(pad));
        int clientId = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(sink.get()), self->m_clientQuark));
        if (!clientId)
            return GST_PAD_PROBE_OK;

        auto appsrc = self->m_clients.get(clientId);
        auto srcSrcPad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
        if (gst_pad_peer_query(srcSrcPad.get(), query))
            return GST_PAD_PROBE_HANDLED;

        return GST_PAD_PROBE_OK;
    }), this, nullptr);

    auto padName = makeString("src_"_s, clientId);
    auto teeSrcPad = adoptGRef(gst_element_request_pad_simple(m_tee.get(), padName.ascii().data()));

    GUniquePtr<char> name(gst_pad_get_name(teeSrcPad.get()));
    auto ghostSrcPad = gst_ghost_pad_new(name.get(), teeSrcPad.get());
    gst_element_add_pad(m_upstreamBin.get(), ghostSrcPad);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), queue, sink, nullptr);
    gst_element_link(queue, sink);

    auto queueSinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
    auto ghostSinkPadName = makeString("sink-"_s, clientId);
    auto ghostSinkPad = gst_ghost_pad_new(ghostSinkPadName.ascii().data(), queueSinkPad.get());
    gst_element_add_pad(m_bin.get(), ghostSinkPad);

    gst_pad_link(ghostSrcPad, ghostSinkPad);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(sink);
    gst_element_sync_state_with_parent(m_bin.get());

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME(m_bin.get()));
    return clientId;
}

void RealtimeIncomingSourceGStreamer::unregisterClient(int clientId)
{
    Locker lock { m_clientLock };
    unregisterClientLocked(clientId);
}

void RealtimeIncomingSourceGStreamer::unregisterClientLocked(int clientId)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Unregistering client %d", clientId);
    auto name = makeString("sink-"_s, clientId);
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), name.ascii().data()));
    auto queue = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("queue-"_s, clientId).ascii().data()));

    auto ghostSinkPad = adoptGRef(gst_element_get_static_pad(m_bin.get(), name.ascii().data()));
    auto padName = makeString("src_"_s, clientId);
    auto teeSrcPad = adoptGRef(gst_element_get_static_pad(m_tee.get(), padName.ascii().data()));

    gst_element_set_locked_state(m_upstreamBin.get(), TRUE);
    gst_element_set_locked_state(m_bin.get(), TRUE);
    gst_element_set_state(queue.get(), GST_STATE_NULL);
    gst_element_set_state(sink.get(), GST_STATE_NULL);
    gst_pad_unlink(teeSrcPad.get(), ghostSinkPad.get());
    gst_element_unlink(queue.get(), sink.get());

    auto ghostSrcPad = adoptGRef(gst_element_get_static_pad(m_upstreamBin.get(), padName.ascii().data()));
    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(ghostSrcPad.get()), nullptr);
    gst_element_remove_pad(m_upstreamBin.get(), ghostSrcPad.get());
    gst_element_release_request_pad(m_tee.get(), teeSrcPad.get());

    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(ghostSinkPad.get()), nullptr);
    gst_element_remove_pad(m_bin.get(), ghostSinkPad.get());

    gst_bin_remove_many(GST_BIN_CAST(m_bin.get()), queue.get(), sink.get(), nullptr);
    gst_element_set_locked_state(m_bin.get(), FALSE);
    gst_element_set_locked_state(m_upstreamBin.get(), FALSE);
    m_clients.remove(clientId);
}

void RealtimeIncomingSourceGStreamer::handleUpstreamEvent(GRefPtr<GstEvent>&& event, int clientId)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Handling %" GST_PTR_FORMAT, event.get());
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("sink-"_s, clientId).ascii().data()));
    auto pad = adoptGRef(gst_element_get_static_pad(sink.get(), "sink"));
    gst_pad_push_event(pad.get(), event.leakRef());
}

bool RealtimeIncomingSourceGStreamer::handleUpstreamQuery(GstQuery* query, int clientId)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Handling %" GST_PTR_FORMAT, query);
    auto sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), makeString("sink-"_s, clientId).ascii().data()));
    auto pad = adoptGRef(gst_element_get_static_pad(sink.get(), "sink"));
    return gst_pad_peer_query(pad.get(), query);
}

void RealtimeIncomingSourceGStreamer::tearDown()
{
    notImplemented();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
