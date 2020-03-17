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
#include "LibWebRTCProvider.h"
#include "RTCRtpSendParameters.h"
#include "RTCSessionDescription.h"
#include "RTCSignalingState.h"
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
class RTCIceCandidate;
class RTCPeerConnection;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCRtpTransceiver;
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
using SessionDescriptionPromise = DOMPromiseDeferred<IDLDictionary<RTCSessionDescription::Init>>;
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

    static Optional<RTCRtpCapabilities> receiverCapabilities(ScriptExecutionContext&, const String& kind);
    static Optional<RTCRtpCapabilities> senderCapabilities(ScriptExecutionContext&, const String& kind);

    explicit PeerConnectionBackend(RTCPeerConnection&);
    virtual ~PeerConnectionBackend();

    void createOffer(RTCOfferOptions&&, PeerConnection::SessionDescriptionPromise&&);
    void createAnswer(RTCAnswerOptions&&, PeerConnection::SessionDescriptionPromise&&);
    void setLocalDescription(RTCSessionDescription&, DOMPromiseDeferred<void>&&);
    void setRemoteDescription(RTCSessionDescription&, DOMPromiseDeferred<void>&&);
    void addIceCandidate(RTCIceCandidate*, DOMPromiseDeferred<void>&&);

    virtual std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) = 0;

    void stop();

    virtual void close() = 0;

    virtual RefPtr<RTCSessionDescription> localDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentLocalDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingLocalDescription() const = 0;

    virtual RefPtr<RTCSessionDescription> remoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentRemoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingRemoteDescription() const = 0;

    virtual bool setConfiguration(MediaEndpointConfiguration&&) = 0;

    virtual void getStats(Ref<DeferredPromise>&&) = 0;
    virtual void getStats(RTCRtpSender&, Ref<DeferredPromise>&&) = 0;
    virtual void getStats(RTCRtpReceiver&, Ref<DeferredPromise>&&) = 0;

    virtual ExceptionOr<Ref<RTCRtpSender>> addTrack(MediaStreamTrack&, Vector<String>&&);
    virtual void removeTrack(RTCRtpSender&) { }

    virtual ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(const String&, const RTCRtpTransceiverInit&);
    virtual ExceptionOr<Ref<RTCRtpTransceiver>> addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&);

    void markAsNeedingNegotiation();
    bool isNegotiationNeeded() const { return m_negotiationNeeded; };
    void clearNegotiationNeededState() { m_negotiationNeeded = false; };

    virtual void emulatePlatformEvent(const String& action) = 0;

    void newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex, String&& serverURL);
    void disableICECandidateFiltering();
    void enableICECandidateFiltering();

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
        Optional<double> expires;

        Optional<RSA> rsaParameters;
    };
    static void generateCertificate(Document&, const CertificateInformation&, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&&);

    virtual void collectTransceivers() { };

    ScriptExecutionContext* context() const;
    RTCRtpTransceiver* transceiverFromSender(const RTCRtpSender&);

    virtual void suspend() { }
    virtual void resume() { }

protected:
    void fireICECandidateEvent(RefPtr<RTCIceCandidate>&&, String&& url);
    void doneGatheringCandidates();

    void updateSignalingState(RTCSignalingState);

    void createOfferSucceeded(String&&);
    void createOfferFailed(Exception&&);

    void createAnswerSucceeded(String&&);
    void createAnswerFailed(Exception&&);

    void setLocalDescriptionSucceeded();
    void setLocalDescriptionFailed(Exception&&);

    void setRemoteDescriptionSucceeded();
    void setRemoteDescriptionFailed(Exception&&);

    void addIceCandidateSucceeded();
    void addIceCandidateFailed(Exception&&);

    String filterSDP(String&&) const;

    struct PendingTrackEvent {
        Ref<RTCRtpReceiver> receiver;
        Ref<MediaStreamTrack> track;
        Vector<RefPtr<MediaStream>> streams;
        RefPtr<RTCRtpTransceiver> transceiver;
    };
    void addPendingTrackEvent(PendingTrackEvent&&);

private:
    virtual void doCreateOffer(RTCOfferOptions&&) = 0;
    virtual void doCreateAnswer(RTCAnswerOptions&&) = 0;
    virtual void doSetLocalDescription(RTCSessionDescription&) = 0;
    virtual void doSetRemoteDescription(RTCSessionDescription&) = 0;
    virtual void doAddIceCandidate(RTCIceCandidate&) = 0;
    virtual void endOfIceCandidates(DOMPromiseDeferred<void>&&);
    virtual void doStop() = 0;

    void registerMDNSName(const String& ipAddress);

protected:
    RTCPeerConnection& m_peerConnection;

private:
    std::unique_ptr<PeerConnection::SessionDescriptionPromise> m_offerAnswerPromise;
    std::unique_ptr<DOMPromiseDeferred<void>> m_setDescriptionPromise;
    std::unique_ptr<DOMPromiseDeferred<void>> m_addIceCandidatePromise;

    bool m_shouldFilterICECandidates { true };
    struct PendingICECandidate {
        // Fields described in https://www.w3.org/TR/webrtc/#idl-def-rtcicecandidateinit.
        String sdp;
        String mid;
        unsigned short sdpMLineIndex;
        String serverURL;
    };
    Vector<PendingICECandidate> m_pendingICECandidates;

    Vector<PendingTrackEvent> m_pendingTrackEvents;

    HashMap<String, String> m_ipAddressToMDNSNameMap;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
    bool m_negotiationNeeded { false };
    bool m_finishedGatheringCandidates { false };
    uint64_t m_waitingForMDNSRegistration { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
