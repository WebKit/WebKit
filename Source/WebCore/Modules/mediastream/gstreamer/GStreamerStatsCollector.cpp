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
#include "GStreamerWebRTCUtils.h"
#include "JSDOMMapLike.h"
#include "JSRTCStatsReport.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_stats_debug);
#define GST_CAT_DEFAULT webkit_webrtc_stats_debug

namespace WebCore {

RTCStatsReport::Stats::Stats(Type type, const GstStructure* structure)
    : type(type)
    , id(gstStructureGetString(structure, "id"_s).span())
{
    if (auto value = gstStructureGet<double>(structure, "timestamp"_s))
        timestamp = Seconds::fromMicroseconds(*value).milliseconds();
}

RTCStatsReport::RtpStreamStats::RtpStreamStats(Type type, const GstStructure* structure)
    : Stats(type, structure)
    , kind(gstStructureGetString(structure, "kind"_s).span())
    , transportId(gstStructureGetString(structure, "transport-id"_s).span())
    , codecId(gstStructureGetString(structure, "codec-id"_s).span())
{
    if (auto value = gstStructureGet<unsigned>(structure, "ssrc"_s))
        ssrc = *value;
}

RTCStatsReport::SentRtpStreamStats::SentRtpStreamStats(Type type, const GstStructure* structure)
    : RtpStreamStats(type, structure)
{
    packetsSent = gstStructureGet<uint64_t>(structure, "packets-sent"_s);
    bytesSent = gstStructureGet<uint64_t>(structure, "bytes-sent"_s);
}

RTCStatsReport::CodecStats::CodecStats(const GstStructure* structure)
    : Stats(Type::Codec, structure)
    , mimeType(gstStructureGetString(structure, "mime-type"_s).span())
    , sdpFmtpLine(gstStructureGetString(structure, "sdp-fmtp-line"_s).span())
{
    clockRate = gstStructureGet<unsigned>(structure, "clock-rate"_s);
    channels = gstStructureGet<unsigned>(structure, "channels"_s);

    if (auto value = gstStructureGet<unsigned>(structure, "payload-type"_s))
        payloadType = *value;

    // FIXME:
    // stats.implementation =
}

RTCStatsReport::ReceivedRtpStreamStats::ReceivedRtpStreamStats(Type type, const GstStructure* structure)
    : RtpStreamStats(type, structure)
{
    GUniqueOutPtr<GstStructure> rtpSourceStats;
    gst_structure_get(structure, "gst-rtpsource-stats", GST_TYPE_STRUCTURE, &rtpSourceStats.outPtr(), nullptr);

    if (rtpSourceStats)
        packetsReceived = gstStructureGet<uint64_t>(rtpSourceStats.get(), "packets-received"_s);

#if GST_CHECK_VERSION(1, 22, 0)
    packetsLost = gstStructureGet<int64_t>(structure, "packets-lost"_s);
#else
    packetsLost = gstStructureGet<unsigned>(structure, "packets-lost"_s);
#endif

    jitter = gstStructureGet<double>(structure, "jitter"_s);
}

RTCStatsReport::RemoteInboundRtpStreamStats::RemoteInboundRtpStreamStats(const GstStructure* structure)
    : ReceivedRtpStreamStats(Type::RemoteInboundRtp, structure)
    , localId(gstStructureGetString(structure, "local-id"_s).span())
{
    roundTripTime = gstStructureGet<double>(structure, "round-trip-time"_s);
    fractionLost = gstStructureGet<double>(structure, "fraction-lost"_s);

    // FIXME:
    // stats.reportsReceived
    // stats.roundTripTimeMeasurements
}

RTCStatsReport::RemoteOutboundRtpStreamStats::RemoteOutboundRtpStreamStats(const GstStructure* structure)
    : SentRtpStreamStats(Type::RemoteOutboundRtp, structure)
    , localId(gstStructureGetString(structure, "local-id"_s).span())
{
    remoteTimestamp = gstStructureGet<double>(structure, "remote-timestamp"_s);

    // FIXME:
    // stats.roundTripTime
    // stats.reportsSent
    // stats.totalRoundTripTime
    // stats.roundTripTimeMeasurements
}

RTCStatsReport::InboundRtpStreamStats::InboundRtpStreamStats(const GstStructure* structure)
    : ReceivedRtpStreamStats(Type::InboundRtp, structure)
{
    bytesReceived = gstStructureGet<uint64_t>(structure, "bytes-received"_s);
    packetsDiscarded = gstStructureGet<uint64_t>(structure, "packets-discarded"_s);
    packetsDuplicated = gstStructureGet<uint64_t>(structure, "packets-duplicated"_s);
    firCount = gstStructureGet<unsigned>(structure, "fir-count"_s);
    pliCount = gstStructureGet<unsigned>(structure, "pli-count"_s);
    nackCount = gstStructureGet<unsigned>(structure, "nack-count"_s);

    decoderImplementation = "GStreamer"_s;

    framesPerSecond = gstStructureGet<double>(structure, "frames-per-second"_s);
    totalDecodeTime = gstStructureGet<double>(structure, "total-decode-time"_s);
    framesDecoded = gstStructureGet<uint64_t>(structure, "frames-decoded"_s);
    framesDropped = gstStructureGet<uint64_t>(structure, "frames-dropped"_s);
    frameWidth = gstStructureGet<unsigned>(structure, "frame-width"_s);
    frameHeight = gstStructureGet<unsigned>(structure, "frame-height"_s);

    if (auto identifier = gstStructureGetString(structure, "track-identifier"_s))
        trackIdentifier = identifier.span();

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

RTCStatsReport::OutboundRtpStreamStats::OutboundRtpStreamStats(const GstStructure* structure)
    : SentRtpStreamStats(Type::OutboundRtp, structure)
    , remoteId(gstStructureGetString(structure, "remote-id"_s).span())
{
    firCount = gstStructureGet<unsigned>(structure, "fir-count"_s);
    pliCount = gstStructureGet<unsigned>(structure, "pli-count"_s);
    nackCount = gstStructureGet<unsigned>(structure, "nack-count"_s);

    framesSent = gstStructureGet<uint64_t>(structure, "frames-sent"_s);
    framesEncoded = gstStructureGet<uint64_t>(structure, "frames-encoded"_s);
    targetBitrate = gstStructureGet<double>(structure, "target-bitrate"_s);
    frameWidth = gstStructureGet<unsigned>(structure, "frame-width"_s);
    frameHeight = gstStructureGet<unsigned>(structure, "frame-height"_s);
    framesPerSecond = gstStructureGet<double>(structure, "frames-per-second"_s);

    if (auto midValue = gstStructureGetString(structure, "mid"_s))
        mid = midValue.span();
    if (auto ridValue = gstStructureGetString(structure, "rid"_s))
        rid = ridValue.span();
}

RTCStatsReport::PeerConnectionStats::PeerConnectionStats(const GstStructure* structure)
    : Stats(Type::PeerConnection, structure)
{
    dataChannelsOpened = gstStructureGet<int>(structure, "data-channels-opened"_s);
    dataChannelsClosed = gstStructureGet<int>(structure, "data-channels-closed"_s);
}

RTCStatsReport::TransportStats::TransportStats(const GstStructure* structure)
    : Stats(Type::Transport, structure)
    , selectedCandidatePairId(gstStructureGetString(structure, "selected-candidate-pair-id"_s).span())
{
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/9e38ee7526ecbb12320d1aef29a0c74b815eb4ef
    if (gst_structure_has_field_typed(structure, "dtls-state", GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE)) {
        GstWebRTCDTLSTransportState state;
        gst_structure_get(structure, "dtls-state", GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE, &state, nullptr);
        dtlsState = toRTCDtlsTransportState(state);
    } else {
        // Our GStreamer version is likely too old, but this field being required, hard-code it to Connected.
        dtlsState = RTCDtlsTransportState::Connected;
    }

    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/9e38ee7526ecbb12320d1aef29a0c74b815eb4ef
#if GST_CHECK_VERSION(1, 27, 0)
    if (gst_structure_has_field_typed(structure, "dtls-role", GST_TYPE_WEBRTC_DTLS_ROLE)) {
        GstWebRTCDTLSRole role;
        gst_structure_get(structure, "dtls-role", GST_TYPE_WEBRTC_DTLS_ROLE, &role, nullptr);
        switch (role) {
        case GST_WEBRTC_DTLS_ROLE_CLIENT:
            dtlsRole = DtlsRole::Client;
            break;
        case GST_WEBRTC_DTLS_ROLE_SERVER:
            dtlsRole = DtlsRole::Server;
            break;
        case GST_WEBRTC_DTLS_ROLE_UNKNOWN:
            dtlsRole = DtlsRole::Unknown;
            break;
        }
    }
#endif
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

static inline RTCIceCandidateType iceCandidateType(CStringView type)
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
    , transportId(gstStructureGetString(structure, "transport-id"_s).span())
    , address(gstStructureGetString(structure, "address"_s).span())
    , protocol(gstStructureGetString(structure, "protocol"_s).span())
    , url(gstStructureGetString(structure, "url"_s).span())
{
    port = gstStructureGet<unsigned>(structure, "port"_s);
    priority = gstStructureGet<unsigned>(structure, "priority"_s);

    if (auto value = gstStructureGetString(structure, "candidate-type"_s))
        candidateType = iceCandidateType(value);
}

RTCStatsReport::IceCandidatePairStats::IceCandidatePairStats(const GstStructure* structure)
    : Stats(Type::CandidatePair, structure)
    , localCandidateId(gstStructureGetString(structure, "local-candidate-id"_s).span())
    , remoteCandidateId(gstStructureGetString(structure, "remote-candidate-id"_s).span())
{
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
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ReportHolder);
    WTF_MAKE_NONCOPYABLE(ReportHolder);
public:
    ReportHolder(DOMMapAdapter* adapter)
        : adapter(adapter) { }

    DOMMapAdapter* adapter;
};

static gboolean fillReportCallback(const GValue* value, Ref<ReportHolder>& reportHolder)
{
    if (!GST_VALUE_HOLDS_STRUCTURE(value))
        return TRUE;

    const GstStructure* structure = gst_value_get_structure(value);
    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return TRUE;

    if (!reportHolder->adapter) [[unlikely]]
        return TRUE;

    auto& report = *reportHolder->adapter;

    switch (statsType) {
    case GST_WEBRTC_STATS_CODEC: {
        RTCStatsReport::CodecStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_INBOUND_RTP: {
        RTCStatsReport::InboundRtpStreamStats stats(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        break;
    }
    case GST_WEBRTC_STATS_OUTBOUND_RTP: {
        RTCStatsReport::OutboundRtpStreamStats stats(structure);
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
        if (gst_check_version(1, 22, 0)) {
            RTCStatsReport::IceCandidateStats stats(statsType, structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidateStats>>(stats.id, WTFMove(stats));
        }
        break;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
        if (gst_check_version(1, 22, 0)) {
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
    RefPtr<GStreamerStatsCollector> collector;
    GStreamerStatsCollector::CollectorCallback callback;
    GStreamerStatsCollector::PreprocessCallback preprocessCallback;
    GRefPtr<GstPad> pad;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(CallbackHolder)

void GStreamerStatsCollector::getStats(CollectorCallback&& callback, const GRefPtr<GstPad>& pad, PreprocessCallback&& preprocessCallback)
{
    static auto s_maximumReportAge = 300_ms;
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_stats_debug, "webkitwebrtcstats", 0, "WebKit WebRTC Stats");
        auto expirationTime = StringView::fromLatin1(std::getenv("WEBKIT_GST_WEBRTC_STATS_CACHE_EXPIRATION_TIME_MS"));
        if (expirationTime.isEmpty())
            return;

        if (auto milliseconds = WTF::parseInteger<int>(expirationTime))
            s_maximumReportAge = Seconds::fromMilliseconds(*milliseconds);
    });

    if (!m_webrtcBin) {
        callback(nullptr);
        return;
    }

    auto now = MonotonicTime::now();
    if (!pad) {
        if (m_cachedGlobalReport && (now - m_cachedGlobalReport->generationTime < s_maximumReportAge)) {
            GST_TRACE_OBJECT(m_webrtcBin.get(), "Returning cached global stats report");
            callback(m_cachedGlobalReport->report.get());
            return;
        }
    } else if (auto report = m_cachedReportsPerPad.getOptional(pad)) {
        if (now - report->generationTime < s_maximumReportAge) {
            GST_TRACE_OBJECT(m_webrtcBin.get(), "Returning cached stats report for pad %" GST_PTR_FORMAT, pad.get());
            callback(report->report.get());
            return;
        }
    }

    auto* holder = createCallbackHolder();
    holder->collector = this;
    holder->callback = WTFMove(callback);
    holder->preprocessCallback = WTFMove(preprocessCallback);
    holder->pad = pad;
    g_signal_emit_by_name(m_webrtcBin.get(), "get-stats", pad.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) mutable {
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

        callOnMainThreadAndWait([holder, stats] mutable {
            auto preprocessedStats = holder->preprocessCallback(holder->pad, stats);
            if (!preprocessedStats)
                return;
            auto report = RTCStatsReport::create([stats = WTFMove(preprocessedStats)](auto& mapAdapter) mutable {
                auto holder = adoptRef(*new ReportHolder(&mapAdapter));
                gstStructureForeach(stats.get(), [&](auto, const auto value) -> bool {
                    return fillReportCallback(value, holder);
                });
            });
            CachedReport cachedReport;
            cachedReport.generationTime = MonotonicTime::now();
            cachedReport.report = report.ptr();
            if (holder->pad)
                holder->collector->m_cachedReportsPerPad.set(holder->pad, WTFMove(cachedReport));
            else
                holder->collector->m_cachedGlobalReport = WTFMove(cachedReport);
            holder->callback(WTFMove(report));
        });
    }, holder, reinterpret_cast<GDestroyNotify>(destroyCallbackHolder)));
}

void GStreamerStatsCollector::invalidateCache()
{
    ASSERT(isMainThread());
    m_cachedGlobalReport = std::nullopt;
    m_cachedReportsPerPad.clear();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
