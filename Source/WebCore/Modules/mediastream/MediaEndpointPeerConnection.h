/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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

#ifndef MediaEndpointPeerConnection_h
#define MediaEndpointPeerConnection_h

#if ENABLE(WEB_RTC)

#include "MediaEndpoint.h"
#include "NotImplemented.h"
#include "PeerConnectionBackend.h"
#include "RTCSessionDescription.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaStreamTrack;
class SDPProcessor;

typedef Vector<RefPtr<RTCRtpSender>> RtpSenderVector;

class MediaEndpointPeerConnection : public PeerConnectionBackend, public MediaEndpointClient {
public:
    MediaEndpointPeerConnection(PeerConnectionBackendClient*);

    void createOffer(RTCOfferOptions&, PeerConnection::SessionDescriptionPromise&&) override;
    void createAnswer(RTCAnswerOptions&, PeerConnection::SessionDescriptionPromise&&) override;

    void setLocalDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&) override;
    RefPtr<RTCSessionDescription> localDescription() const override;
    RefPtr<RTCSessionDescription> currentLocalDescription() const override;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const override;

    void setRemoteDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&) override;
    RefPtr<RTCSessionDescription> remoteDescription() const override;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const override;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const override;

    void setConfiguration(RTCConfiguration&) override;
    void addIceCandidate(RTCIceCandidate&, PeerConnection::VoidPromise&&) override;

    void getStats(MediaStreamTrack*, PeerConnection::StatsPromise&&) override;

    void replaceTrack(RTCRtpSender&, MediaStreamTrack&, PeerConnection::VoidPromise&&) override;

    void stop() override;

    bool isNegotiationNeeded() const override { return false; };
    void markAsNeedingNegotiation() override;
    void clearNegotiationNeededState() override { notImplemented(); };

private:
    void runTask(std::function<void()>);
    void startRunningTasks();

    void createOfferTask(RTCOfferOptions&, PeerConnection::SessionDescriptionPromise&);

    // MediaEndpointClient
    void gotDtlsFingerprint(const String& fingerprint, const String& fingerprintFunction) override;
    void gotIceCandidate(unsigned mdescIndex, RefPtr<IceCandidate>&&) override;
    void doneGatheringCandidates(unsigned mdescIndex) override;
    void gotRemoteSource(unsigned mdescIndex, RefPtr<RealtimeMediaSource>&&) override;

    PeerConnectionBackendClient* m_client;
    std::unique_ptr<MediaEndpoint> m_mediaEndpoint;

    std::function<void()> m_initialDeferredTask;

    std::unique_ptr<SDPProcessor> m_sdpProcessor;

    Vector<RefPtr<MediaPayload>> m_defaultAudioPayloads;
    Vector<RefPtr<MediaPayload>> m_defaultVideoPayloads;

    String m_cname;
    String m_iceUfrag;
    String m_icePassword;
    String m_dtlsFingerprint;
    String m_dtlsFingerprintFunction;
    unsigned m_sdpSessionVersion { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

#endif // MediaEndpointPeerConnection_h
