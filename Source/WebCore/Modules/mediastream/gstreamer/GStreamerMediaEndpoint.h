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
#include "GUniquePtrGStreamer.h"
#include "RTCRtpReceiver.h"

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/LoggerHelper.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

class GStreamerRtpReceiverBackend;
class GStreamerRtpTransceiverBackend;
class MediaStreamTrack;
class RTCSessionDescription;
class RealtimeOutgoingMediaSourceGStreamer;

class GStreamerMediaEndpoint : public ThreadSafeRefCounted<GStreamerMediaEndpoint, WTF::DestructionThread::Main>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<GStreamerMediaEndpoint> create(GStreamerPeerConnectionBackend& peerConnection) { return adoptRef(*new GStreamerMediaEndpoint(peerConnection)); }
    ~GStreamerMediaEndpoint() = default;

    bool setConfiguration(MediaEndpointConfiguration&);
    void restartIce();

    void doSetLocalDescription(const RTCSessionDescription*);
    void doSetRemoteDescription(const RTCSessionDescription&);
    void doCreateOffer(const RTCOfferOptions&);
    void doCreateAnswer();

    void getStats(GstPad*, const GstStructure*, Ref<DeferredPromise>&&);

    std::unique_ptr<RTCDataChannelHandler> createDataChannel(const String&, const RTCDataChannelInit&);

    void addIceCandidate(GStreamerIceCandidate&, PeerConnectionBackend::AddIceCandidateCallback&&);

    void close();
    void stop();
    bool isStopped() const { return !m_pipeline; }

    void suspend();
    void resume();

    void gatherDecoderImplementationName(Function<void(String&&)>&&);
    bool isNegotiationNeeded() const { return m_isNegotiationNeeded; }

    void configureAndLinkSource(RealtimeOutgoingMediaSourceGStreamer&);

    bool addTrack(GStreamerRtpSenderBackend&, MediaStreamTrack&, const FixedVector<String>&);
    void removeTrack(GStreamerRtpSenderBackend&);

    struct Backends {
        std::unique_ptr<GStreamerRtpSenderBackend> senderBackend;
        std::unique_ptr<GStreamerRtpReceiverBackend> receiverBackend;
        std::unique_ptr<GStreamerRtpTransceiverBackend> transceiverBackend;
    };
    std::optional<Backends> addTransceiver(const String& trackKind, const RTCRtpTransceiverInit&);
    std::optional<Backends> addTransceiver(MediaStreamTrack&, const RTCRtpTransceiverInit&);
    std::unique_ptr<GStreamerRtpTransceiverBackend> transceiverBackendFromSender(GStreamerRtpSenderBackend&);

    void setSenderSourceFromTrack(GStreamerRtpSenderBackend&, MediaStreamTrack&);

    void collectTransceivers();

    void createSessionDescriptionSucceeded(GUniquePtr<GstWebRTCSessionDescription>&&);
    void createSessionDescriptionFailed(GUniquePtr<GError>&&);

    GstElement* pipeline() const { return m_pipeline.get(); }
    bool handleMessage(GstMessage*);

#if !RELEASE_LOG_DISABLED
    void processStats(const GValue*);
#endif

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

    void setDescription(const RTCSessionDescription*, DescriptionType, Function<void(const GstSDPMessage&)>&& preProcessCallback, Function<void(const GstSDPMessage&)>&& successCallback, Function<void(const GError*)>&& failureCallback);
    void initiate(bool isInitiator, GstStructure*);

    void onNegotiationNeeded();
    void onIceConnectionChange();
    void onIceGatheringChange();
    void onIceCandidate(guint sdpMLineIndex, gchararray candidate);

    void prepareDataChannel(GstWebRTCDataChannel*, gboolean isLocal);
    void onDataChannel(GstWebRTCDataChannel*);

    MediaStream& mediaStreamFromRTCStream(String mediaStreamId);

    void addRemoteStream(GstPad*);
    void removeRemoteStream(GstPad*);

    std::optional<Backends> createTransceiverBackends(const String& kind, const RTCRtpTransceiverInit&, GStreamerRtpSenderBackend::Source&&);
    GStreamerRtpSenderBackend::Source createSourceForTrack(MediaStreamTrack&);

    void processSDPMessage(const GstSDPMessage*, Function<void(unsigned index, const char* mid, const GstSDPMedia*)>);

    GRefPtr<GstPad> requestPad(unsigned mlineIndex, const GRefPtr<GstCaps>&);

#if !RELEASE_LOG_DISABLED
    void gatherStatsForLogging();
    void startLoggingStats();
    void stopLoggingStats();

    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "GStreamerMediaEndpoint"; }
    WTFLogChannel& logChannel() const final;

    Seconds statsLogInterval(Seconds) const;
#endif

    HashMap<String, RealtimeMediaSource::Type> m_mediaForMid;
    RealtimeMediaSource::Type mediaTypeForMid(const char* mid);

    GStreamerPeerConnectionBackend& m_peerConnectionBackend;
    GRefPtr<GstElement> m_webrtcBin;
    GRefPtr<GstElement> m_pipeline;

    HashMap<String, RefPtr<MediaStream>> m_remoteStreamsById;

    Ref<GStreamerStatsCollector> m_statsCollector;

    unsigned m_requestPadCounter { 0 };
    int m_ptCounter { 96 };
    unsigned m_pendingIncomingStreams { 0 };
    bool m_isInitiator { false };
    bool m_isNegotiationNeeded { false };

#if !RELEASE_LOG_DISABLED
    Timer m_statsLogTimer;
    Seconds m_statsFirstDeliveredTimestamp;
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    UniqueRef<GStreamerDataChannelHandler> findOrCreateIncomingChannelHandler(GRefPtr<GstWebRTCDataChannel>&&);

    using DataChannelHandlerIdentifier = ObjectIdentifier<GstWebRTCDataChannel>;
    HashMap<DataChannelHandlerIdentifier, UniqueRef<GStreamerDataChannelHandler>> m_incomingDataChannels;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
