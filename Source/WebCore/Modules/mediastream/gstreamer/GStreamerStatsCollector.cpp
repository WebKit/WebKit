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

RTCStatsReport::Stats::Stats(Type type, const GstStructure* structure)
    : type(type)
    , id(String::fromLatin1(gst_structure_get_string(structure, "id")))
{
    if (auto value = gstStructureGet<double>(structure, "timestamp"_s))
        timestamp = *value;
}

RTCStatsReport::RtpStreamStats::RtpStreamStats(Type type, const GstStructure* structure)
    : Stats(type, structure)
{
    transportId = String::fromLatin1(gst_structure_get_string(structure, "transport-id"));
    codecId = String::fromLatin1(gst_structure_get_string(structure, "codec-id"));

    if (auto value = gstStructureGet<unsigned>(structure, "ssrc"_s))
        ssrc = *value;

    if (const char* gstKind = gst_structure_get_string(structure, "kind"))
        kind = String::fromLatin1(gstKind);
}

RTCStatsReport::SentRtpStreamStats::SentRtpStreamStats(Type type, const GstStructure* structure)
    : RtpStreamStats(type, structure)
{
    packetsSent = gstStructureGet<uint64_t>(structure, "packets-sent"_s);
    bytesSent = gstStructureGet<uint64_t>(structure, "bytes-sent"_s);
}

RTCStatsReport::CodecStats::CodecStats(const GstStructure* structure)
    : Stats(Type::Codec, structure)
{
    payloadType = gstStructureGet<unsigned>(structure, "payload-type"_s);
    clockRate = gstStructureGet<unsigned>(structure, "clock-rate"_s);
    channels = gstStructureGet<unsigned>(structure, "channels"_s);

    if (const char* gstSdpFmtpLine = gst_structure_get_string(structure, "sdp-fmtp-line"))
        sdpFmtpLine = String::fromLatin1(gstSdpFmtpLine);

    if (const char* gstMIMEType = gst_structure_get_string(structure, "mime-type"))
        mimeType = String::fromLatin1(gstMIMEType);

    // FIXME:
    // stats.implementation =
}

RTCStatsReport::ReceivedRtpStreamStats::ReceivedRtpStreamStats(Type type, const GstStructure* structure)
    : RtpStreamStats(type, structure)
{
    GstStructure* rtpSourceStats;
    gst_structure_get(structure, "gst-rtpsource-stats", GST_TYPE_STRUCTURE, &rtpSourceStats, nullptr);

    packetsReceived = gstStructureGet<uint64_t>(rtpSourceStats, "packets-received"_s);

#if GST_CHECK_VERSION(1, 22, 0)
    packetsLost = gstStructureGet<int64_t>(structure, "packets-lost"_s);
#else
    packetsLost = gstStructureGet<unsigned>(structure, "packets-lost"_s);
#endif

    jitter = gstStructureGet<double>(structure, "jitter"_s);
}

RTCStatsReport::RemoteInboundRtpStreamStats::RemoteInboundRtpStreamStats(const GstStructure* structure)
    : ReceivedRtpStreamStats(Type::RemoteInboundRtp, structure)
{
    roundTripTime = gstStructureGet<double>(structure, "round-trip-time"_s);

    if (const char* gstLocalId = gst_structure_get_string(structure, "local-id"))
        localId = String::fromLatin1(gstLocalId);

    fractionLost = gstStructureGet<double>(structure, "fraction-lost"_s);

    // FIXME:
    // stats.reportsReceived
    // stats.roundTripTimeMeasurements
}

RTCStatsReport::RemoteOutboundRtpStreamStats::RemoteOutboundRtpStreamStats(const GstStructure* structure)
    : SentRtpStreamStats(Type::RemoteOutboundRtp, structure)
{
    remoteTimestamp = gstStructureGet<double>(structure, "remote-timestamp"_s);

    if (const char* gstLocalId = gst_structure_get_string(structure, "local-id"))
        localId = String::fromLatin1(gstLocalId);

    // FIXME:
    // stats.roundTripTime
    // stats.reportsSent
    // stats.totalRoundTripTime
    // stats.roundTripTimeMeasurements
}

RTCStatsReport::InboundRtpStreamStats::InboundRtpStreamStats(const GstStructure* structure, const GstStructure* additionalStats)
    : ReceivedRtpStreamStats(Type::InboundRtp, structure)
{
    bytesReceived = gstStructureGet<uint64_t>(structure, "bytes-received"_s);
    packetsDiscarded = gstStructureGet<uint64_t>(structure, "packets-discarded"_s);
    packetsDuplicated = gstStructureGet<uint64_t>(structure, "packets-duplicated"_s);
    firCount = gstStructureGet<unsigned>(structure, "fir-count"_s);
    pliCount = gstStructureGet<unsigned>(structure, "pli-count"_s);
    nackCount = gstStructureGet<unsigned>(structure, "nack-count"_s);

    decoderImplementation = "GStreamer"_s;

    if (!additionalStats)
        return;

    framesDecoded = gstStructureGet<uint64_t>(additionalStats, "frames-decoded"_s);
    framesDropped = gstStructureGet<uint64_t>(additionalStats, "frames-dropped"_s);
    frameWidth = gstStructureGet<unsigned>(additionalStats, "frame-width"_s);
    frameHeight = gstStructureGet<unsigned>(additionalStats, "frame-height"_s);

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

RTCStatsReport::OutboundRtpStreamStats::OutboundRtpStreamStats(const GstStructure* structure, const GstStructure* additionalStats)
    : SentRtpStreamStats(Type::OutboundRtp, structure)
{
    firCount = gstStructureGet<unsigned>(structure, "fir-count"_s);
    pliCount = gstStructureGet<unsigned>(structure, "pli-count"_s);
    nackCount = gstStructureGet<unsigned>(structure, "nack-count"_s);

    if (const char* gstRemoteId = gst_structure_get_string(structure, "remote-id"))
        remoteId = String::fromLatin1(gstRemoteId);

    if (!additionalStats)
        return;

    framesSent = gstStructureGet<uint64_t>(additionalStats, "frames-sent"_s);
    framesEncoded = gstStructureGet<uint64_t>(additionalStats, "frames-encoded"_s);
    targetBitrate = gstStructureGet<double>(additionalStats, "bitrate"_s);
}

RTCStatsReport::PeerConnectionStats::PeerConnectionStats(const GstStructure* structure)
    : Stats(Type::PeerConnection, structure)
{
    dataChannelsOpened = gstStructureGet<int>(structure, "data-channels-opened"_s);
    dataChannelsClosed = gstStructureGet<int>(structure, "data-channels-closed"_s);
}

RTCStatsReport::TransportStats::TransportStats(const GstStructure* structure)
    : Stats(Type::Transport, structure)
{
    if (const char* gstSelectedCandidatePairId = gst_structure_get_string(structure, "selected-candidate-pair-id"))
        selectedCandidatePairId = String::fromLatin1(gstSelectedCandidatePairId);

    // FIXME: This field is required, GstWebRTC doesn't provide it, so hard-code a value here.
    dtlsState = RTCDtlsTransportState::Connected;

    // FIXME
    // stats.bytesSent =
    // stats.bytesReceived =
    // stats.rtcpTransportStatsId =
    // stats.localCertificateId =
    // stats.remoteCertificateId =
    // stats.tlsVersion =
    // stats.dtlsCipher =
    // stats.srtpCipher =
}

static inline RTCIceCandidateType iceCandidateType(const String& type)
{
    if (type == "host"_s)
        return RTCIceCandidateType::Host;
    if (type == "srflx"_s)
        return RTCIceCandidateType::Srflx;
    if (type == "prflx"_s)
        return RTCIceCandidateType::Prflx;
    if (type == "relay"_s)
        return RTCIceCandidateType::Relay;
    ASSERT_NOT_REACHED();
    return RTCIceCandidateType::Host;
}

RTCStatsReport::IceCandidateStats::IceCandidateStats(GstWebRTCStatsType statsType, const GstStructure* structure)
    : Stats(statsType == GST_WEBRTC_STATS_REMOTE_CANDIDATE ? Type::RemoteCandidate : Type::LocalCandidate, structure)
{
    transportId = String::fromLatin1(gst_structure_get_string(structure, "transport-id"));
    address = String::fromLatin1(gst_structure_get_string(structure, "address"));
    protocol = String::fromLatin1(gst_structure_get_string(structure, "protocol"));
    url = String::fromLatin1(gst_structure_get_string(structure, "url"));

    port = gstStructureGet<unsigned>(structure, "port"_s);
    priority = gstStructureGet<unsigned>(structure, "priority"_s);

    auto gstIceCandidateType = String::fromLatin1(gst_structure_get_string(structure, "candidate-type"));
    candidateType = iceCandidateType(gstIceCandidateType);
}

RTCStatsReport::IceCandidatePairStats::IceCandidatePairStats(const GstStructure* structure)
    : Stats(Type::CandidatePair, structure)
{
    localCandidateId = String::fromLatin1(gst_structure_get_string(structure, "local-candidate-id"));
    remoteCandidateId = String::fromLatin1(gst_structure_get_string(structure, "remote-candidate-id"));

    // FIXME
    // stats.transportId =
    state = RTCStatsReport::IceCandidatePairState::Succeeded;
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
        RTCStatsReport::CodecStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_INBOUND_RTP: {
        RTCStatsReport::InboundRtpStreamStats stats(structure, additionalStats);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_OUTBOUND_RTP: {
        RTCStatsReport::OutboundRtpStreamStats stats(structure, additionalStats);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_INBOUND_RTP: {
        RTCStatsReport::RemoteInboundRtpStreamStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteInboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP: {
        RTCStatsReport::RemoteOutboundRtpStreamStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteOutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_CSRC:
        // Deprecated stats: csrc.
        break;
    case GST_WEBRTC_STATS_PEER_CONNECTION: {
        RTCStatsReport::PeerConnectionStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::PeerConnectionStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_TRANSPORT: {
        RTCStatsReport::TransportStats stats(structure);
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
        if (webkitGstCheckVersion(1, 22, 0)) {
            RTCStatsReport::IceCandidateStats stats(statsType, structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidateStats>>(stats.id, WTFMove(stats));
        }
        break;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
        if (webkitGstCheckVersion(1, 22, 0)) {
            RTCStatsReport::IceCandidatePairStats stats(structure);
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

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
