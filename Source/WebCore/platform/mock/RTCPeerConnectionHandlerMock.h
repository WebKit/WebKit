/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef RTCPeerConnectionHandlerMock_h
#define RTCPeerConnectionHandlerMock_h

#if ENABLE(MEDIA_STREAM)

#include "RTCPeerConnectionHandler.h"
#include "RTCPeerConnectionHandlerClient.h"
#include "RTCSessionDescriptionDescriptor.h"
#include "TimerEventBasedMock.h"

namespace WebCore {

class RTCPeerConnectionHandlerMock final : public RTCPeerConnectionHandler, public TimerEventBasedMock {
public:
    static std::unique_ptr<RTCPeerConnectionHandler> create(RTCPeerConnectionHandlerClient*);

    virtual ~RTCPeerConnectionHandlerMock() { }

    virtual bool initialize(PassRefPtr<RTCConfiguration>) override;

    virtual void createOffer(PassRefPtr<RTCSessionDescriptionRequest>, PassRefPtr<MediaConstraints>) override;
    virtual void createAnswer(PassRefPtr<RTCSessionDescriptionRequest>, PassRefPtr<MediaConstraints>) override;
    virtual void setLocalDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>) override;
    virtual void setRemoteDescription(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCSessionDescriptionDescriptor>) override;
    virtual PassRefPtr<RTCSessionDescriptionDescriptor> localDescription() override;
    virtual PassRefPtr<RTCSessionDescriptionDescriptor> remoteDescription() override;
    virtual bool updateIce(PassRefPtr<RTCConfiguration>) override;
    virtual bool addIceCandidate(PassRefPtr<RTCVoidRequest>, PassRefPtr<RTCIceCandidateDescriptor>) override;
    virtual bool addStream(PassRefPtr<MediaStreamPrivate>) override;
    virtual void removeStream(PassRefPtr<MediaStreamPrivate>) override;
    virtual void getStats(PassRefPtr<RTCStatsRequest>) override;
    virtual std::unique_ptr<RTCDataChannelHandler> createDataChannel(const String& label, const RTCDataChannelInit&) override;
    virtual std::unique_ptr<RTCDTMFSenderHandler> createDTMFSender(PassRefPtr<MediaStreamSource>) override;
    virtual void stop() override;

    explicit RTCPeerConnectionHandlerMock(RTCPeerConnectionHandlerClient*);

private:
    RTCPeerConnectionHandlerClient::SignalingState signalingStateFromSDP(RTCSessionDescriptionDescriptor*);
    RTCPeerConnectionHandlerClient* m_client;

    RefPtr<RTCSessionDescriptionDescriptor> m_localSessionDescription;
    RefPtr<RTCSessionDescriptionDescriptor> m_remoteSessionDescription;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCPeerConnectionHandlerMock_h
