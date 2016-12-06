/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "MediaEndpoint.h"
#include "MediaEndpointSessionDescription.h"
#include "PeerConnectionBackend.h"
#include <wtf/Function.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaStream;
class MediaStreamTrack;
class SDPProcessor;

struct PeerMediaDescription;

using MediaDescriptionVector = Vector<PeerMediaDescription>;
using RtpSenderVector = Vector<RefPtr<RTCRtpSender>>;
using RtpTransceiverVector = Vector<RefPtr<RTCRtpTransceiver>>;

class MediaEndpointPeerConnection final : public PeerConnectionBackend, public MediaEndpointClient {
public:
    MediaEndpointPeerConnection(RTCPeerConnection&);

    RefPtr<RTCSessionDescription> localDescription() const override;
    RefPtr<RTCSessionDescription> currentLocalDescription() const override;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const override;

    RefPtr<RTCSessionDescription> remoteDescription() const override;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const override;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const override;

    void setConfiguration(RTCConfiguration&) override;

    void getStats(MediaStreamTrack*, PeerConnection::StatsPromise&&) override;

    Vector<RefPtr<MediaStream>> getRemoteStreams() const override;

    Ref<RTCRtpReceiver> createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId) override;
    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromise<void>&&) override;

    bool isNegotiationNeeded() const override { return m_negotiationNeeded; };
    void markAsNeedingNegotiation() override;
    void clearNegotiationNeededState() override { m_negotiationNeeded = false; };

    void emulatePlatformEvent(const String& action) override;

private:
    void runTask(Function<void ()>&&);
    void startRunningTasks();

    void doCreateOffer(RTCOfferOptions&&) final;
    void doCreateAnswer(RTCAnswerOptions&&) final;
    void doSetLocalDescription(RTCSessionDescription&) final;
    void doSetRemoteDescription(RTCSessionDescription&) final;
    void doAddIceCandidate(RTCIceCandidate&) final;
    void doStop() final;

    void createOfferTask(const RTCOfferOptions&);
    void createAnswerTask(const RTCAnswerOptions&);

    void setLocalDescriptionTask(RefPtr<RTCSessionDescription>&&);
    void setRemoteDescriptionTask(RefPtr<RTCSessionDescription>&&);

    void addIceCandidateTask(RTCIceCandidate&);

    void replaceTrackTask(RTCRtpSender&, const String& mid, RefPtr<MediaStreamTrack>&&, DOMPromise<void>&);

    bool localDescriptionTypeValidForState(RTCSessionDescription::SdpType) const;
    bool remoteDescriptionTypeValidForState(RTCSessionDescription::SdpType) const;

    MediaEndpointSessionDescription* internalLocalDescription() const;
    MediaEndpointSessionDescription* internalRemoteDescription() const;
    RefPtr<RTCSessionDescription> createRTCSessionDescription(MediaEndpointSessionDescription*) const;

    // MediaEndpointClient
    void gotDtlsFingerprint(const String& fingerprint, const String& fingerprintFunction) override;
    void gotIceCandidate(const String& mid, IceCandidate&&) override;
    void doneGatheringCandidates(const String& mid) override;
    void iceTransportStateChanged(const String& mid, MediaEndpoint::IceTransportState) override;

    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;

    std::unique_ptr<MediaEndpoint> m_mediaEndpoint;

    Function<void ()> m_initialDeferredTask;

    std::unique_ptr<SDPProcessor> m_sdpProcessor;

    Vector<MediaPayload> m_defaultAudioPayloads;
    Vector<MediaPayload> m_defaultVideoPayloads;

    String m_cname;
    String m_iceUfrag;
    String m_icePassword;
    String m_dtlsFingerprint;
    String m_dtlsFingerprintFunction;
    unsigned m_sdpOfferSessionVersion { 0 };
    unsigned m_sdpAnswerSessionVersion { 0 };

    RefPtr<MediaEndpointSessionDescription> m_currentLocalDescription;
    RefPtr<MediaEndpointSessionDescription> m_pendingLocalDescription;

    RefPtr<MediaEndpointSessionDescription> m_currentRemoteDescription;
    RefPtr<MediaEndpointSessionDescription> m_pendingRemoteDescription;

    HashMap<String, RefPtr<MediaStream>> m_remoteStreamMap;

    bool m_negotiationNeeded { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
