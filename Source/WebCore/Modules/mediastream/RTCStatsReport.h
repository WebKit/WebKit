/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RTCDtlsTransportState.h"
#include <WebCore/RTCIceCandidateType.h>
#include <WebCore/RTCIceRole.h>
#include <WebCore/RTCIceServerTransportProtocol.h>
#include <WebCore/RTCIceTcpCandidateType.h>
#include "RTCIceTransportState.h"
#include <wtf/KeyValuePair.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if USE(GSTREAMER_WEBRTC)
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API
#endif

#if USE(LIBWEBRTC)
namespace webrtc {
class RTCAudioPlayoutStats;
class RTCAudioSourceStats;
class RTCCertificateStats;
class RTCCodecStats;
class RTCDataChannelStats;
class RTCIceCandidatePairStats;
class RTCIceCandidateStats;
class RTCInboundRtpStreamStats;
class RTCInboundRtpStreamStats;
class RTCMediaSourceStats;
class RTCOutboundRtpStreamStats;
class RTCPeerConnectionStats;
class RTCReceivedRtpStreamStats;
class RTCRemoteIceCandidateStats;
class RTCRemoteInboundRtpStreamStats;
class RTCRemoteOutboundRtpStreamStats;
class RTCRtpStreamStats;
class RTCSentRtpStreamStats;
class RTCStats;
class RTCTransportStats;
class RTCVideoSourceStats;
}
#endif

namespace WebCore {

class DOMMapAdapter;

class RTCStatsReport : public RefCounted<RTCStatsReport> {
public:
    using MapInitializer = Function<void(DOMMapAdapter&)>;
    static Ref<RTCStatsReport> create(MapInitializer&& mapInitializer) { return adoptRef(*new RTCStatsReport(WTF::move(mapInitializer))); }

    void initializeMapLike(DOMMapAdapter& adapter) { m_mapInitializer(adapter); }

    enum class Type {
        Codec,
        InboundRtp,
        OutboundRtp,
        RemoteInboundRtp,
        RemoteOutboundRtp,
        MediaSource,
        MediaPlayout,
        PeerConnection,
        DataChannel,
        Transport,
        CandidatePair,
        LocalCandidate,
        RemoteCandidate,
        Certificate,
    };

    struct Stats {
        double timestamp { 0 };
        Type type;
        String id;

#if USE(LIBWEBRTC)
        static Stats convert(Type, const webrtc::RTCStats&);
#elif USE(GSTREAMER_WEBRTC)
        static Stats convert(Type, const GstStructure*);
#endif
    };

    struct RtpStreamStats : Stats {
        uint32_t ssrc { 0 };
        String kind;
        String transportId;
        String codecId;

#if USE(LIBWEBRTC)
        static RtpStreamStats convert(Type, const webrtc::RTCRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static RtpStreamStats convert(Type, const GstStructure*);
#endif
    };

    struct ReceivedRtpStreamStats : RtpStreamStats {
        std::optional<uint64_t> packetsReceived;
        std::optional<int64_t> packetsLost;
        std::optional<double> jitter;

#if USE(LIBWEBRTC)
        static ReceivedRtpStreamStats convert(Type, const webrtc::RTCReceivedRtpStreamStats&, std::optional<uint64_t> packetsReceived);
#elif USE(GSTREAMER_WEBRTC)
        static ReceivedRtpStreamStats convert(Type, const GstStructure*);
#endif
    };

    struct InboundRtpStreamStats : ReceivedRtpStreamStats {
        String trackIdentifier;
        String mid;
        String remoteId;
        std::optional<uint32_t> framesDecoded;
        std::optional<uint32_t> keyFramesDecoded;
        std::optional<uint32_t> framesRendered;
        std::optional<uint32_t> framesDropped;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<double> framesPerSecond;
        std::optional<uint64_t> qpSum;
        std::optional<double> totalDecodeTime;
        std::optional<double> totalInterFrameDelay;
        std::optional<double> totalSquaredInterFrameDelay;
        std::optional<uint32_t> pauseCount;
        std::optional<double> totalPausesDuration;
        std::optional<uint32_t> freezeCount;
        std::optional<double> totalFreezesDuration;
        std::optional<double> lastPacketReceivedTimestamp;
        std::optional<uint64_t> headerBytesReceived;
        std::optional<uint64_t> packetsDiscarded;
        std::optional<uint64_t> fecBytesReceived;
        std::optional<uint64_t> fecPacketsReceived;
        std::optional<uint64_t> fecPacketsDiscarded;
        std::optional<uint64_t> bytesReceived;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<double> totalProcessingDelay;
        std::optional<double> estimatedPlayoutTimestamp;
        std::optional<double> jitterBufferDelay;
        std::optional<double> jitterBufferTargetDelay;
        std::optional<uint64_t> jitterBufferEmittedCount;
        std::optional<double> jitterBufferMinimumDelay;
        std::optional<uint64_t> totalSamplesReceived;
        std::optional<uint64_t> concealedSamples;
        std::optional<uint64_t> silentConcealedSamples;
        std::optional<uint64_t> concealmentEvents;
        std::optional<uint64_t> insertedSamplesForDeceleration;
        std::optional<uint64_t> removedSamplesForAcceleration;
        std::optional<double> audioLevel;
        std::optional<double> totalAudioEnergy;
        std::optional<double> totalSamplesDuration;
        std::optional<uint32_t> framesReceived;
        String decoderImplementation;
        String playoutId;
        std::optional<bool> powerEfficientDecoder;
        std::optional<uint32_t> framesAssembledFromMultiplePackets;
        std::optional<double> totalAssemblyTime;
        std::optional<uint64_t> retransmittedPacketsReceived;
        std::optional<uint64_t> retransmittedBytesReceived;
        std::optional<uint32_t> rtxSsrc;
        std::optional<uint32_t> fecSsrc;

#if USE(LIBWEBRTC)
        static InboundRtpStreamStats convert(const webrtc::RTCInboundRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static InboundRtpStreamStats convert(const GstStructure*);
#endif
    };

    struct RemoteInboundRtpStreamStats : ReceivedRtpStreamStats {
        String localId;
        std::optional<double> roundTripTime;
        std::optional<double> totalRoundTripTime;
        std::optional<double> fractionLost;
        std::optional<uint64_t> roundTripTimeMeasurements;

#if USE(LIBWEBRTC)
        static RemoteInboundRtpStreamStats convert(const webrtc::RTCRemoteInboundRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static RemoteInboundRtpStreamStats convert(const GstStructure*);
#endif
    };

    struct SentRtpStreamStats : RtpStreamStats {
        std::optional<uint32_t> packetsSent;
        std::optional<uint64_t> bytesSent;

#if USE(LIBWEBRTC)
        static SentRtpStreamStats convert(Type, const webrtc::RTCSentRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static SentRtpStreamStats convert(Type, const GstStructure*);
#endif
    };

    enum class QualityLimitationReason {
        None,
        Cpu,
        Bandwidth,
        Other
    };

    struct OutboundRtpStreamStats : SentRtpStreamStats {
        String mid;
        String mediaSourceId;
        String remoteId;
        String rid;
        std::optional<uint64_t> headerBytesSent;
        std::optional<uint64_t> retransmittedPacketsSent;
        std::optional<uint64_t> retransmittedBytesSent;
        std::optional<uint32_t> rtxSsrc;
        std::optional<double> targetBitrate;
        std::optional<uint64_t> totalEncodedBytesTarget;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<double> framesPerSecond;
        std::optional<uint32_t> framesSent;
        std::optional<uint32_t> hugeFramesSent;
        std::optional<uint32_t> framesEncoded;
        std::optional<uint32_t> keyFramesEncoded;
        std::optional<uint64_t> qpSum;
        std::optional<double> totalEncodeTime;
        std::optional<double> totalPacketSendDelay;
        std::optional<QualityLimitationReason> qualityLimitationReason;
        std::optional<Vector<KeyValuePair<String, double>>> qualityLimitationDurations;
        std::optional<uint32_t> qualityLimitationResolutionChanges;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<bool> active;
        String scalabilityMode;

#if USE(LIBWEBRTC)
        static OutboundRtpStreamStats convert(const webrtc::RTCOutboundRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static OutboundRtpStreamStats convert(const GstStructure*);
#endif
    };

    struct RemoteOutboundRtpStreamStats : SentRtpStreamStats {
        String localId;
        std::optional<double> remoteTimestamp;
        std::optional<uint64_t> reportsSent;
        std::optional<double> roundTripTime;
        std::optional<double> totalRoundTripTime;
        std::optional<uint64_t> roundTripTimeMeasurements;

#if USE(LIBWEBRTC)
        static RemoteOutboundRtpStreamStats convert(const webrtc::RTCRemoteOutboundRtpStreamStats&);
#elif USE(GSTREAMER_WEBRTC)
        static RemoteOutboundRtpStreamStats convert(const GstStructure*);
#endif
    };

    struct DataChannelStats : Stats {
        String label;
        String protocol;
        std::optional<int> dataChannelIdentifier;
        String state;
        std::optional<uint32_t> messagesSent;
        std::optional<uint64_t> bytesSent;
        std::optional<uint32_t> messagesReceived;
        std::optional<uint64_t> bytesReceived;

#if USE(LIBWEBRTC)
        static DataChannelStats convert(const webrtc::RTCDataChannelStats&);
#elif USE(GSTREAMER_WEBRTC)
        static DataChannelStats convert(const GstStructure*);
#endif
    };

    enum class IceCandidatePairState {
        Frozen,
        Waiting,
        InProgress,
        Failed,
        Succeeded
    };

    struct IceCandidatePairStats : Stats {
        String transportId;
        String localCandidateId;
        String remoteCandidateId;
        IceCandidatePairState state;
        std::optional<bool> nominated;
        std::optional<uint64_t> packetsSent;
        std::optional<uint64_t> packetsReceived;
        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        std::optional<double> lastPacketSentTimestamp;
        std::optional<double> lastPacketReceivedTimestamp;
        std::optional<double> totalRoundTripTime;
        std::optional<double> currentRoundTripTime;
        std::optional<double> availableOutgoingBitrate;
        std::optional<double> availableIncomingBitrate;
        std::optional<uint64_t> requestsReceived;
        std::optional<uint64_t> requestsSent;
        std::optional<uint64_t> responsesReceived;
        std::optional<uint64_t> responsesSent;
        std::optional<uint64_t> consentRequestsSent;
        std::optional<uint32_t> packetsDiscardedOnSend;
        std::optional<uint64_t> bytesDiscardedOnSend;

#if USE(LIBWEBRTC)
        static IceCandidatePairStats convert(const webrtc::RTCIceCandidatePairStats&);
#elif USE(GSTREAMER_WEBRTC)
        static IceCandidatePairStats convert(const GstStructure*);
#endif
    };

    struct IceCandidateStats : Stats {
        String transportId;
        std::optional<String> address;
        std::optional<int32_t> port;
        String protocol;
        RTCIceCandidateType candidateType;
        std::optional<int32_t> priority;
        String url;
        std::optional<RTCIceServerTransportProtocol> relayProtocol;
        String foundation;
        String relatedAddress;
        std::optional<int32_t> relatedPort;
        String usernameFragment;
        std::optional<RTCIceTcpCandidateType> tcpType;

#if USE(LIBWEBRTC)
        static IceCandidateStats convert(const webrtc::RTCIceCandidateStats&);
#elif USE(GSTREAMER_WEBRTC)
        static IceCandidateStats convert(GstWebRTCStatsType, const GstStructure*);
#endif
    };

    struct CertificateStats : Stats {
        String fingerprint;
        String fingerprintAlgorithm;
        String base64Certificate;
        String issuerCertificateId;

#if USE(LIBWEBRTC)
        static CertificateStats convert(const webrtc::RTCCertificateStats&);
#elif USE(GSTREAMER_WEBRTC)
        static CertificateStats convert(const GstStructure*);
#endif
    };

    enum class CodecType {
        Encode,
        Decode
    };

    struct CodecStats : Stats {
        uint32_t payloadType { 0 };
        String transportId;
        String mimeType;
        std::optional<uint32_t> clockRate;
        std::optional<uint32_t> channels;
        String sdpFmtpLine;

#if USE(LIBWEBRTC)
        static CodecStats convert(const webrtc::RTCCodecStats&);
#elif USE(GSTREAMER_WEBRTC)
        static CodecStats convert(const GstStructure*);
#endif
    };

    enum class DtlsRole : uint8_t {
        Client,
        Server,
        Unknown
    };

    struct TransportStats : Stats {
        std::optional<uint64_t> packetsSent;
        std::optional<uint64_t> packetsReceived;
        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        std::optional<RTCIceRole> iceRole;
        String iceLocalUsernameFragment;
        RTCDtlsTransportState dtlsState;
        std::optional<RTCIceTransportState> iceState;
        String selectedCandidatePairId;
        String localCertificateId;
        String remoteCertificateId;
        String tlsVersion;
        String dtlsCipher;
        std::optional<DtlsRole> dtlsRole;
        String srtpCipher;
        std::optional<uint32_t> selectedCandidatePairChanges;

#if USE(LIBWEBRTC)
        static TransportStats convert(const webrtc::RTCTransportStats&);
#elif USE(GSTREAMER_WEBRTC)
        static TransportStats convert(const GstStructure*);
#endif
    };

    struct AudioPlayoutStats : Stats {
        String kind;
        std::optional<double> synthesizedSamplesDuration;
        std::optional<uint32_t> synthesizedSamplesEvents;
        std::optional<double> totalSamplesDuration;
        std::optional<double> totalPlayoutDelay;
        std::optional<uint64_t> totalSamplesCount;

#if USE(LIBWEBRTC)
        static AudioPlayoutStats convert(const webrtc::RTCAudioPlayoutStats&);
#elif USE(GSTREAMER_WEBRTC)
        static AudioPlayoutStats convert(const GstStructure*);
#endif
    };

    struct PeerConnectionStats : Stats {
        std::optional<uint32_t> dataChannelsOpened;
        std::optional<uint32_t> dataChannelsClosed;

#if USE(LIBWEBRTC)
        static PeerConnectionStats convert(const webrtc::RTCPeerConnectionStats&);
#elif USE(GSTREAMER_WEBRTC)
        static PeerConnectionStats convert(const GstStructure*);
#endif
    };

    struct MediaSourceStats : Stats {
        String trackIdentifier;
        String kind;

#if USE(LIBWEBRTC)
        static MediaSourceStats convert(Type, const webrtc::RTCMediaSourceStats&);
#elif USE(GSTREAMER_WEBRTC)
        static MediaSourceStats convert(Type, const GstStructure*);
#endif
    };

    struct AudioSourceStats : MediaSourceStats {
        std::optional<double> audioLevel;
        std::optional<double> totalAudioEnergy;
        std::optional<double> totalSamplesDuration;
        std::optional<double> echoReturnLoss;
        std::optional<double> echoReturnLossEnhancement;

#if USE(LIBWEBRTC)
        static AudioSourceStats convert(const webrtc::RTCAudioSourceStats&);
#elif USE(GSTREAMER_WEBRTC)
        static AudioSourceStats convert(const GstStructure*);
#endif
    };


    struct VideoSourceStats : MediaSourceStats {
        std::optional<unsigned long> width;
        std::optional<unsigned long> height;
        std::optional<unsigned long> frames;
        std::optional<double> framesPerSecond;

#if USE(LIBWEBRTC)
        static VideoSourceStats convert(const webrtc::RTCVideoSourceStats&);
#elif USE(GSTREAMER_WEBRTC)
        static VideoSourceStats convert(const GstStructure*);
#endif
    };

private:
    explicit RTCStatsReport(MapInitializer&&);

    MapInitializer m_mapInitializer;
};

inline RTCStatsReport::RTCStatsReport(MapInitializer&& mapInitializer)
    : m_mapInitializer(WTF::move(mapInitializer))
{
}

} // namespace WebCore
