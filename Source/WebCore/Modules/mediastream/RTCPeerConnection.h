/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaStream.h"
#include "RTCConfiguration.h"
#include "RTCDataChannel.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCRtpTransceiver.h"

namespace WebCore {

class MediaStreamTrack;
class PeerConnectionBackend;
class RTCIceCandidate;
class RTCPeerConnectionErrorCallback;
class RTCSessionDescription;
class RTCStatsCallback;

class RTCPeerConnection final : public RefCounted<RTCPeerConnection>, public RTCRtpSenderClient, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<RTCPeerConnection> create(ScriptExecutionContext&);
    virtual ~RTCPeerConnection();

    using AnswerOptions = RTCAnswerOptions;
    using DataChannelInit = RTCDataChannelInit;
    using OfferAnswerOptions = RTCOfferAnswerOptions;
    using OfferOptions = RTCOfferOptions;

    ExceptionOr<void> initializeWith(Document&, RTCConfiguration&&);

    const Vector<std::reference_wrapper<RTCRtpSender>>& getSenders() const { return m_transceiverSet->senders(); }
    const Vector<std::reference_wrapper<RTCRtpReceiver>>& getReceivers() const { return m_transceiverSet->receivers(); }
    const Vector<RefPtr<RTCRtpTransceiver>>& getTransceivers() const { return m_transceiverSet->list(); }

    // Part of legacy MediaStream-based API (mostly implemented as JS built-ins)
    Vector<RefPtr<MediaStream>> getRemoteStreams() const { return m_backend->getRemoteStreams(); }

    ExceptionOr<Ref<RTCRtpSender>> addTrack(Ref<MediaStreamTrack>&&, const Vector<std::reference_wrapper<MediaStream>>&);
    ExceptionOr<void> removeTrack(RTCRtpSender&);

    // This enum is mirrored in RTCRtpTransceiver.h
    enum class RtpTransceiverDirection { Sendrecv, Sendonly, Recvonly, Inactive };

    struct RtpTransceiverInit {
        RtpTransceiverDirection direction;
    };

    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(Ref<MediaStreamTrack>&&, const RtpTransceiverInit&);
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(const String& kind, const RtpTransceiverInit&);

    void queuedCreateOffer(RTCOfferOptions&&, PeerConnection::SessionDescriptionPromise&&);
    void queuedCreateAnswer(RTCAnswerOptions&&, PeerConnection::SessionDescriptionPromise&&);

    void queuedSetLocalDescription(RTCSessionDescription&, DOMPromise<void>&&);
    RefPtr<RTCSessionDescription> localDescription() const;
    RefPtr<RTCSessionDescription> currentLocalDescription() const;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const;

    void queuedSetRemoteDescription(RTCSessionDescription&, DOMPromise<void>&&);
    RefPtr<RTCSessionDescription> remoteDescription() const;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const;

    String signalingState() const;

    void queuedAddIceCandidate(RTCIceCandidate&, DOMPromise<void>&&);

    String iceGatheringState() const;
    String iceConnectionState() const;

    const RTCConfiguration& getConfiguration() const { return m_configuration; }
    ExceptionOr<void> setConfiguration(RTCConfiguration&&);

    void getStats(MediaStreamTrack*, Ref<DeferredPromise>&&);

    ExceptionOr<Ref<RTCDataChannel>> createDataChannel(ScriptExecutionContext&, String&&, RTCDataChannelInit&&);

    void close();

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return RTCPeerConnectionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

    // Used for testing with a mock
    WEBCORE_EXPORT void emulatePlatformEvent(const String& action);

    // API used by PeerConnectionBackend and relatives
    void addTransceiver(Ref<RTCRtpTransceiver>&&);
    void setSignalingState(PeerConnectionStates::SignalingState);
    void updateIceGatheringState(PeerConnectionStates::IceGatheringState);
    void updateIceConnectionState(PeerConnectionStates::IceConnectionState);

    void scheduleNegotiationNeededEvent();

    RTCRtpSenderClient& senderClient() { return *this; }
    void fireEvent(Event&);
    PeerConnectionStates::SignalingState internalSignalingState() const { return m_signalingState; }
    PeerConnectionStates::IceGatheringState internalIceGatheringState() const { return m_iceGatheringState; }
    PeerConnectionStates::IceConnectionState internalIceConnectionState() const { return m_iceConnectionState; }

private:
    RTCPeerConnection(ScriptExecutionContext&);

    void completeAddTransceiver(RTCRtpTransceiver&, const RtpTransceiverInit&);

    // EventTarget implementation.
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // RTCRtpSenderClient
    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, DOMPromise<void>&&) final;

    PeerConnectionStates::SignalingState m_signalingState { PeerConnectionStates::SignalingState::Stable };
    PeerConnectionStates::IceGatheringState m_iceGatheringState { PeerConnectionStates::IceGatheringState::New };
    PeerConnectionStates::IceConnectionState m_iceConnectionState { PeerConnectionStates::IceConnectionState::New };

    std::unique_ptr<RtpTransceiverSet> m_transceiverSet { std::unique_ptr<RtpTransceiverSet>(new RtpTransceiverSet()) };

    Vector<RefPtr<RTCDataChannel>> m_dataChannels;

    std::unique_ptr<PeerConnectionBackend> m_backend;

    RTCConfiguration m_configuration;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
