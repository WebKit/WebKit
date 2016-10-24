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
#include "Dictionary.h"
#include "EventTarget.h"
#include "MediaStream.h"
#include "PeerConnectionBackend.h"
#include "RTCRtpTransceiver.h"
#include "ScriptWrappable.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaStreamTrack;
class PeerConnectionBackend;
class RTCConfiguration;
class RTCDataChannel;
class RTCIceCandidate;
class RTCPeerConnectionErrorCallback;
class RTCSessionDescription;
class RTCStatsCallback;

class RTCPeerConnection final : public RefCounted<RTCPeerConnection>, public PeerConnectionBackendClient, public RTCRtpSenderClient, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<RTCPeerConnection> create(ScriptExecutionContext&);
    ~RTCPeerConnection();

    void initializeWith(Document&, const Dictionary&, ExceptionCode&);

    const Vector<RefPtr<RTCRtpSender>>& getSenders() const { return m_transceiverSet->getSenders(); }
    const Vector<RefPtr<RTCRtpReceiver>>& getReceivers() const { return m_transceiverSet->getReceivers(); }
    const Vector<RefPtr<RTCRtpTransceiver>>& getTransceivers() const final { return m_transceiverSet->list(); }

    // Part of legacy MediaStream-based API (mostly implemented as JS built-ins)
    Vector<RefPtr<MediaStream>> getRemoteStreams() const { return m_backend->getRemoteStreams(); }

    RefPtr<RTCRtpSender> addTrack(Ref<MediaStreamTrack>&&, const Vector<std::reference_wrapper<MediaStream>>&, ExceptionCode&);
    void removeTrack(RTCRtpSender&, ExceptionCode&);

    // This enum is mirrored in RTCRtpTransceiver.h
    enum class RtpTransceiverDirection { Sendrecv, Sendonly, Recvonly, Inactive };

    struct RtpTransceiverInit {
        RtpTransceiverDirection direction;
    };

    RefPtr<RTCRtpTransceiver> addTransceiver(Ref<MediaStreamTrack>&&, const RtpTransceiverInit&, ExceptionCode&);
    RefPtr<RTCRtpTransceiver> addTransceiver(const String& kind, const RtpTransceiverInit&, ExceptionCode&);

    void queuedCreateOffer(const Dictionary& offerOptions, PeerConnection::SessionDescriptionPromise&&);
    void queuedCreateAnswer(const Dictionary& answerOptions, PeerConnection::SessionDescriptionPromise&&);

    void queuedSetLocalDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&);
    RefPtr<RTCSessionDescription> localDescription() const;
    RefPtr<RTCSessionDescription> currentLocalDescription() const;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const;

    void queuedSetRemoteDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&);
    RefPtr<RTCSessionDescription> remoteDescription() const;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const;

    String signalingState() const;

    void queuedAddIceCandidate(RTCIceCandidate&, PeerConnection::VoidPromise&&);

    String iceGatheringState() const;
    String iceConnectionState() const;

    RTCConfiguration* getConfiguration() const;
    void setConfiguration(const Dictionary& configuration, ExceptionCode&);

    void privateGetStats(MediaStreamTrack*, PeerConnection::StatsPromise&&);

    RefPtr<RTCDataChannel> createDataChannel(String label, const Dictionary& dataChannelDict, ExceptionCode&);

    void close();

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return RTCPeerConnectionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<RTCPeerConnection>::ref;
    using RefCounted<RTCPeerConnection>::deref;

    // Used for testing with a mock
    WEBCORE_EXPORT void emulatePlatformEvent(const String& action);

private:
    RTCPeerConnection(ScriptExecutionContext&);

    RefPtr<RTCRtpTransceiver> completeAddTransceiver(Ref<RTCRtpTransceiver>&&, const RtpTransceiverInit&);

    // EventTarget implementation.
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // PeerConnectionBackendClient
    void addTransceiver(RefPtr<RTCRtpTransceiver>&&) final;
    void setSignalingState(PeerConnectionStates::SignalingState) final;
    void updateIceGatheringState(PeerConnectionStates::IceGatheringState) final;
    void updateIceConnectionState(PeerConnectionStates::IceConnectionState) final;

    void scheduleNegotiationNeededEvent() final;

    RTCRtpSenderClient& senderClient() final { return *this; }
    void fireEvent(Event&) final;
    PeerConnectionStates::SignalingState internalSignalingState() const final { return m_signalingState; }
    PeerConnectionStates::IceGatheringState internalIceGatheringState() const final { return m_iceGatheringState; }
    PeerConnectionStates::IceConnectionState internalIceConnectionState() const final { return m_iceConnectionState; }

    // RTCRtpSenderClient
    void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, PeerConnection::VoidPromise&&) final;

    PeerConnectionStates::SignalingState m_signalingState { PeerConnectionStates::SignalingState::Stable };
    PeerConnectionStates::IceGatheringState m_iceGatheringState { PeerConnectionStates::IceGatheringState::New };
    PeerConnectionStates::IceConnectionState m_iceConnectionState { PeerConnectionStates::IceConnectionState::New };

    std::unique_ptr<RtpTransceiverSet> m_transceiverSet { std::unique_ptr<RtpTransceiverSet>(new RtpTransceiverSet()) };

    Vector<RefPtr<RTCDataChannel>> m_dataChannels;

    std::unique_ptr<PeerConnectionBackend> m_backend;

    RefPtr<RTCConfiguration> m_configuration;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
