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

#include "GStreamerRtpSenderBackend.h"
#include "PeerConnectionBackend.h"
#include "RealtimeMediaSource.h"

#include <gst/gst.h>
#include <wtf/HashMap.h>

namespace WebCore {

class GStreamerMediaEndpoint;
class GStreamerRtpReceiverBackend;
class GStreamerRtpTransceiverBackend;
class RTCRtpReceiver;
class RTCRtpReceiverBackend;
class RTCSessionDescription;
class RTCStatsReport;
class RealtimeIncomingAudioSourceGStreamer;
class RealtimeIncomingVideoSourceGStreamer;
class RealtimeMediaSourceGStreamer;
class RealtimeOutgoingAudioSourceGStreamer;
class RealtimeOutgoingVideoSourceGStreamer;

struct GStreamerIceCandidate {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    unsigned sdpMLineIndex;
    String candidate;
};

class GStreamerPeerConnectionBackend final : public PeerConnectionBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GStreamerPeerConnectionBackend(RTCPeerConnection&);
    ~GStreamerPeerConnectionBackend();

private:
    void close() final;
    void doCreateOffer(RTCOfferOptions&&) final;
    void doCreateAnswer(RTCAnswerOptions&&) final;
    void doSetLocalDescription(const RTCSessionDescription*) final;
    void doSetRemoteDescription(const RTCSessionDescription&) final;
    void doAddIceCandidate(RTCIceCandidate&, AddIceCandidateCallback&&) final;
    void doStop() final;
    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;
    void restartIce() final;
    bool setConfiguration(MediaEndpointConfiguration&&) final;
    void getStats(Ref<DeferredPromise>&&) final;
    void getStats(RTCRtpSender&, Ref<DeferredPromise>&&) final;
    void getStats(RTCRtpReceiver&, Ref<DeferredPromise>&&) final;

    void emulatePlatformEvent(const String&) final { }
    void applyRotationForOutgoingVideoSources() final;

    void gatherDecoderImplementationName(Function<void(String&&)>&&) final;

    bool isNegotiationNeeded(uint32_t) const final;

    std::optional<bool> canTrickleIceCandidates() const { return true; };

    friend class GStreamerMediaEndpoint;
    friend class GStreamerRtpSenderBackend;
    RTCPeerConnection& connection() { return m_peerConnection; }

    void getStatsSucceeded(const DeferredPromise&, Ref<RTCStatsReport>&&);

    ExceptionOr<Ref<RTCRtpSender>> addTrack(MediaStreamTrack&, FixedVector<String>&&) final;
    void removeTrack(RTCRtpSender&) final;

    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(const String&, const RTCRtpTransceiverInit&) final;
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&) final;

    GStreamerRtpSenderBackend::Source createLinkedSourceForTrack(MediaStreamTrack&);

    RTCRtpTransceiver* existingTransceiver(WTF::Function<bool(GStreamerRtpTransceiverBackend&)>&&);
    RTCRtpTransceiver& newRemoteTransceiver(std::unique_ptr<GStreamerRtpTransceiverBackend>&&, RealtimeMediaSource::Type);

    void collectTransceivers() final;

    void addPendingTrackEvent(PendingTrackEvent&&);
    void dispatchPendingTrackEvents(MediaStream&);

    bool isLocalDescriptionSet() const final { return m_isLocalDescriptionSet; }

    template<typename T>
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiverFromTrackOrKind(T&& trackOrKind, const RTCRtpTransceiverInit&);

    Ref<RTCRtpReceiver> createReceiver(std::unique_ptr<GStreamerRtpReceiverBackend>&&, const String& trackKind, const String& trackId);

    void suspend() final;
    void resume() final;

    void setReconfiguring(bool isReconfiguring) { m_isReconfiguring = isReconfiguring; }
    bool isReconfiguring() const { return m_isReconfiguring; }

    Ref<GStreamerMediaEndpoint> m_endpoint;
    bool m_isLocalDescriptionSet { false };
    bool m_isRemoteDescriptionSet { false };

    Vector<std::unique_ptr<GStreamerIceCandidate>> m_pendingCandidates;
    Vector<Ref<RTCRtpReceiver>> m_pendingReceivers;
    Vector<PendingTrackEvent> m_pendingTrackEvents;

    bool m_isReconfiguring { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
