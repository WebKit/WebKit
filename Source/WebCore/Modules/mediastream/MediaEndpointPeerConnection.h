/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "PeerConnectionBackend.h"
#include "RTCSessionDescription.h"
#include <wtf/Function.h>

namespace WebCore {

class MediaEndpointSessionDescription;
class SDPProcessor;

class MediaEndpointPeerConnection final : public PeerConnectionBackend, public MediaEndpointClient {
public:
    WEBCORE_EXPORT explicit MediaEndpointPeerConnection(RTCPeerConnection&);

private:
    RefPtr<RTCSessionDescription> localDescription() const final;
    RefPtr<RTCSessionDescription> currentLocalDescription() const final;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const final;

    RefPtr<RTCSessionDescription> remoteDescription() const final;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const final;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const final;

    bool setConfiguration(MediaEndpointConfiguration&&) final;

    void getStats(MediaStreamTrack*, Ref<DeferredPromise>&&) final;

    Vector<RefPtr<MediaStream>> getRemoteStreams() const final;

    Ref<RTCRtpReceiver> createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId) final;
    void replaceTrack(RTCRtpSender&, Ref<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&) final;

    void emulatePlatformEvent(const String& action) final;

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

    void replaceTrackTask(RTCRtpSender&, const String& mid, Ref<MediaStreamTrack>&&, DOMPromiseDeferred<void>&);

    bool localDescriptionTypeValidForState(RTCSdpType) const;
    bool remoteDescriptionTypeValidForState(RTCSdpType) const;

    MediaEndpointSessionDescription* internalLocalDescription() const;
    MediaEndpointSessionDescription* internalRemoteDescription() const;
    RefPtr<RTCSessionDescription> createRTCSessionDescription(MediaEndpointSessionDescription*) const;

    // MediaEndpointClient
    void gotDtlsFingerprint(const String& fingerprint, const String& fingerprintFunction) final;
    void gotIceCandidate(const String& mid, IceCandidate&&) final;
    void doneGatheringCandidates(const String& mid) final;
    void iceTransportStateChanged(const String& mid, RTCIceTransportState) final;

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
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
