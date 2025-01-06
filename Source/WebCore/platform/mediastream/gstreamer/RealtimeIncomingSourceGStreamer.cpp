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
}

bool RealtimeIncomingSourceGStreamer::setBin(const GRefPtr<GstElement>& bin)
{
    ASSERT(!m_bin);
    if (UNLIKELY(m_bin)) {
        GST_ERROR_OBJECT(m_bin.get(), "Calling setBin twice on the same incoming source instance is not allowed");
        return false;
    }

    m_bin = bin;
    m_sink = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_bin.get()), "sink"));
    g_object_set(m_sink.get(), "signal-handoffs", TRUE, nullptr);

    auto handoffCallback = G_CALLBACK(+[](GstElement*, GstBuffer* buffer, GstPad* pad, gpointer userData) {
        auto source = reinterpret_cast<RealtimeIncomingSourceGStreamer*>(userData);
        auto caps = adoptGRef(gst_pad_get_current_caps(pad));
        auto sample = adoptGRef(gst_sample_new(buffer, caps.get(), nullptr, nullptr));
        // dispatchSample might trigger RealtimeMediaSource::notifySettingsDidChangeObservers()
        // which expects to run in the main thread.
        callOnMainThread([source, sample = WTFMove(sample)]() mutable {
            source->dispatchSample(WTFMove(sample));
        });
    });
    g_signal_connect(m_sink.get(), "preroll-handoff", handoffCallback, this);
    g_signal_connect(m_sink.get(), "handoff", handoffCallback, this);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
    gst_pad_add_probe(sinkPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM), reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto self = reinterpret_cast<RealtimeIncomingSourceGStreamer*>(userData);
        if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
            GRefPtr event = GST_PAD_PROBE_INFO_EVENT(info);
            auto sink = adoptGRef(gst_pad_get_parent_element(pad));
            self->handleDownstreamEvent(sink.get(), WTFMove(event));
            return GST_PAD_PROBE_OK;
        }

        auto query = GST_PAD_PROBE_INFO_QUERY(info);
        self->forEachClient([&](auto* appsrc) {
            auto srcSrcPad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
            gst_pad_peer_query(srcSrcPad.get(), query);
        });
        return GST_PAD_PROBE_OK;
    }), this, nullptr);
    return true;
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

int RealtimeIncomingSourceGStreamer::registerClient(GRefPtr<GstElement>&& appsrc)
{
    Locker lock { m_clientLock };
    static Atomic<int> counter = 1;
    auto clientId = counter.exchangeAdd(1);

    m_clients.add(clientId, WTFMove(appsrc));
    return clientId;
}

void RealtimeIncomingSourceGStreamer::unregisterClient(int clientId)
{
    Locker lock { m_clientLock };
    GST_DEBUG_OBJECT(m_bin.get(), "Unregistering client %d", clientId);
    m_clients.remove(clientId);
}

void RealtimeIncomingSourceGStreamer::forEachClient(Function<void(GstElement*)>&& applyFunction)
{
    Locker lock { m_clientLock };
    for (auto& client : m_clients.values())
        applyFunction(client.get());
}

void RealtimeIncomingSourceGStreamer::handleUpstreamEvent(GRefPtr<GstEvent>&& event)
{
    RELEASE_ASSERT(m_bin);
    GST_DEBUG_OBJECT(m_bin.get(), "Handling %" GST_PTR_FORMAT, event.get());
    auto pad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
    gst_pad_push_event(pad.get(), event.leakRef());
}

bool RealtimeIncomingSourceGStreamer::handleUpstreamQuery(GstQuery* query)
{
    RELEASE_ASSERT(m_bin);
    GST_DEBUG_OBJECT(m_bin.get(), "Handling %" GST_PTR_FORMAT, query);
    auto pad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
    return gst_pad_peer_query(pad.get(), query);
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
            forEachClient([&](auto* appsrc) {
                GST_DEBUG_OBJECT(sink, "Setting client latency to min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(minLatency), GST_TIME_ARGS(maxLatency));
                g_object_set(appsrc, "min-latency", minLatency, "max-latency", maxLatency, nullptr);
            });
        }
        return;
    }
    default:
        break;
    }

    forEachClient([&](auto* appsrc) {
        auto pad = adoptGRef(gst_element_get_static_pad(appsrc, "src"));
        GRefPtr eventCopy(event);
        GST_DEBUG_OBJECT(sink, "Forwarding event %" GST_PTR_FORMAT " to client", eventCopy.get());
        gst_pad_push_event(pad.get(), eventCopy.leakRef());
    });
}

void RealtimeIncomingSourceGStreamer::tearDown()
{
    notImplemented();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
