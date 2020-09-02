/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
#include "MediaEndpointConfiguration.h"
#include "MediaStream.h"
#include "PeerConnectionBackend.h"
#include "RTCConfiguration.h"
#include "RTCDataChannel.h"
#include "RTCIceConnectionState.h"
#include "RTCIceGatheringState.h"
#include "RTCPeerConnectionState.h"
#include "RTCRtpTransceiver.h"
#include "RTCSignalingState.h"
#include <JavaScriptCore/Uint8Array.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMPromiseDeferredBase;
class MediaStream;
class MediaStreamTrack;
class RTCController;
class RTCIceCandidate;
class RTCPeerConnectionErrorCallback;
class RTCSessionDescription;
class RTCStatsCallback;

struct RTCAnswerOptions;
struct RTCOfferOptions;
struct RTCRtpParameters;
struct RTCIceCandidateInit;
struct RTCSessionDescriptionInit;

struct RTCRtpTransceiverInit {
    RTCRtpTransceiverDirection direction { RTCRtpTransceiverDirection::Sendrecv };
    Vector<RefPtr<MediaStream>> streams;
};

class RTCPeerConnection final
    : public RefCounted<RTCPeerConnection>
    , public EventTargetWithInlineData
    , public ActiveDOMObject
    , public CanMakeWeakPtr<RTCPeerConnection>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(RTCPeerConnection);
public:
    static ExceptionOr<Ref<RTCPeerConnection>> create(Document&, RTCConfiguration&&);
    virtual ~RTCPeerConnection();

    using DataChannelInit = RTCDataChannelInit;

    struct CertificateParameters {
        String name;
        String hash;
        String namedCurve;
        Optional<uint32_t> modulusLength;
        RefPtr<Uint8Array> publicExponent;
        Optional<double> expires;
    };

    using AlgorithmIdentifier = Variant<JSC::Strong<JSC::JSObject>, String>;
    static void generateCertificate(JSC::JSGlobalObject&, AlgorithmIdentifier&&, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&&);

    // 4.3.2 RTCPeerConnection Interface
    void createOffer(RTCOfferOptions&&, Ref<DeferredPromise>&&);
    void createAnswer(RTCAnswerOptions&&, Ref<DeferredPromise>&&);

    using Description = Variant<RTCSessionDescriptionInit, RefPtr<RTCSessionDescription>>;
    void setLocalDescription(Optional<Description>&&, Ref<DeferredPromise>&&);
    RefPtr<RTCSessionDescription> localDescription() const;
    RefPtr<RTCSessionDescription> currentLocalDescription() const;
    RefPtr<RTCSessionDescription> pendingLocalDescription() const;

    void setRemoteDescription(Description&&, Ref<DeferredPromise>&&);
    RefPtr<RTCSessionDescription> remoteDescription() const;
    RefPtr<RTCSessionDescription> currentRemoteDescription() const;
    RefPtr<RTCSessionDescription> pendingRemoteDescription() const;

    using Candidate = Optional<Variant<RTCIceCandidateInit, RefPtr<RTCIceCandidate>>>;
    void addIceCandidate(Candidate&&, Ref<DeferredPromise>&&);

    RTCSignalingState signalingState() const { return m_signalingState; }
    RTCIceGatheringState iceGatheringState() const { return m_iceGatheringState; }
    RTCIceConnectionState iceConnectionState() const { return m_iceConnectionState; }
    RTCPeerConnectionState connectionState() const { return m_connectionState; }

    const RTCConfiguration& getConfiguration() const { return m_configuration; }
    ExceptionOr<void> setConfiguration(RTCConfiguration&&);
    void close();

    bool isClosed() const { return m_connectionState == RTCPeerConnectionState::Closed; }
    bool isStopped() const { return m_isStopped; }

    void addInternalTransceiver(Ref<RTCRtpTransceiver>&& transceiver) { m_transceiverSet->append(WTFMove(transceiver)); }

    // 5.1 RTCPeerConnection extensions
    Vector<std::reference_wrapper<RTCRtpSender>> getSenders() const;
    Vector<std::reference_wrapper<RTCRtpReceiver>> getReceivers() const;
    const Vector<RefPtr<RTCRtpTransceiver>>& getTransceivers() const;

    const Vector<RefPtr<RTCRtpTransceiver>>& currentTransceivers() const { return m_transceiverSet->list(); }

    ExceptionOr<Ref<RTCRtpSender>> addTrack(Ref<MediaStreamTrack>&&, const Vector<std::reference_wrapper<MediaStream>>&);
    ExceptionOr<void> removeTrack(RTCRtpSender&);

    using AddTransceiverTrackOrKind = Variant<RefPtr<MediaStreamTrack>, String>;
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(AddTransceiverTrackOrKind&&, const RTCRtpTransceiverInit&);

    // 6.1 Peer-to-peer data API
    ExceptionOr<Ref<RTCDataChannel>> createDataChannel(String&&, RTCDataChannelInit&&);

    // 8.2 Statistics API
    void getStats(MediaStreamTrack*, Ref<DeferredPromise>&&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return RTCPeerConnectionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

    // Used for testing with a mock
    WEBCORE_EXPORT void emulatePlatformEvent(const String& action);

    // API used by PeerConnectionBackend and relatives
    void addTransceiver(Ref<RTCRtpTransceiver>&&);
    void setSignalingState(RTCSignalingState);
    void updateIceGatheringState(RTCIceGatheringState);
    void updateIceConnectionState(RTCIceConnectionState);

    void scheduleNegotiationNeededEvent();

    void dispatchEventWhenFeasible(Ref<Event>&&);

    void disableICECandidateFiltering() { m_backend->disableICECandidateFiltering(); }
    void enableICECandidateFiltering() { m_backend->enableICECandidateFiltering(); }

    void clearController() { m_controller = nullptr; }

    Document* document();

    void doTask(Function<void()>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "RTCPeerConnection"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    RTCPeerConnection(Document&);

    ExceptionOr<void> initializeConfiguration(RTCConfiguration&&);

    void registerToController(RTCController&);
    void unregisterFromController();

    friend class Internals;
    void applyRotationForOutgoingVideoSources() { m_backend->applyRotationForOutgoingVideoSources(); }

    // EventTarget implementation.
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void dispatchEvent(Event&) final;

    // ActiveDOMObject
    WEBCORE_EXPORT void stop() final;
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    bool virtualHasPendingActivity() const final;

    void updateConnectionState();
    bool doClose();
    void doStop();

    void getStats(RTCRtpSender& sender, Ref<DeferredPromise>&& promise) { m_backend->getStats(sender, WTFMove(promise)); }

    ExceptionOr<Vector<MediaEndpointConfiguration::CertificatePEM>> certificatesFromConfiguration(const RTCConfiguration&);
    void chainOperation(Ref<DeferredPromise>&&, Function<void(Ref<DeferredPromise>&&)>&&);
    friend class RTCRtpSender;

    bool m_isStopped { false };
    RTCSignalingState m_signalingState { RTCSignalingState::Stable };
    RTCIceGatheringState m_iceGatheringState { RTCIceGatheringState::New };
    RTCIceConnectionState m_iceConnectionState { RTCIceConnectionState::New };
    RTCPeerConnectionState m_connectionState { RTCPeerConnectionState::New };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    std::unique_ptr<RtpTransceiverSet> m_transceiverSet { std::unique_ptr<RtpTransceiverSet>(new RtpTransceiverSet()) };

    std::unique_ptr<PeerConnectionBackend> m_backend;

    RTCConfiguration m_configuration;
    RTCController* m_controller { nullptr };
    Vector<RefPtr<RTCCertificate>> m_certificates;
    bool m_shouldDelayTasks { false };
    Vector<Function<void()>> m_pendingTasks;
    Deque<std::pair<Ref<DeferredPromise>, Function<void(Ref<DeferredPromise>&&)>>> m_operations;
    bool m_hasPendingOperation { false };
    bool m_shouldFireNegotiationNeededOnceOperationChainIsEmpty { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
