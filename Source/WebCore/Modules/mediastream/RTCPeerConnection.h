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
#include "RTCLocalSessionDescriptionInit.h"
#include "RTCPeerConnectionState.h"
#include "RTCRtpEncodingParameters.h"
#include "RTCRtpTransceiver.h"
#include "RTCSessionDescriptionInit.h"
#include "RTCSignalingState.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMPromiseDeferredBase;
class MediaStream;
class MediaStreamTrack;
class RTCController;
class RTCDtlsTransport;
class RTCDtlsTransportBackend;
class RTCIceCandidate;
class RTCIceTransportBackend;
class RTCPeerConnectionErrorCallback;
class RTCSctpTransport;
class RTCSessionDescription;
class RTCStatsCallback;

struct RTCAnswerOptions;
struct RTCOfferOptions;
struct RTCRtpParameters;
struct RTCIceCandidateInit;
struct RTCSessionDescriptionInit;

struct RTCRtpTransceiverInit {
    RTCRtpTransceiverDirection direction { RTCRtpTransceiverDirection::Sendrecv };
    Vector<Ref<MediaStream>> streams;
    Vector<RTCRtpEncodingParameters> sendEncodings;
};

class RTCPeerConnection final
    : public RefCounted<RTCPeerConnection>
    , public EventTarget
    , public ActiveDOMObject
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(RTCPeerConnection, WEBCORE_EXPORT);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static ExceptionOr<Ref<RTCPeerConnection>> create(Document&, RTCConfiguration&&);
    WEBCORE_EXPORT virtual ~RTCPeerConnection();

    using DataChannelInit = RTCDataChannelInit;

    struct CertificateParameters {
        String name;
        String hash;
        String namedCurve;
        std::optional<uint32_t> modulusLength;
        RefPtr<Uint8Array> publicExponent;
        std::optional<double> expires;
    };

    using AlgorithmIdentifier = std::variant<JSC::Strong<JSC::JSObject>, String>;
    static void generateCertificate(JSC::JSGlobalObject&, AlgorithmIdentifier&&, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&&);

    // 4.3.2 RTCPeerConnection Interface
    void createOffer(RTCOfferOptions&&, Ref<DeferredPromise>&&);
    void createAnswer(RTCAnswerOptions&&, Ref<DeferredPromise>&&);

    void setLocalDescription(std::optional<RTCLocalSessionDescriptionInit>&&, Ref<DeferredPromise>&&);
    RefPtr<RTCSessionDescription> localDescription() const { return m_pendingLocalDescription ? m_pendingLocalDescription.get() : m_currentLocalDescription.get(); }
    RefPtr<RTCSessionDescription> currentLocalDescription() const { return m_currentLocalDescription.get(); }
    RefPtr<RTCSessionDescription> pendingLocalDescription() const { return m_pendingLocalDescription.get(); }

    void setRemoteDescription(RTCSessionDescriptionInit&&, Ref<DeferredPromise>&&);
    RTCSessionDescription* remoteDescription() const { return m_pendingRemoteDescription ? m_pendingRemoteDescription.get() : m_currentRemoteDescription.get(); }
    RTCSessionDescription* currentRemoteDescription() const { return m_currentRemoteDescription.get(); }
    RTCSessionDescription* pendingRemoteDescription() const { return m_pendingRemoteDescription.get(); }

    using Candidate = std::optional<std::variant<RTCIceCandidateInit, RefPtr<RTCIceCandidate>>>;
    void addIceCandidate(Candidate&&, Ref<DeferredPromise>&&);

    RTCSignalingState signalingState() const { return m_signalingState; }
    RTCIceGatheringState iceGatheringState() const { return m_iceGatheringState; }
    RTCIceConnectionState iceConnectionState() const { return m_iceConnectionState; }
    RTCPeerConnectionState connectionState() const { return m_connectionState; }
    std::optional<bool> canTrickleIceCandidates() const;

    void restartIce() { protectedBackend()->restartIce(); }
    const RTCConfiguration& getConfiguration() const { return m_configuration; }
    ExceptionOr<void> setConfiguration(RTCConfiguration&&);
    void close();

    bool isClosed() const { return m_connectionState == RTCPeerConnectionState::Closed; }
    bool isStopped() const { return m_isStopped; }

    void addInternalTransceiver(Ref<RTCRtpTransceiver>&&);

    // 5.1 RTCPeerConnection extensions
    Vector<std::reference_wrapper<RTCRtpSender>> getSenders() const;
    Vector<std::reference_wrapper<RTCRtpReceiver>> getReceivers() const;
    const Vector<RefPtr<RTCRtpTransceiver>>& getTransceivers() const;

    const Vector<RefPtr<RTCRtpTransceiver>>& currentTransceivers() const { return m_transceiverSet.list(); }

    ExceptionOr<Ref<RTCRtpSender>> addTrack(Ref<MediaStreamTrack>&&, const FixedVector<std::reference_wrapper<MediaStream>>&);
    ExceptionOr<void> removeTrack(RTCRtpSender&);

    using AddTransceiverTrackOrKind = std::variant<RefPtr<MediaStreamTrack>, String>;
    ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(AddTransceiverTrackOrKind&&, RTCRtpTransceiverInit&&);

    // 6.1 Peer-to-peer data API
    ExceptionOr<Ref<RTCDataChannel>> createDataChannel(String&&, RTCDataChannelInit&&);

    // 8.2 Statistics API
    void getStats(MediaStreamTrack*, Ref<DeferredPromise>&&);
    // Used for testing
    WEBCORE_EXPORT void gatherDecoderImplementationName(Function<void(String&&)>&&);

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::RTCPeerConnection; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    // Used for testing with a mock
    WEBCORE_EXPORT void emulatePlatformEvent(const String& action);

    // API used by PeerConnectionBackend and relatives
    void updateIceGatheringState(RTCIceGatheringState);
    void updateIceConnectionState(RTCIceConnectionState);
    void updateConnectionState();

    void updateNegotiationNeededFlag(std::optional<uint32_t>);

    void scheduleEvent(Ref<Event>&&);

    void disableICECandidateFiltering() { protectedBackend()->disableICECandidateFiltering(); }
    void enableICECandidateFiltering() { protectedBackend()->enableICECandidateFiltering(); }

    void clearController() { m_controller = nullptr; }

    Document* document();

    void updateDescriptions(PeerConnectionBackend::DescriptionStates&&);
    void updateTransceiversAfterSuccessfulLocalDescription();
    void updateTransceiversAfterSuccessfulRemoteDescription();
    void updateSctpBackend(std::unique_ptr<RTCSctpTransportBackend>&&, std::optional<double>);

    void processIceTransportStateChange(RTCIceTransport&);
    void processIceTransportChanges();

    RTCSctpTransport* sctp() { return m_sctpTransport.get(); }

    // EventTarget implementation.
    void dispatchEvent(Event&) final;

    void dispatchDataChannelEvent(UniqueRef<RTCDataChannelHandler>&&, String&& label, RTCDataChannelInit&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "RTCPeerConnection"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    void startGatheringStatLogs(Function<void(String&&)>&&);
    void stopGatheringStatLogs();

private:
    RTCPeerConnection(Document&);

    ExceptionOr<void> initializeConfiguration(RTCConfiguration&&);

    ExceptionOr<Ref<RTCRtpTransceiver>> addReceiveOnlyTransceiver(String&&);

    void registerToController(RTCController&);
    void unregisterFromController();

    friend class Internals;
    void applyRotationForOutgoingVideoSources() { protectedBackend()->applyRotationForOutgoingVideoSources(); }

    // EventTarget implementation.
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    WEBCORE_EXPORT void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    bool virtualHasPendingActivity() const final;

    bool updateIceConnectionStateFromIceTransports();
    RTCIceConnectionState computeIceConnectionStateFromIceTransports();
    RTCPeerConnectionState computeConnectionState();

    bool doClose();
    void doStop();

    void getStats(RTCRtpSender& sender, Ref<DeferredPromise>&& promise) { protectedBackend()->getStats(sender, WTFMove(promise)); }

    ExceptionOr<Vector<MediaEndpointConfiguration::CertificatePEM>> certificatesFromConfiguration(const RTCConfiguration&);
    void chainOperation(Ref<DeferredPromise>&&, Function<void(Ref<DeferredPromise>&&)>&&);
    friend class RTCRtpSender;

    ExceptionOr<Vector<MediaEndpointConfiguration::IceServerInfo>> iceServersFromConfiguration(RTCConfiguration& newConfiguration, const RTCConfiguration* existingConfiguration, bool isLocalDescriptionSet);

    Ref<RTCIceTransport> getOrCreateIceTransport(UniqueRef<RTCIceTransportBackend>&&);
    RefPtr<RTCDtlsTransport> getOrCreateDtlsTransport(std::unique_ptr<RTCDtlsTransportBackend>&&);
    void updateTransceiverTransports();

    void setSignalingState(RTCSignalingState);

    WEBCORE_EXPORT RefPtr<PeerConnectionBackend> protectedBackend() const;

    bool m_isStopped { false };
    RTCSignalingState m_signalingState { RTCSignalingState::Stable };
    RTCIceGatheringState m_iceGatheringState { RTCIceGatheringState::New };
    RTCIceConnectionState m_iceConnectionState { RTCIceConnectionState::New };
    RTCPeerConnectionState m_connectionState { RTCPeerConnectionState::New };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    RtpTransceiverSet m_transceiverSet;

    std::unique_ptr<PeerConnectionBackend> m_backend;

    RTCConfiguration m_configuration;
    RTCController* m_controller { nullptr };
    Vector<RefPtr<RTCCertificate>> m_certificates;
    bool m_shouldDelayTasks { false };
    Deque<std::pair<Ref<DeferredPromise>, Function<void(Ref<DeferredPromise>&&)>>> m_operations;
    bool m_hasPendingOperation { false };
    std::optional<uint32_t> m_negotiationNeededEventId;
    Vector<Ref<RTCDtlsTransport>> m_dtlsTransports;
    Vector<Ref<RTCIceTransport>> m_iceTransports;
    RefPtr<RTCSctpTransport> m_sctpTransport;

    RefPtr<RTCSessionDescription> m_currentLocalDescription;
    RefPtr<RTCSessionDescription> m_pendingLocalDescription;
    RefPtr<RTCSessionDescription> m_currentRemoteDescription;
    RefPtr<RTCSessionDescription> m_pendingRemoteDescription;

    String m_lastCreatedOffer;
    String m_lastCreatedAnswer;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
