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

#include "GStreamerCommon.h"
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
    stats.id = String::fromLatin1(gst_structure_get_string(structure, "id"));
}

static inline void fillRTCRTPStreamStats(RTCStatsReport::RtpStreamStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    stats.transportId = String::fromLatin1(gst_structure_get_string(structure, "transport-id"));
    stats.codecId = String::fromLatin1(gst_structure_get_string(structure, "codec-id"));

    unsigned value;
    if (gst_structure_get_uint(structure, "ssrc", &value))
        stats.ssrc = value;

    // FIXME:
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

static inline void fillRemoteInboundRTPStreamStats(RTCStatsReport::RemoteInboundRtpStreamStats& stats, const GstStructure* structure, const GstStructure* additionalStats)
{
    UNUSED_PARAM(additionalStats);
    unsigned value;
    if (gst_structure_get_uint(structure, "ssrc", &value))
        stats.ssrc = value;

    double jitter;
    if (gst_structure_get_double(structure, "jitter", &jitter))
        stats.jitter = jitter;

#if GST_CHECK_VERSION(1, 21, 0)
    int64_t packetsLost;
    if (gst_structure_get_int64(structure, "packets-lost", &packetsLost))
        stats.packetsLost = packetsLost;
#else
    unsigned packetsLost;
    if (gst_structure_get_uint(structure, "packets-lost", &packetsLost))
        stats.packetsLost = packetsLost;
#endif

    double roundTripTime;
    if (gst_structure_get_double(structure, "round-trip-time", &roundTripTime))
        stats.roundTripTime = roundTripTime;

    if (const char* localId = gst_structure_get_string(structure, "local-id"))
        stats.localId = String::fromLatin1(localId);

    // FIXME: Fill remaining fields.
}

static inline void fillInboundRTPStreamStats(RTCStatsReport::InboundRtpStreamStats& stats, const GstStructure* structure, const GstStructure* additionalStats)
{
    fillRTCRTPStreamStats(stats, structure);

    uint64_t value;
    if (gst_structure_get_uint64(structure, "packets-received", &value))
        stats.packetsReceived = value;
    if (gst_structure_get_uint64(structure, "bytes-received", &value))
        stats.bytesReceived = value;

#if GST_CHECK_VERSION(1, 21, 0)
    int64_t packetsLost;
    if (gst_structure_get_int64(structure, "packets-lost", &packetsLost))
        stats.packetsLost = packetsLost;
#else
    unsigned packetsLost;
    if (gst_structure_get_uint(structure, "packets-lost", &packetsLost))
        stats.packetsLost = packetsLost;
#endif

    double jitter;
    if (gst_structure_get_double(structure, "jitter", &jitter))
        stats.jitter = jitter;

    if (gst_structure_get_uint64(structure, "packets-repaired", &value))
        stats.packetsRepaired = value;

    if (gst_structure_get_uint64(structure, "packets-discarded", &value))
        stats.packetsDiscarded = value;

    if (gst_structure_get_uint64(structure, "packets-duplicated", &value))
        stats.packetsDuplicated = value;

    unsigned firCount;
    if (gst_structure_get_uint(structure, "fir-count", &firCount))
        stats.firCount = firCount;

    unsigned pliCount;
    if (gst_structure_get_uint(structure, "pli-count", &pliCount))
        stats.pliCount = pliCount;

    unsigned nackCount;
    if (gst_structure_get_uint(structure, "nack-count", &nackCount))
        stats.nackCount = nackCount;

    uint64_t bytesReceived;
    if (gst_structure_get_uint64(structure, "bytes-received", &bytesReceived))
        stats.bytesReceived = bytesReceived;

    if (additionalStats && gst_structure_get_uint64(additionalStats, "frames-decoded", &value))
        stats.framesDecoded = value;

    // FIXME:
    // stats.fractionLost =
    // stats.burstPacketsLost =
    // stats.burstPacketsDiscarded =
    // stats.burstLossCount =
    // stats.burstDiscardCount =
    // stats.burstLossRate =
    // stats.burstDiscardRate =
    // stats.gapLossRate =
    // stats.gapDiscardRate =
}

static inline void fillOutboundRTPStreamStats(RTCStatsReport::OutboundRtpStreamStats& stats, const GstStructure* structure, const GstStructure* additionalStats)
{
    fillRTCRTPStreamStats(stats, structure);

    uint64_t value;
    if (gst_structure_get_uint64(structure, "packets-sent", &value))
        stats.packetsSent = value;
    if (gst_structure_get_uint64(structure, "bytes-sent", &value))
        stats.bytesSent = value;

    unsigned firCount;
    if (gst_structure_get_uint(structure, "fir-count", &firCount))
        stats.firCount = firCount;

    unsigned pliCount;
    if (gst_structure_get_uint(structure, "pli-count", &pliCount))
        stats.pliCount = pliCount;

    unsigned nackCount;
    if (gst_structure_get_uint(structure, "nack-count", &nackCount))
        stats.nackCount = nackCount;

    if (const char* remoteId = gst_structure_get_string(structure, "remote-id"))
        stats.remoteId = String::fromLatin1(remoteId);

    if (!additionalStats)
        return;

    if (gst_structure_get_uint64(additionalStats, "frames-sent", &value))
        stats.framesSent = value;
    if (gst_structure_get_uint64(additionalStats, "frames-encoded", &value))
        stats.framesEncoded = value;

    double bitrate;
    if (gst_structure_get_double(additionalStats, "bitrate", &bitrate))
        stats.targetBitrate = bitrate;
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

static inline std::optional<RTCIceCandidateType> iceCandidateType(const String& type)
{
    if (type == "host"_s)
        return RTCIceCandidateType::Host;
    if (type == "srflx"_s)
        return RTCIceCandidateType::Srflx;
    if (type == "prflx"_s)
        return RTCIceCandidateType::Prflx;
    if (type == "relay"_s)
        return RTCIceCandidateType::Relay;

    return { };
}

static inline void fillRTCCandidateStats(RTCStatsReport::IceCandidateStats& stats, GstWebRTCStatsType statsType, const GstStructure* structure)
{
    stats.type = statsType == GST_WEBRTC_STATS_REMOTE_CANDIDATE ? RTCStatsReport::Type::RemoteCandidate : RTCStatsReport::Type::LocalCandidate;

    fillRTCStats(stats, structure);

    stats.transportId = String::fromLatin1(gst_structure_get_string(structure, "transport-id"));
    stats.address = String::fromLatin1(gst_structure_get_string(structure, "address"));
    stats.protocol = String::fromLatin1(gst_structure_get_string(structure, "protocol"));
    stats.url = String::fromLatin1(gst_structure_get_string(structure, "url"));

    unsigned port;
    if (gst_structure_get_uint(structure, "port", &port))
        stats.port = port;

    auto candidateType = String::fromLatin1(gst_structure_get_string(structure, "candidate-type"));
    stats.candidateType = iceCandidateType(candidateType);

    uint64_t priority;
    if (gst_structure_get_uint64(structure, "priority", &priority))
        stats.priority = priority;
}

static inline void fillRTCCandidatePairStats(RTCStatsReport::IceCandidatePairStats& stats, const GstStructure* structure)
{
    fillRTCStats(stats, structure);

    stats.localCandidateId = String::fromLatin1(gst_structure_get_string(structure, "local-candidate-id"));
    stats.remoteCandidateId = String::fromLatin1(gst_structure_get_string(structure, "remote-candidate-id"));

    // FIXME
    // stats.transportId =
    // stats.state =
    // stats.priority =
    // stats.nominated =
    // stats.writable =
    // stats.readable =
    // stats.bytesSent =
    // stats.bytesReceived =
    // stats.totalRoundTripTime =
    // stats.currentRoundTripTime =
    // stats.availableOutgoingBitrate =
    // stats.availableIncomingBitrate =
    // stats.requestsReceived =
    // stats.requestsSent =
    // stats.responsesReceived =
    // stats.responsesSent =
    // stats.retransmissionsReceived =
    // stats.retransmissionsSent =
    // stats.consentRequestsReceived =
    // stats.consentRequestsSent =
    // stats.consentResponsesReceived =
    // stats.consentResponsesSent =
}

struct ReportHolder : public ThreadSafeRefCounted<ReportHolder> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ReportHolder);
public:
    ReportHolder(DOMMapAdapter* adapter, const GstStructure* additionalStats)
        : adapter(adapter)
        , additionalStats(additionalStats) { }

    DOMMapAdapter* adapter;
    const GstStructure* additionalStats;
};

static gboolean fillReportCallback(GQuark, const GValue* value, gpointer userData)
{
    if (!GST_VALUE_HOLDS_STRUCTURE(value))
        return TRUE;

    const GstStructure* structure = gst_value_get_structure(value);
    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return TRUE;

    auto* reportHolder = reinterpret_cast<ReportHolder*>(userData);
    DOMMapAdapter& report = *reportHolder->adapter;
    const auto* additionalStats = reportHolder->additionalStats;

    switch (statsType) {
    case GST_WEBRTC_STATS_CODEC: {
        RTCStatsReport::CodecStats stats;
        fillRTCCodecStats(stats, structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_INBOUND_RTP: {
        RTCStatsReport::InboundRtpStreamStats stats;
        fillInboundRTPStreamStats(stats, structure, additionalStats);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_OUTBOUND_RTP: {
        RTCStatsReport::OutboundRtpStreamStats stats;
        fillOutboundRTPStreamStats(stats, structure, additionalStats);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_INBOUND_RTP: {
        RTCStatsReport::RemoteInboundRtpStreamStats stats;
        fillRemoteInboundRTPStreamStats(stats, structure, additionalStats);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteInboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP: {
        // FIXME: Remote outbound RTP stats not exposed yet.
        break;
    }
    case GST_WEBRTC_STATS_CSRC:
        // Deprecated stats: csrc.
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
        // Deprecated stats: stream.
        break;
    case GST_WEBRTC_STATS_DATA_CHANNEL:
        // FIXME: Missing data-channel stats support.
        break;
    case GST_WEBRTC_STATS_LOCAL_CANDIDATE:
    case GST_WEBRTC_STATS_REMOTE_CANDIDATE:
        if (webkitGstCheckVersion(1, 21, 0)) {
            RTCStatsReport::IceCandidateStats stats;
            fillRTCCandidateStats(stats, statsType, structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidateStats>>(stats.id, WTFMove(stats));
        }
        break;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
        if (webkitGstCheckVersion(1, 21, 0)) {
            RTCStatsReport::IceCandidatePairStats stats;
            fillRTCCandidatePairStats(stats, structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidatePairStats>>(stats.id, WTFMove(stats));
        }
        break;
    case GST_WEBRTC_STATS_CERTIFICATE:
        // FIXME: Missing certificate stats support
        break;
    }

    return TRUE;
}

struct CallbackHolder {
    GStreamerStatsCollector::CollectorCallback callback;
    GUniquePtr<GstStructure> additionalStats;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(CallbackHolder)

void GStreamerStatsCollector::getStats(CollectorCallback&& callback, GstPad* pad, const GstStructure* additionalStats)
{
    if (!m_webrtcBin) {
        callback(nullptr);
        return;
    }

    auto* holder = createCallbackHolder();
    holder->callback = WTFMove(callback);
    if (additionalStats)
        holder->additionalStats.reset(gst_structure_copy(additionalStats));
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
            GUniquePtr<GstStructure> additionalStats;
            if (holder->additionalStats)
                additionalStats.reset(gst_structure_copy(holder->additionalStats.get()));
            holder->callback(RTCStatsReport::create([promise = GRefPtr<GstPromise>(promise.get()), additionalStats = WTFMove(additionalStats)](auto& mapAdapter) {
                const auto* stats = gst_promise_get_reply(promise.get());
                auto holder = adoptRef(*new ReportHolder(&mapAdapter, additionalStats.get()));
                gst_structure_foreach(stats, fillReportCallback, holder.ptr());
            }));
        });
    }, holder, reinterpret_cast<GDestroyNotify>(destroyCallbackHolder)));
}

}; // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
