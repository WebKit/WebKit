/*
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

#include "IDLTypes.h"
#include "RTCIceGatheringState.h"
#include "RTCRtpCapabilities.h"
#include "RTCRtpSendParameters.h"
#include "RTCRtpTransceiverDirection.h"
#include "RTCSessionDescription.h"
#include "RTCSignalingState.h"
#include <wtf/FixedVector.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class Document;
class MediaStream;
class MediaStreamTrack;
class PeerConnectionBackend;
class RTCCertificate;
class RTCDataChannelHandler;
class RTCDtlsTransport;
class RTCDtlsTransportBackend;
class RTCIceCandidate;
class RTCPeerConnection;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCRtpTransceiver;
class RTCSctpTransportBackend;
class RTCSessionDescription;
class RTCStatsReport;
class ScriptExecutionContext;

struct MediaEndpointConfiguration;
struct RTCAnswerOptions;
struct RTCDataChannelInit;
struct RTCOfferOptions;
struct RTCRtpTransceiverInit;

template<typename IDLType> class DOMPromiseDeferred;

namespace PeerConnection {
using SessionDescriptionPromise = DOMPromiseDeferred<IDLDictionary<RTCSessionDescriptionInit>>;
using StatsPromise = DOMPromiseDeferred<IDLInterface<RTCStatsReport>>;
}

using CreatePeerConnectionBackend = std::unique_ptr<PeerConnectionBackend> (*)(RTCPeerConnection&);

class PeerConnectionBackend
    : public CanMakeWeakPtr<PeerConnectionBackend>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    WEBCORE_EXPORT static CreatePeerConnectionBackend create;

    static std::optional<RTCRtpCapabilities> receiverCapabilities(ScriptExecutionContext&, const String& kind);
    static std::optional<RTCRtpCapabilities> senderCapabilities(ScriptExecutionContext&, const String& kind);

    explicit PeerConnectionBackend(RTCPeerConnection&);
    virtual ~PeerConnectionBackend();

    using CreateCallback = Function<void(ExceptionOr<RTCSessionDescriptionInit>&&)>;
    void createOffer(RTCOfferOptions&&, CreateCallback&&);
    void createAnswer(RTCAnswerOptions&&, CreateCallback&&);
    void setLocalDescription(const RTCSessionDescription*, Function<void(ExceptionOr<void>&&)>&&);
    void setRemoteDescription(const RTCSessionDescription&, Function<void(ExceptionOr<void>&&)>&&);
    void addIceCandidate(RTCIceCandidate*, Function<void(ExceptionOr<void>&&)>&&);

    virtual std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) = 0;

    void stop();

    virtual void close() = 0;

    virtual void restartIce() = 0;
    virtual bool setConfiguration(MediaEndpointConfiguration&&) = 0;

    virtual void gatherDecoderImplementationName(Function<void(String&&)>&&) = 0;
    virtual void getStats(Ref<DeferredPromise>&&) = 0;
    virtual void getStats(RTCRtpSender&, Ref<DeferredPromise>&&) = 0;
    virtual void getStats(RTCRtpReceiver&, Ref<DeferredPromise>&&) = 0;

    virtual ExceptionOr<Ref<RTCRtpSender>> addTrack(MediaStreamTrack&, FixedVector<String>&&);
    virtual void removeTrack(RTCRtpSender&) { }

    virtual ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(const String&, const RTCRtpTransceiverInit&);
    virtual ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&);

    void markAsNeedingNegotiation(uint32_t);
    virtual bool isNegotiationNeeded(uint32_t) const = 0;

    virtual void emulatePlatformEvent(const String& action) = 0;

    struct DescriptionStates {
        std::optional<RTCSignalingState> signalingState;
        std::optional<RTCSdpType> currentLocalDescriptionSdpType;
        String currentLocalDescriptionSdp;
        std::optional<RTCSdpType> pendingLocalDescriptionSdpType;
        String pendingLocalDescriptionSdp;
        std::optional<RTCSdpType> currentRemoteDescriptionSdpType;
        String currentRemoteDescriptionSdp;
        std::optional<RTCSdpType> pendingRemoteDescriptionSdpType;
        String pendingRemoteDescriptionSdp;

        DescriptionStates isolatedCopy() &&;
    };
    struct TransceiverState {
        String mid;
        Vector<RefPtr<MediaStream>> receiverStreams;
        std::optional<RTCRtpTransceiverDirection> firedDirection;
    };
    using TransceiverStates = Vector<TransceiverState>;

    void newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex, String&& serverURL, std::optional<DescriptionStates>&&);
    void newDataChannel(UniqueRef<RTCDataChannelHandler>&&, String&&, RTCDataChannelInit&&);

    virtual void disableICECandidateFiltering();
    void enableICECandidateFiltering();

    virtual std::optional<bool> canTrickleIceCandidates() const = 0;

    virtual void applyRotationForOutgoingVideoSources() { }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const override { return "PeerConnectionBackend"; }
    WTFLogChannel& logChannel() const final;
#endif

    virtual bool isLocalDescriptionSet() const = 0;

    void finishedRegisteringMDNSName(const String& ipAddress, const String& name);

    struct CertificateInformation {
        enum class Type { RSASSAPKCS1v15, ECDSAP256 };
        struct RSA {
            unsigned modulusLength;
            int publicExponent;
        };

        static CertificateInformation RSASSA_PKCS1_v1_5()
        {
            return CertificateInformation { Type::RSASSAPKCS1v15 };
        }

        static CertificateInformation ECDSA_P256()
        {
            return CertificateInformation { Type::ECDSAP256 };
        }

        explicit CertificateInformation(Type type)
            : type(type)
        {
        }

        Type type;
        std::optional<double> expires;

        std::optional<RSA> rsaParameters;
    };
    static void generateCertificate(Document&, const CertificateInformation&, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&&);

    virtual void collectTransceivers() { };

    ScriptExecutionContext* context() const;

    virtual void suspend() { }
    virtual void resume() { }

    bool shouldFilterICECandidates() const { return m_shouldFilterICECandidates; };

    using AddIceCandidateCallbackFunction = void(ExceptionOr<std::optional<PeerConnectionBackend::DescriptionStates>>&&);
    using AddIceCandidateCallback = Function<AddIceCandidateCallbackFunction>;

    void iceGatheringStateChanged(RTCIceGatheringState);

protected:
    void doneGatheringCandidates();

    void createOfferSucceeded(String&&);
    void createOfferFailed(Exception&&);

    void createAnswerSucceeded(String&&);
    void createAnswerFailed(Exception&&);

    void setLocalDescriptionSucceeded(std::optional<DescriptionStates>&&, std::optional<TransceiverStates>&&, std::unique_ptr<RTCSctpTransportBackend>&&);
    void setLocalDescriptionFailed(Exception&&);

    void setRemoteDescriptionSucceeded(std::optional<DescriptionStates>&&, std::optional<TransceiverStates>&&, std::unique_ptr<RTCSctpTransportBackend>&&);
    void setRemoteDescriptionFailed(Exception&&);

    void validateSDP(const String&) const;

    struct PendingTrackEvent {
        Ref<RTCRtpReceiver> receiver;
        Ref<MediaStreamTrack> track;
        Vector<RefPtr<MediaStream>> streams;
        RefPtr<RTCRtpTransceiver> transceiver;
    };
    void addPendingTrackEvent(PendingTrackEvent&&);

    void dispatchTrackEvent(PendingTrackEvent&);

private:
    virtual void doCreateOffer(RTCOfferOptions&&) = 0;
    virtual void doCreateAnswer(RTCAnswerOptions&&) = 0;
    virtual void doSetLocalDescription(const RTCSessionDescription*) = 0;
    virtual void doSetRemoteDescription(const RTCSessionDescription&) = 0;
    virtual void doAddIceCandidate(RTCIceCandidate&, AddIceCandidateCallback&&) = 0;
    virtual void doStop() = 0;

protected:
    RTCPeerConnection& m_peerConnection;

private:
    CreateCallback m_offerAnswerCallback;
    Function<void(ExceptionOr<void>&&)> m_setDescriptionCallback;

    bool m_shouldFilterICECandidates { true };

    Vector<PendingTrackEvent> m_pendingTrackEvents;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
    bool m_finishedGatheringCandidates { false };
    bool m_isProcessingLocalDescriptionAnswer { false };
};

inline PeerConnectionBackend::DescriptionStates PeerConnectionBackend::DescriptionStates::isolatedCopy() &&
{
    return DescriptionStates {
        signalingState,
        currentLocalDescriptionSdpType,
        WTFMove(currentLocalDescriptionSdp).isolatedCopy(),
        pendingLocalDescriptionSdpType,
        WTFMove(pendingLocalDescriptionSdp).isolatedCopy(),
        currentRemoteDescriptionSdpType,
        WTFMove(currentRemoteDescriptionSdp).isolatedCopy(),
        pendingRemoteDescriptionSdpType,
        WTFMove(pendingRemoteDescriptionSdp).isolatedCopy()
    };
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
