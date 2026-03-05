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

GStreamerStatsCollector::GStreamerStatsCollector()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_stats_debug, "webkitwebrtcstats", 0, "WebKit WebRTC Stats");
    });
}

RTCStatsReport::Stats RTCStatsReport::Stats::convert(Type type, const GstStructure* structure)
{
    return Stats {
        // FIXME: This should probably call Performance::reduceTimeResolution() like the LibWebRTC collector.
        Seconds::fromMicroseconds(gstStructureGet<double>(structure, "timestamp"_s).value_or(0)).milliseconds(),
        type,
        gstStructureGetString(structure, "id"_s).span(),
    };
}

RTCStatsReport::RtpStreamStats RTCStatsReport::RtpStreamStats::convert(Type type, const GstStructure* structure)
{
    return RtpStreamStats {
        Stats::convert(type, structure),
        gstStructureGet<unsigned>(structure, "ssrc"_s).value_or(0),
        gstStructureGetString(structure, "kind"_s).span(),
        gstStructureGetString(structure, "transport-id"_s).span(),
        gstStructureGetString(structure, "codec-id"_s).span(),
    };
}

RTCStatsReport::SentRtpStreamStats RTCStatsReport::SentRtpStreamStats::convert(Type type, const GstStructure* structure)
{
    return SentRtpStreamStats {
        RtpStreamStats::convert(type, structure),
        gstStructureGet<uint64_t>(structure, "packets-sent"_s),
        gstStructureGet<uint64_t>(structure, "bytes-sent"_s),
    };
}

RTCStatsReport::CodecStats RTCStatsReport::CodecStats::convert(const GstStructure* structure)
{
    return CodecStats {
        Stats::convert(Type::Codec, structure),
        gstStructureGet<unsigned>(structure, "payload-type"_s).value_or(0),
        { }, // FIXME: Add support for `transportId`.
        gstStructureGetString(structure, "mime-type"_s).span(),
        gstStructureGet<unsigned>(structure, "clock-rate"_s),
        gstStructureGet<unsigned>(structure, "channels"_s),
        gstStructureGetString(structure, "sdp-fmtp-line"_s).span(),
    };
}

RTCStatsReport::ReceivedRtpStreamStats RTCStatsReport::ReceivedRtpStreamStats::convert(Type type, const GstStructure* structure)
{
    GUniqueOutPtr<GstStructure> rtpSourceStats;
    gst_structure_get(structure, "gst-rtpsource-stats", GST_TYPE_STRUCTURE, &rtpSourceStats.outPtr(), nullptr);

    return ReceivedRtpStreamStats {
        RtpStreamStats::convert(type, structure),
        rtpSourceStats ? gstStructureGet<uint64_t>(rtpSourceStats.get(), "packets-received"_s) : std::nullopt,
#if GST_CHECK_VERSION(1, 22, 0)
        gstStructureGet<int64_t>(structure, "packets-lost"_s),
#else
        gstStructureGet<unsigned>(structure, "packets-lost"_s),
#endif
        gstStructureGet<double>(structure, "jitter"_s),
    };
}

RTCStatsReport::RemoteInboundRtpStreamStats RTCStatsReport::RemoteInboundRtpStreamStats::convert(const GstStructure* structure)
{
    return RemoteInboundRtpStreamStats {
        ReceivedRtpStreamStats::convert(Type::RemoteInboundRtp, structure),
        gstStructureGetString(structure, "local-id"_s).span(),
        gstStructureGet<double>(structure, "round-trip-time"_s),
        { }, // FIXME: Add support for `totalRoundTripTime`
        gstStructureGet<double>(structure, "fraction-lost"_s),
        { }, // FIXME: Add support for `roundTripTimeMeasurements`
    };
}

RTCStatsReport::RemoteOutboundRtpStreamStats RTCStatsReport::RemoteOutboundRtpStreamStats::convert(const GstStructure* structure)
{
    return RemoteOutboundRtpStreamStats {
        SentRtpStreamStats::convert(Type::RemoteOutboundRtp, structure),
        gstStructureGetString(structure, "local-id"_s).span(),
        gstStructureGet<double>(structure, "remote-timestamp"_s),
        { }, // FIXME: Add support for `reportsSent`
        { }, // FIXME: Add support for `roundTripTime`
        { }, // FIXME: Add support for `totalRoundTripTime`
        { }, // FIXME: Add support for `roundTripTimeMeasurements`
    };

}

RTCStatsReport::InboundRtpStreamStats RTCStatsReport::InboundRtpStreamStats::convert(const GstStructure* structure)
{
    auto getTrackIdentifier = [&] -> String {
        if (auto identifier = gstStructureGetString(structure, "track-identifier"_s))
            return identifier.span();
        return String();
    };

    return InboundRtpStreamStats {
        ReceivedRtpStreamStats::convert(Type::InboundRtp, structure),
        getTrackIdentifier(),
        { }, // FIXME: Add support for `mid`
        { }, // FIXME: Add support for `remoteId`
        gstStructureGet<uint64_t>(structure, "frames-decoded"_s),
        gstStructureGet<uint64_t>(structure, "key-frames-decoded"_s),
        { }, // FIXME: Add support for `framesRendered`
        gstStructureGet<uint64_t>(structure, "frames-dropped"_s),
        gstStructureGet<unsigned>(structure, "frame-width"_s),
        gstStructureGet<unsigned>(structure, "frame-height"_s),
        gstStructureGet<double>(structure, "frames-per-second"_s),
        { }, // FIXME: Add support for `qpSum`
        gstStructureGet<double>(structure, "total-decode-time"_s),
        { }, // FIXME: Add support for `totalInterFrameDelay`
        { }, // FIXME: Add support for `totalSquaredInterFrameDelay`
        { }, // FIXME: Add support for `pauseCount`
        { }, // FIXME: Add support for `totalPausesDuration`
        { }, // FIXME: Add support for `freezeCount`
        { }, // FIXME: Add support for `totalFreezesDuration`
        { }, // FIXME: Add support for `lastPacketReceivedTimestamp`
        { }, // FIXME: Add support for `headerBytesReceived`
        gstStructureGet<uint64_t>(structure, "packets-discarded"_s),
        { }, // FIXME: Add support for `fecBytesReceived`
        { }, // FIXME: Add support for `fecPacketsReceived`
        { }, // FIXME: Add support for `fecPacketsDiscarded`
        gstStructureGet<uint64_t>(structure, "bytes-received"_s),
        gstStructureGet<unsigned>(structure, "nack-count"_s),
        gstStructureGet<unsigned>(structure, "fir-count"_s),
        gstStructureGet<unsigned>(structure, "pli-count"_s),
        { }, // FIXME: Add support for `totalProcessingDelay`
        { }, // FIXME: Add support for `estimatedPlayoutTimestamp`
        { }, // FIXME: Add support for `jitterBufferDelay`
        { }, // FIXME: Add support for `jitterBufferTargetDelay`
        { }, // FIXME: Add support for `jitterBufferEmittedCount`
        { }, // FIXME: Add support for `jitterBufferMinimumDelay`
        { }, // FIXME: Add support for `totalSamplesReceived`
        { }, // FIXME: Add support for `concealedSamples`
        { }, // FIXME: Add support for `silentConcealedSamples`
        { }, // FIXME: Add support for `concealmentEvents`
        { }, // FIXME: Add support for `insertedSamplesForDeceleration`
        { }, // FIXME: Add support for `removedSamplesForAcceleration`
        { }, // FIXME: Add support for `audioLevel`
        { }, // FIXME: Add support for `totalAudioEnergy`
        { }, // FIXME: Add support for `totalSamplesDuration`
        gstStructureGet<uint64_t>(structure, "frames-received"_s),
        { }, // FIXME: Add support for `decoderImplementation`
        { }, // FIXME: Add support for `playoutId`
        { }, // FIXME: Add support for `powerEfficientDecoder`
        { }, // FIXME: Add support for `framesAssembledFromMultiplePackets`
        { }, // FIXME: Add support for `totalAssemblyTime`
        { }, // FIXME: Add support for `retransmittedPacketsReceived`
        { }, // FIXME: Add support for `retransmittedBytesReceived`
        { }, // FIXME: Add support for `rtxSsrc`
        { }, // FIXME: Add support for `fecSsrc`
    };
}

RTCStatsReport::OutboundRtpStreamStats RTCStatsReport::OutboundRtpStreamStats::convert(const GstStructure* structure)
{
    auto getMid = [&] -> String {
        if (auto identifier = gstStructureGetString(structure, "mid"_s))
            return identifier.span();
        return String();
    };
    auto getRid = [&] -> String {
        if (auto identifier = gstStructureGetString(structure, "rid"_s))
            return identifier.span();
        return String();
    };

    return OutboundRtpStreamStats {
        SentRtpStreamStats::convert(Type::OutboundRtp, structure),
        getMid(),
        { }, // FIXME: Add support for `mediaSourceId`
        gstStructureGetString(structure, "remote-id"_s).span(),
        getRid(),
        { }, // FIXME: Add support for `headerBytesSent`
        { }, // FIXME: Add support for `retransmittedPacketsSent`
        { }, // FIXME: Add support for `retransmittedBytesSent`
        { }, // FIXME: Add support for `rtxSsrc`
        gstStructureGet<double>(structure, "target-bitrate"_s),
        { }, // FIXME: Add support for `totalEncodedBytesTarget`
        gstStructureGet<unsigned>(structure, "frame-width"_s),
        gstStructureGet<unsigned>(structure, "frame-height"_s),
        gstStructureGet<double>(structure, "frames-per-second"_s),
        gstStructureGet<uint64_t>(structure, "frames-sent"_s),
        { }, // FIXME: Add support for `hugeFramesSent`
        gstStructureGet<uint64_t>(structure, "frames-encoded"_s),
        { }, // FIXME: Add support for `keyFramesEncoded`
        { }, // FIXME: Add support for `qpSum`
        { }, // FIXME: Add support for `totalEncodeTime`
        { }, // FIXME: Add support for `totalPacketSendDelay`
        { }, // FIXME: Add support for `qualityLimitationReason`
        { }, // FIXME: Add support for `qualityLimitationDurations;
        { }, // FIXME: Add support for `qualityLimitationResolutionChanges`
        gstStructureGet<unsigned>(structure, "nack-count"_s),
        gstStructureGet<unsigned>(structure, "fir-count"_s),
        gstStructureGet<unsigned>(structure, "pli-count"_s),
        { }, // FIXME: Add support for `active`
        { }, // FIXME: Add support for `scalabilityMode`
    };
}

RTCStatsReport::PeerConnectionStats RTCStatsReport::PeerConnectionStats::convert(const GstStructure* structure)
{
    return PeerConnectionStats {
        Stats::convert(Type::PeerConnection, structure),
        gstStructureGet<int>(structure, "data-channels-opened"_s),
        gstStructureGet<int>(structure, "data-channels-closed"_s),
    };
}

RTCStatsReport::TransportStats RTCStatsReport::TransportStats::convert(const GstStructure* structure)
{
    auto getDtlsState = [&] -> RTCDtlsTransportState {
        // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/9e38ee7526ecbb12320d1aef29a0c74b815eb4ef
        if (gst_structure_has_field_typed(structure, "dtls-state", GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE)) {
            GstWebRTCDTLSTransportState state;
            gst_structure_get(structure, "dtls-state", GST_TYPE_WEBRTC_DTLS_TRANSPORT_STATE, &state, nullptr);
            return toRTCDtlsTransportState(state);
        } else {
            // Our GStreamer version is likely too old, but this field being required, hard-code it to Connected.
            return RTCDtlsTransportState::Connected;
        }
    };

    auto getDtlsRole = [&] -> std::optional<DtlsRole> {
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/commit/9e38ee7526ecbb12320d1aef29a0c74b815eb4ef
#if GST_CHECK_VERSION(1, 28, 0)
        if (!gst_structure_has_field_typed(structure, "dtls-role", GST_TYPE_WEBRTC_DTLS_ROLE))
            return std::nullopt;

        GstWebRTCDTLSRole role;
        gst_structure_get(structure, "dtls-role", GST_TYPE_WEBRTC_DTLS_ROLE, &role, nullptr);
        switch (role) {
        case GST_WEBRTC_DTLS_ROLE_CLIENT:
            return DtlsRole::Client;
        case GST_WEBRTC_DTLS_ROLE_SERVER:
            return DtlsRole::Server;
        case GST_WEBRTC_DTLS_ROLE_UNKNOWN:
            return DtlsRole::Unknown;
        }
#else
        return std::nullopt;
#endif
    };

    return TransportStats {
        Stats::convert(Type::Transport, structure),
        { }, // FIXME: Add support for `packetsSent`
        { }, // FIXME: Add support for `packetsReceived`
        { }, // FIXME: Add support for `bytesSent`
        { }, // FIXME: Add support for `bytesReceived`
        { }, // FIXME: Add support for `iceRole`
        { }, // FIXME: Add support for `iceLocalUsernameFragment`
        getDtlsState(),
        { }, // FIXME: Add support for `iceState`
        gstStructureGetString(structure, "selected-candidate-pair-id"_s).span(),
        { }, // FIXME: Add support for `localCertificateId`
        { }, // FIXME: Add support for `remoteCertificateId`
        gstStructureGetString(structure, "tls-version"_s).span(),
        gstStructureGetString(structure, "dtls-cipher"_s).span(),
        getDtlsRole(),
        gstStructureGetString(structure, "srtp-cipher"_s).span(),
        { }, // FIXME: Add support for `selectedCandidatePairChanges`
    };
}

RTCStatsReport::IceCandidateStats RTCStatsReport::IceCandidateStats::convert(GstWebRTCStatsType statsType, const GstStructure* structure)
{
    auto getCandidateType = [&] -> RTCIceCandidateType {
        if (auto value = gstStructureGetString(structure, "candidate-type"_s)) {
            if (auto iceCandidateType = toRTCIceCandidateType(StringView::fromLatin1(value.utf8())))
                return *iceCandidateType;
        }
        return RTCIceCandidateType::Host;
    };

    auto getTcpType = [&] -> std::optional<RTCIceTcpCandidateType> {
#if GST_CHECK_VERSION(1, 28, 0)
        GstWebRTCICETcpCandidateType gstTcpType;
        if (gst_structure_get(structure, "tcp-type", &gstTcpType, nullptr)) {
            switch (gstTcpType) {
            case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_ACTIVE:
                return RTCIceTcpCandidateType::Active;
            case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_PASSIVE:
                return RTCIceTcpCandidateType::Passive;
            case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_SO:
                return RTCIceTcpCandidateType::So;
            case GST_WEBRTC_ICE_TCP_CANDIDATE_TYPE_NONE:
                break;
            };
        }
#endif
        return std::nullopt;
    };

    return IceCandidateStats {
        Stats::convert(statsType == GST_WEBRTC_STATS_REMOTE_CANDIDATE ? Type::RemoteCandidate : Type::LocalCandidate, structure),
        gstStructureGetString(structure, "transport-id"_s).span(),
        { }, // NOTE: We have the `address` field in the structure but we don't expose it for privacy reasons. Covered by test: webrtc/candidate-stats.html
        gstStructureGet<unsigned>(structure, "port"_s),
        gstStructureGetString(structure, "protocol"_s).span(),
        getCandidateType(),
        gstStructureGet<unsigned>(structure, "priority"_s),
        gstStructureGetString(structure, "url"_s).span(),
        { }, // FIXME: Add support for `relayProtocol`
        gstStructureGetString(structure, "foundation"_s).span(),
        { }, // FIXME: Add support for `relatedAddress`
        { }, // FIXME: Add support for `relatedPort`
        gstStructureGetString(structure, "username-fragment"_s).span(),
        getTcpType(),
    };
}

RTCStatsReport::IceCandidatePairStats RTCStatsReport::IceCandidatePairStats::convert(const GstStructure* structure)
{
    return IceCandidatePairStats {
        Stats::convert(Type::CandidatePair, structure),
        { }, // FIXME: Add support for `transportId`
        gstStructureGetString(structure, "local-candidate-id"_s).span(),
        gstStructureGetString(structure, "remote-candidate-id"_s).span(),
        RTCStatsReport::IceCandidatePairState::Succeeded,
        { }, // FIXME: Add support for `nominated`
        { }, // FIXME: Add support for `packetsSent`
        { }, // FIXME: Add support for `packetsReceived`
        { }, // FIXME: Add support for `bytesSent`
        { }, // FIXME: Add support for `bytesReceived`
        { }, // FIXME: Add support for `lastPacketSentTimestamp`
        { }, // FIXME: Add support for `lastPacketReceivedTimestamp`
        { }, // FIXME: Add support for `totalRoundTripTime`
        { }, // FIXME: Add support for `currentRoundTripTime`
        { }, // FIXME: Add support for `availableOutgoingBitrate`
        { }, // FIXME: Add support for `availableIncomingBitrate`
        { }, // FIXME: Add support for `requestsReceived`
        { }, // FIXME: Add support for `requestsSent`
        { }, // FIXME: Add support for `responsesReceived`
        { }, // FIXME: Add support for `responsesSent`
        { }, // FIXME: Add support for `consentRequestsSent`
        { }, // FIXME: Add support for `packetsDiscardedOnSend`
        { }, // FIXME: Add support for `bytesDiscardedOnSend`
    };
}

RTCStatsReport::CertificateStats RTCStatsReport::CertificateStats::convert(const GstStructure* structure)
{
    return CertificateStats {
        Stats::convert(Type::Certificate, structure),
        gstStructureGetString(structure, "fingerprint"_s).span(),
        gstStructureGetString(structure, "fingerprint-algorithm"_s).span(),
        gstStructureGetString(structure, "base64-certificate"_s).span(),
        { }, // FIXME: Add support for `issuerCertificateId`
    };
}

RTCStatsReport::MediaSourceStats RTCStatsReport::MediaSourceStats::convert(Type type, const GstStructure* structure)
{
    return MediaSourceStats {
        Stats::convert(type, structure),
        gstStructureGetString(structure, "track-identifier"_s).span(),
        gstStructureGetString(structure, "kind"_s).span(),
    };
}

RTCStatsReport::AudioSourceStats RTCStatsReport::AudioSourceStats::convert(const GstStructure* structure)
{
    return AudioSourceStats {
        MediaSourceStats::convert(Type::MediaSource, structure),
        gstStructureGet<double>(structure, "audio-level"_s),
        gstStructureGet<double>(structure, "total-audio-energy"_s),
        gstStructureGet<double>(structure, "total-samples-duration"_s),
        { }, // FIXME: Add support for `echoReturnLoss`
        { }, // FIXME: Add support for `echoReturnLossEnhancement`
    };
}

RTCStatsReport::VideoSourceStats RTCStatsReport::VideoSourceStats::convert(const GstStructure* structure)
{
    return VideoSourceStats {
        MediaSourceStats::convert(Type::MediaSource, structure),
        gstStructureGet<unsigned>(structure, "width"_s),
        gstStructureGet<unsigned>(structure, "height"_s),
        gstStructureGet<unsigned>(structure, "frames"_s),
        gstStructureGet<double>(structure, "frames-per-second"_s),
    };
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

    if (!reportHolder->adapter) [[unlikely]]
        return TRUE;

    auto& report = *reportHolder->adapter;

    if (auto webkitStatsType = gstStructureGetString(structure, "webkit-stats-type"_s)) {
        if (webkitStatsType == "audio-source-stats"_s) {
            auto stats = RTCStatsReport::AudioSourceStats::convert(structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::AudioSourceStats>>(stats.id, WTF::move(stats));
            return TRUE;
        }
        if (webkitStatsType == "video-source-stats"_s) {
            auto stats = RTCStatsReport::VideoSourceStats::convert(structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::VideoSourceStats>>(stats.id, WTF::move(stats));
            return TRUE;
        }
    }

    GstWebRTCStatsType statsType;
    if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr))
        return TRUE;

    switch (statsType) {
    case GST_WEBRTC_STATS_CODEC: {
        auto stats = RTCStatsReport::CodecStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_INBOUND_RTP: {
        auto stats = RTCStatsReport::InboundRtpStreamStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_OUTBOUND_RTP: {
        auto stats = RTCStatsReport::OutboundRtpStreamStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_INBOUND_RTP: {
        auto stats = RTCStatsReport::RemoteInboundRtpStreamStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteInboundRtpStreamStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_REMOTE_OUTBOUND_RTP: {
        auto stats = RTCStatsReport::RemoteOutboundRtpStreamStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteOutboundRtpStreamStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_CSRC:
        // Deprecated stats: csrc.
        break;
    case GST_WEBRTC_STATS_PEER_CONNECTION: {
        auto stats = RTCStatsReport::PeerConnectionStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::PeerConnectionStats>>(stats.id, WTF::move(stats));
        break;
    }
    case GST_WEBRTC_STATS_TRANSPORT: {
        auto stats = RTCStatsReport::TransportStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::TransportStats>>(stats.id, WTF::move(stats));
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
            auto stats = RTCStatsReport::IceCandidateStats::convert(statsType, structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidateStats>>(stats.id, WTF::move(stats));
        }
        break;
    case GST_WEBRTC_STATS_CANDIDATE_PAIR:
        if (gst_check_version(1, 22, 0)) {
            auto stats = RTCStatsReport::IceCandidatePairStats::convert(structure);
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidatePairStats>>(stats.id, WTF::move(stats));
        }
        break;
    case GST_WEBRTC_STATS_CERTIFICATE: {
        // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/10313
        auto stats = RTCStatsReport::CertificateStats::convert(structure);
        report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CertificateStats>>(stats.id, WTF::move(stats));
        break;
    }
    }

    return TRUE;
}

struct CallbackHolder {
    GStreamerStatsCollector::StatsCallback callback;
    GStreamerStatsCollector::PreprocessCallback preprocessCallback;
    GRefPtr<GstPad> pad;
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(CallbackHolder)

void GStreamerStatsCollector::gatherStats(StatsCallback&& callback, const GRefPtr<GstPad>& pad, PreprocessCallback&& preprocessCallback)
{
    auto webrtcBin = m_webrtcBin.get();
    if (!webrtcBin) {
        callback(nullptr);
        return;
    }

    auto* holder = createCallbackHolder();
    holder->callback = WTF::move(callback);
    holder->preprocessCallback = WTF::move(preprocessCallback);
    holder->pad = pad;
    g_signal_emit_by_name(webrtcBin.get(), "get-stats", pad.get(), gst_promise_new_with_change_func([](GstPromise* rawPromise, gpointer userData) mutable {
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

        callOnMainThreadAndWait([holder, stats] {
            holder->callback(holder->preprocessCallback(holder->pad, stats));
        });
    }, holder, reinterpret_cast<GDestroyNotify>(destroyCallbackHolder)));
}

void GStreamerStatsCollector::getStats(CollectorCallback&& callback, const GRefPtr<GstPad>& pad, PreprocessCallback&& preprocessCallback)
{
    static auto s_maximumReportAge = 300_ms;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto expirationTime = StringView::fromLatin1(std::getenv("WEBKIT_GST_WEBRTC_STATS_CACHE_EXPIRATION_TIME_MS"));
        if (expirationTime.isEmpty())
            return;

        if (auto milliseconds = WTF::parseInteger<int>(expirationTime))
            s_maximumReportAge = Seconds::fromMilliseconds(*milliseconds);
    });

    auto webrtcBin = m_webrtcBin.get();
    if (!webrtcBin) {
        callback(nullptr);
        return;
    }

    auto now = MonotonicTime::now();
    if (!pad) {
        if (m_cachedGlobalReport && (now - m_cachedGlobalReport->generationTime < s_maximumReportAge)) {
            GST_TRACE_OBJECT(webrtcBin.get(), "Returning cached global stats report");
            callback(m_cachedGlobalReport->report.get());
            return;
        }
    } else if (auto report = m_cachedReportsPerPad.getOptional(pad)) {
        if (now - report->generationTime < s_maximumReportAge) {
            GST_TRACE_OBJECT(webrtcBin.get(), "Returning cached stats report for pad %" GST_PTR_FORMAT, pad.get());
            callback(report->report.get());
            return;
        }
    }

    gatherStats([this, pad = GRefPtr(pad), callback = WTF::move(callback)](auto&& stats) mutable {
        if (!stats) {
            callback(nullptr);
            return;
        }
        auto report = RTCStatsReport::create([stats = WTF::move(stats)](auto& mapAdapter) mutable {
            auto holder = adoptRef(*new ReportHolder(&mapAdapter));
            gstStructureForeach(stats.get(), [&](auto, const auto value) -> bool {
                return fillReportCallback(value, holder);
            });
        });
        CachedReport cachedReport;
        cachedReport.generationTime = MonotonicTime::now();
        cachedReport.report = report.ptr();
        if (pad)
            m_cachedReportsPerPad.set(pad, WTF::move(cachedReport));
        else
            m_cachedGlobalReport = WTF::move(cachedReport);
        callback(WTF::move(report));
    }, pad, WTF::move(preprocessCallback));
}

void GStreamerStatsCollector::invalidateCache()
{
    ASSERT(isMainThread());
    m_cachedGlobalReport = std::nullopt;
    m_cachedReportsPerPad.clear();
}

void GStreamerStatsCollector::gatherDecoderImplementationName(const GRefPtr<GstPad>& pad, PreprocessCallback&& preprocessCallback, Function<void(String&&)>&& callback)
{
    gatherStats([callback = WTF::move(callback)](auto&& stats) mutable {
        if (!stats) {
            callback({ });
            return;
        }
        auto decoderImplementation = emptyString();
        gstStructureForeach(stats.get(), [&](auto, const auto value) -> bool {
            if (!GST_VALUE_HOLDS_STRUCTURE(value)) [[unlikely]]
                return true;

            const GstStructure* structure = gst_value_get_structure(value);
            GstWebRTCStatsType statsType;
            if (!gst_structure_get(structure, "type", GST_TYPE_WEBRTC_STATS_TYPE, &statsType, nullptr)) [[unlikely]]
                return true;

            if (statsType != GST_WEBRTC_STATS_INBOUND_RTP)
                return true;

            auto kind = gstStructureGetString(structure, "kind"_s);
            if (kind != "video"_s)
                return true;

            decoderImplementation = gstStructureGetString(structure, "decoder-implementation"_s).span();
            return false;
        });
        callback(WTF::move(decoderImplementation));
    }, pad, WTF::move(preprocessCallback));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
