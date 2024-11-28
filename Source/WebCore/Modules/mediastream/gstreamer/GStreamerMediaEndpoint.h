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

#pragma once

#if USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include "GStreamerDataChannelHandler.h"
#include "GStreamerPeerConnectionBackend.h"
#include "GStreamerRtpSenderBackend.h"
#include "GStreamerStatsCollector.h"
#include "GStreamerWebRTCCommon.h"
#include "GUniquePtrGStreamer.h"
#include "RTCRtpReceiver.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/LoggerHelper.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

namespace WebCore {

class GStreamerIncomingTrackProcessor;
class GStreamerRtpReceiverBackend;
class GStreamerRtpTransceiverBackend;
class MediaStreamTrack;
class RTCSessionDescription;
class RealtimeOutgoingMediaSourceGStreamer;
class UniqueSSRCGenerator;

class GStreamerMediaEndpoint : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerMediaEndpoint, WTF::DestructionThread::Main>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<GStreamerMediaEndpoint> create(GStreamerPeerConnectionBackend& peerConnection) { return adoptRef(*new GStreamerMediaEndpoint(peerConnection)); }
    ~GStreamerMediaEndpoint();

    bool setConfiguration(MediaEndpointConfiguration&);
    void restartIce();

    void doSetLocalDescription(const RTCSessionDescription*);
    void doSetRemoteDescription(const RTCSessionDescription&);
    void doCreateOffer(const RTCOfferOptions&);
    void doCreateAnswer();

    void getStats(const GRefPtr<GstPad>&, Ref<DeferredPromise>&&);
    void getStats(RTCRtpReceiver&, Ref<DeferredPromise>&&);

    std::unique_ptr<RTCDataChannelHandler> createDataChannel(const String&, const RTCDataChannelInit&);

    void addIceCandidate(GStreamerIceCandidate&, PeerConnectionBackend::AddIceCandidateCallback&&);

    void close();
    void stop();
    bool isStopped() const { return !m_pipeline; }

    void suspend();
    void resume();

    void gatherDecoderImplementationName(Function<void(String&&)>&&);
    bool isNegotiationNeeded(uint32_t eventId) const { return eventId == m_negotiationNeededEventId; }

    std::optional<bool> canTrickleIceCandidates() const;

    void configureSource(RealtimeOutgoingMediaSourceGStreamer&, GUniquePtr<GstStructure>&&);

    ExceptionOr<std::unique_ptr<GStreamerRtpSenderBackend>> addTrack(MediaStreamTrack&, const FixedVector<String>&);
    void removeTrack(GStreamerRtpSenderBackend&);

    struct Backends {
        std::unique_ptr<GStreamerRtpSenderBackend> senderBackend;
        std::unique_ptr<GStreamerRtpReceiverBackend> receiverBackend;
        std::unique_ptr<GStreamerRtpTransceiverBackend> transceiverBackend;
    };
    ExceptionOr<Backends> addTransceiver(const String& trackKind, const RTCRtpTransceiverInit&, PeerConnectionBackend::IgnoreNegotiationNeededFlag);
    ExceptionOr<Backends> addTransceiver(MediaStreamTrack&, const RTCRtpTransceiverInit&, PeerConnectionBackend::IgnoreNegotiationNeededFlag);
    std::unique_ptr<GStreamerRtpTransceiverBackend> transceiverBackendFromSender(GStreamerRtpSenderBackend&);

    GStreamerRtpSenderBackend::Source createSourceForTrack(MediaStreamTrack&);

    void collectTransceivers();

    void createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>&&);
    void createSessionDescriptionFailed(RTCSdpType, GUniquePtr<GError>&&);

    GstElement* pipeline() const { return m_pipeline.get(); }
    GstElement* webrtcBin() const { return m_webrtcBin.get(); }
    bool handleMessage(GstMessage*);

    GUniquePtr<GstStructure> preprocessStats(const GRefPtr<GstPad>&, const GstStructure*);
#if !RELEASE_LOG_DISABLED
    void processStatsItem(const GValue*);
#endif

    void connectIncomingTrack(WebRTCTrackData&);

    void startRTCLogs();
    void stopRTCLogs();

protected:
#if !RELEASE_LOG_DISABLED
    void onStatsDelivered(const GstStructure*);
#endif

private:
    GStreamerMediaEndpoint(GStreamerPeerConnectionBackend&);

    bool initializePipeline();
    void teardownPipeline();
    void disposeElementChain(GstElement*);

    enum class DescriptionType {
        Local,
        Remote
    };

    void setDescription(const RTCSessionDescription*, DescriptionType, Function<void(const GstSDPMessage&)>&& successCallback, Function<void(const GError*)>&& failureCallback);
    void initiate(bool isInitiator, GstStructure*);

    void onNegotiationNeeded();
    void onIceConnectionChange();
    void onIceGatheringChange();
    void onIceCandidate(guint sdpMLineIndex, gchararray candidate);

    void prepareDataChannel(GstWebRTCDataChannel*, gboolean isLocal);
    void onDataChannel(GstWebRTCDataChannel*);

    WARN_UNUSED_RETURN GstElement* requestAuxiliarySender();

    MediaStream& mediaStreamFromRTCStream(String mediaStreamId);

    void connectPad(GstPad*);
    void removeRemoteStream(GstPad*);

    int pickAvailablePayloadType();

    ExceptionOr<Backends> createTransceiverBackends(const String& kind, const RTCRtpTransceiverInit&, GStreamerRtpSenderBackend::Source&&, PeerConnectionBackend::IgnoreNegotiationNeededFlag);

    void processSDPMessage(const GstSDPMessage*, Function<void(unsigned index, StringView mid, const GstSDPMedia*)>);

    WARN_UNUSED_RETURN GRefPtr<GstPad> requestPad(const GRefPtr<GstCaps>&, const String& mediaStreamID);

    std::optional<bool> isIceGatheringComplete(const String& currentLocalDescription);

    void setTransceiverCodecPreferences(const GstSDPMedia&, guint transceiverIdx);

#if !RELEASE_LOG_DISABLED
    void gatherStatsForLogging();
    void startLoggingStats();
    void stopLoggingStats();

    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "GStreamerMediaEndpoint"_s; }
    WTFLogChannel& logChannel() const final;

    Seconds statsLogInterval(Seconds) const;
#endif

    String trackIdFromSDPMedia(const GstSDPMedia&);

    HashMap<String, RealtimeMediaSource::Type> m_mediaForMid;

    GStreamerPeerConnectionBackend& m_peerConnectionBackend;
    GRefPtr<GstElement> m_webrtcBin;
    GRefPtr<GstElement> m_pipeline;

    HashMap<String, RefPtr<MediaStream>> m_remoteStreamsById;

    Ref<GStreamerStatsCollector> m_statsCollector;

    uint32_t m_negotiationNeededEventId { 0 };

#if !RELEASE_LOG_DISABLED
    Timer m_statsLogTimer;
    Seconds m_statsFirstDeliveredTimestamp;
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    UniqueRef<GStreamerDataChannelHandler> findOrCreateIncomingChannelHandler(GRefPtr<GstWebRTCDataChannel>&&);

    using DataChannelHandlerIdentifier = ObjectIdentifier<GstWebRTCDataChannel>;
    HashMap<DataChannelHandlerIdentifier, UniqueRef<GStreamerDataChannelHandler>> m_incomingDataChannels;

    RefPtr<UniqueSSRCGenerator> m_ssrcGenerator;

    using SSRC = unsigned;
    HashMap<SSRC, RefPtr<GStreamerIncomingTrackProcessor>> m_trackProcessors;

    Vector<String> m_pendingIncomingMediaStreamIDs;

    bool m_shouldIgnoreNegotiationNeededSignal { false };

    Vector<RefPtr<MediaStreamTrackPrivate>> m_pendingIncomingTracks;

    Vector<RefPtr<RealtimeOutgoingMediaSourceGStreamer>> m_unlinkedOutgoingSources;

    bool m_isGatheringRTCLogs { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
