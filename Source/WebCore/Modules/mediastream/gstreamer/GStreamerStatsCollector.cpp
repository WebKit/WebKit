/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
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
#include "GStreamerStatsCollector.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GUniquePtrGStreamer.h"
#include "JSDOMMapLike.h"
#include "JSRTCStatsReport.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/MainThread.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

static inline void fillRTCStats(RTCStatsReport::Stats& stats, const GstStructure* structure)
{
    double timestamp;
    if (gst_structure_get_double(structure, "timestamp", &timestamp))
        stats.timestamp = timestamp;
    stats.id = gst_structure_get_string(structure, "id");
}

static inline void fillRTCRTPStreamStats(RTCStatsReport::RtpStreamStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    stats.transportId = gst_structure_get_string(structure, "transport-id");
    stats.codecId = gst_structure_get_string(structure, "codec-id");

    unsigned value;
    if (gst_structure_get_uint(structure, "ssrc", &value))
        stats.ssrc = value;

    // FIXME:
    // stats.transportId
    // stats.codecId
    // stats.mediaType
    // stats.kind
}

static inline void fillRTCCodecStats(RTCStatsReport::CodecStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    unsigned value;
    if (gst_structure_get_uint(structure, "payload-type", &value))
        stats.payloadType = value;
    if (gst_structure_get_uint(structure, "clock-rate", &value))
        stats.clockRate = value;

    // FIXME:
    // stats.mimeType =
    // stats.channels =
    // stats.sdpFmtpLine =
    // stats.implementation =
}

static inline void fillInboundRTPStreamStats(RTCStatsReport::InboundRtpStreamStats& stats, const GstStructure* structure)
{
    fillRTCRTPStreamStats(stats, structure);

    uint64_t value;
    if (gst_structure_get_uint64(structure, "packets-received", &value))
        stats.packetsReceived = value;
    if (gst_structure_get_uint64(structure, "bytes-received", &value))
        stats.bytesReceived = value;

    unsigned packetsLost;
    if (gst_structure_get_uint(structure, "packets-lost", &packetsLost))
        stats.packetsLost = packetsLost;

    double jitter;
    if (gst_structure_get_double(structure, "jitter", &jitter))
        stats.jitter = jitter;

    // FIXME:
    // stats.fractionLost =
    // stats.packetsDiscarded =
    // stats.packetsRepaired =
    // stats.burstPacketsLost =
    // stats.burstPacketsDiscarded =
    // stats.burstLossCount =
    // stats.burstDiscardCount =
    // stats.burstLossRate =
    // stats.burstDiscardRate =
    // stats.gapLossRate =
    // stats.gapDiscardRate =
    // stats.framesDecoded =
}

static inline void fillOutboundRTPStreamStats(RTCStatsReport::OutboundRtpStreamStats& stats, const GstStructure* structure)
{
    fillRTCRTPStreamStats(stats, structure);

    uint64_t value;
    if (gst_structure_get_uint64(structure, "packets-sent", &value))
        stats.packetsSent = value;
    if (gst_structure_get_uint64(structure, "bytes-sent", &value))
        stats.bytesSent = value;

    // FIXME
    // stats.targetBitrate =
    // stats.framesEncoded =
}

static inline void fillRTCPeerConnectionStats(RTCStatsReport::PeerConnectionStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    int value;
    if (gst_structure_get_int(structure, "data-channels-opened", &value))
        stats.dataChannelsOpened = value;
    if (gst_structure_get_int(structure, "data-channels-closed", &value))
        stats.dataChannelsClosed = value;
}

static inline void fillRTCTransportStats(RTCStatsReport::TransportStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    // FIXME
    // stats.bytesSent =
    // stats.bytesReceived =
    // stats.rtcpTransportStatsId =
    // stats.selectedCandidatePairId =
    // stats.localCertificateId =
    // stats.remoteCertificateId =
    // stats.dtlsState =
    // stats.tlsVersion =
    // stats.dtlsCipher =
    // stats.srtpCipher =
}

static gboolean fillReportCallback(GQuark, const GValue* value, gpointer userData)
{
    if (!GST_VALUE_HOLDS_STRUCTURE(value))
        return TRUE;

    const GstStructure* structure = gst_value_get_structure(value);
    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return TRUE;

    DOMMapAdapter& report = *reinterpret_cast<DOMMapAdapter*>(userData);

    switch (statsType) {
    case GST_WEBRTC_STATS_CODEC: {
        RTCStatsReport::CodecStats stats;
        fillRTCCodecStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_INBOUND_RTP: {
        RTCStatsReport::InboundRtpStreamStats stats;
        fillInboundRTPStreamStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_OUTBOUND_RTP: {
        RTCStatsReport::OutboundRtpStreamStats stats;
        fillOutboundRTPStreamStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_INBOUND_RTP: {
        RTCStatsReport::InboundRtpStreamStats stats;
        fillInboundRTPStreamStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP: {
        RTCStatsReport::OutboundRtpStreamStats stats;
        fillOutboundRTPStreamStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_CSRC:
        GST_FIXME("Deprecated stats: csrc");
        break;
    case GST_WEBRTC_STATS_PEER_CONNECTION: {
        RTCStatsReport::PeerConnectionStats stats;
        fillRTCPeerConnectionStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::PeerConnectionStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_TRANSPORT: {
        RTCStatsReport::TransportStats stats;
        fillRTCTransportStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::TransportStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_STREAM:
        GST_FIXME("Deprecated stats: stream");
        break;
    case GST_WEBRTC_STATS_DATA_CHANNEL:
        GST_FIXME("Missing data-channel stats support");
        break;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
    case GST_WEBRTC_STATS_LOCAL_CANDIDATE:
    case GST_WEBRTC_STATS_REMOTE_CANDIDATE:
        GST_FIXME("Missing candidate stats support");
        break;
    case GST_WEBRTC_STATS_CERTIFICATE:
        GST_FIXME("Missing certificate stats support");
        break;
    }

    return TRUE;
}

struct CallbackHolder {
    GStreamerStatsCollector::CollectorCallback callback;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(CallbackHolder)

void GStreamerStatsCollector::getStats(CollectorCallback&& callback, GstPad* pad)
{
    if (!m_webrtcBin) {
        callback(nullptr);
        return;
    }

    auto* holder = createCallbackHolder();
    holder->callback = WTFMove(callback);
    g_signal_emit_by_name(m_webrtcBin.get(), "get-stats", pad, gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) {
        auto promise = adoptGRef(rawPromise);
        auto* holder = static_cast<CallbackHolder*>(userData);
        if (gst_promise_wait(promise.get()) != GST_PROMISE_RESULT_REPLIED) {
            holder->callback(nullptr);
            return;
        }

        const auto* stats = gst_promise_get_reply(promise.get());
        if (!stats) {
            holder->callback(nullptr);
            return;
        }

        if (gst_structure_has_field(stats, "error")) {
            GUniqueOutPtr<GError> error;
            gst_structure_get(stats, "error", G_TYPE_ERROR, &error.outPtr(), nullptr);
            GST_WARNING("Unable to get stats, error: %s", error->message);
            holder->callback(nullptr);
            return;
        }

        callOnMainThreadAndWait([promise = WTFMove(promise), holder] {
            // Hold an additional ref to the promise because it is asynchronously used from the JS bindings.
            holder->callback(RTCStatsReport::create([promise = GRefPtr<GstPromise>(promise.get())](auto& mapAdapter) {
                const auto* stats = gst_promise_get_reply(promise.get());
                gst_structure_foreach(stats, fillReportCallback, &mapAdapter);
            }));
        });
    }, holder, reinterpret_cast<GDestroyNotify>(destroyCallbackHolder)));
}

}; // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
