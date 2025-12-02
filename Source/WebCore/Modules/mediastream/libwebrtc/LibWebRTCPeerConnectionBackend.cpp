/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCPeerConnectionBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "DocumentPage.h"
#include "IceCandidate.h"
#include "LibWebRTCAudioModule.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCMediaEndpoint.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCRtpTransceiverBackend.h"
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "Page.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCRtpCapabilities.h"
#include "RTCRtpReceiver.h"
#include "RTCSessionDescription.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"
#include "RealtimeOutgoingAudioSource.h"
#include "RealtimeOutgoingVideoSource.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static webrtc::PeerConnectionInterface::RTCConfiguration configurationFromMediaEndpointConfiguration(MediaEndpointConfiguration&&);

static const std::unique_ptr<PeerConnectionBackend> createLibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection, MediaEndpointConfiguration&& configuration)
{
    if (!LibWebRTCProvider::webRTCAvailable()) {
        RELEASE_LOG_ERROR(WebRTC, "LibWebRTC is not available to create a backend");
        return nullptr;
    }
    Ref document = downcast<Document>(*peerConnection.scriptExecutionContext());
    ASSERT(document->settings().peerConnectionEnabled());
    RefPtr page = document->page();
    if (!page)
        return nullptr;

    auto& webRTCProvider = static_cast<LibWebRTCProvider&>(page->webRTCProvider());
    webRTCProvider.setEnableWebRTCEncryption(page->settings().webRTCEncryptionEnabled());

    RefPtr endpoint = LibWebRTCMediaEndpoint::create(peerConnection, webRTCProvider, document, configurationFromMediaEndpointConfiguration(WTFMove(configuration)));
    if (!endpoint)
        return nullptr;

    return makeUniqueWithoutRefCountedCheck<LibWebRTCPeerConnectionBackend, PeerConnectionBackend>(peerConnection, endpoint.releaseNonNull());
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createLibWebRTCPeerConnectionBackend;

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCPeerConnectionBackend);

LibWebRTCPeerConnectionBackend::LibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection, Ref<LibWebRTCMediaEndpoint>&& endpoint)
    : PeerConnectionBackend(peerConnection)
    , m_endpoint(WTFMove(endpoint))
{
    m_endpoint->setPeerConnectionBackend(*this);
}

LibWebRTCPeerConnectionBackend::~LibWebRTCPeerConnectionBackend() = default;

void LibWebRTCPeerConnectionBackend::suspend()
{
    m_endpoint->suspend();
}

void LibWebRTCPeerConnectionBackend::resume()
{
    m_endpoint->resume();
}

void LibWebRTCPeerConnectionBackend::disableICECandidateFiltering()
{
    PeerConnectionBackend::disableICECandidateFiltering();
    m_endpoint->disableSocketRelay();
}

bool LibWebRTCPeerConnectionBackend::isNegotiationNeeded(uint32_t eventId) const
{
    return m_endpoint->isNegotiationNeeded(eventId);
}

static inline webrtc::PeerConnectionInterface::BundlePolicy bundlePolicyfromConfiguration(const MediaEndpointConfiguration& configuration)
{
    switch (configuration.bundlePolicy) {
    case RTCBundlePolicy::MaxCompat:
        return webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
    case RTCBundlePolicy::MaxBundle:
        return webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
    case RTCBundlePolicy::Balanced:
        return webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
    }

    ASSERT_NOT_REACHED();
    return webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
}

static inline webrtc::PeerConnectionInterface::RtcpMuxPolicy rtcpMuxPolicyfromConfiguration(const MediaEndpointConfiguration& configuration)
{
    switch (configuration.rtcpMuxPolicy) {
    case RTCPMuxPolicy::Negotiate:
        return webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    case RTCPMuxPolicy::Require:
        return webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
    }

    ASSERT_NOT_REACHED();
    return webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
}

static inline webrtc::PeerConnectionInterface::IceTransportsType iceTransportPolicyfromConfiguration(const MediaEndpointConfiguration& configuration)
{
    switch (configuration.iceTransportPolicy) {
    case RTCIceTransportPolicy::Relay:
        return webrtc::PeerConnectionInterface::kRelay;
    case RTCIceTransportPolicy::All:
        return webrtc::PeerConnectionInterface::kAll;
    }

    ASSERT_NOT_REACHED();
    return webrtc::PeerConnectionInterface::kNone;
}

webrtc::PeerConnectionInterface::RTCConfiguration configurationFromMediaEndpointConfiguration(MediaEndpointConfiguration&& configuration)
{
    webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;

    rtcConfiguration.type = iceTransportPolicyfromConfiguration(configuration);
    rtcConfiguration.bundle_policy = bundlePolicyfromConfiguration(configuration);
    rtcConfiguration.rtcp_mux_policy = rtcpMuxPolicyfromConfiguration(configuration);

    for (auto& server : configuration.iceServers) {
        webrtc::PeerConnectionInterface::IceServer iceServer;
        iceServer.username = server.username.utf8().data();
        iceServer.password = server.credential.utf8().data();
        for (auto& url : server.urls)
            iceServer.urls.push_back({ url.string().utf8().data() });
        rtcConfiguration.servers.push_back(WTFMove(iceServer));
    }

    // FIXME: Activate ice candidate pool size once it no longer bothers test bots.
    // rtcConfiguration.ice_candidate_pool_size = configuration.iceCandidatePoolSize;

    for (auto& pem : configuration.certificates) {
        rtcConfiguration.certificates.push_back(webrtc::RTCCertificate::FromPEM(webrtc::RTCCertificatePEM {
            pem.privateKey.utf8().data(), pem.certificate.utf8().data()
        }));
    }

    return rtcConfiguration;
}

void LibWebRTCPeerConnectionBackend::restartIce()
{
    m_endpoint->restartIce();
}

bool LibWebRTCPeerConnectionBackend::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    return m_endpoint->setConfiguration(configurationFromMediaEndpointConfiguration(WTFMove(configuration)));
}

void LibWebRTCPeerConnectionBackend::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    m_endpoint->gatherDecoderImplementationName(WTFMove(callback));

}

void LibWebRTCPeerConnectionBackend::getStats(Ref<DeferredPromise>&& promise)
{
    m_endpoint->getStats(WTFMove(promise));
}

static inline LibWebRTCRtpSenderBackend& backendFromRTPSender(RTCRtpSender& sender)
{
    ASSERT(!sender.isStopped());
    return static_cast<LibWebRTCRtpSenderBackend&>(*sender.backend());
}

void LibWebRTCPeerConnectionBackend::getStats(RTCRtpSender& sender, Ref<DeferredPromise>&& promise)
{
    webrtc::RtpSenderInterface* rtcSender = sender.backend() ? backendFromRTPSender(sender).rtcSender() : nullptr;

    if (!rtcSender) {
        m_endpoint->getStats(WTFMove(promise));
        return;
    }
    m_endpoint->getStats(*rtcSender, WTFMove(promise));
}

void LibWebRTCPeerConnectionBackend::getStats(RTCRtpReceiver& receiver, Ref<DeferredPromise>&& promise)
{
    RefPtr<webrtc::RtpReceiverInterface> rtcReceiver;
    if (auto* backend = receiver.backend())
        rtcReceiver = &static_cast<LibWebRTCRtpReceiverBackend*>(backend)->rtcReceiver();

    if (!rtcReceiver) {
        m_endpoint->getStats(WTFMove(promise));
        return;
    }
    m_endpoint->getStats(*rtcReceiver, WTFMove(promise));
}

void LibWebRTCPeerConnectionBackend::doSetLocalDescription(const RTCSessionDescription* description)
{
    m_endpoint->doSetLocalDescription(description);
    m_isLocalDescriptionSet = true;
}

void LibWebRTCPeerConnectionBackend::doSetRemoteDescription(const RTCSessionDescription& description)
{
    m_endpoint->doSetRemoteDescription(description);
    m_isRemoteDescriptionSet = true;
}

void LibWebRTCPeerConnectionBackend::doCreateOffer(RTCOfferOptions&& options)
{
    m_endpoint->doCreateOffer(options);
}

void LibWebRTCPeerConnectionBackend::doCreateAnswer(RTCAnswerOptions&&)
{
    if (!m_isRemoteDescriptionSet) {
        createAnswerFailed(Exception { ExceptionCode::InvalidStateError, "No remote description set"_s });
        return;
    }
    m_endpoint->doCreateAnswer();
}

void LibWebRTCPeerConnectionBackend::close()
{
    m_endpoint->close();
}

void LibWebRTCPeerConnectionBackend::doStop()
{
    m_endpoint->stop();
    m_pendingReceivers.clear();
}

void LibWebRTCPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate, AddIceCandidateCallback&& callback)
{
    webrtc::SdpParseError error;
    int sdpMLineIndex = candidate.sdpMLineIndex() ? candidate.sdpMLineIndex().value() : 0;
    std::unique_ptr<webrtc::IceCandidate> rtcCandidate(webrtc::CreateIceCandidate(candidate.sdpMid().utf8().data(), sdpMLineIndex, candidate.candidate().utf8().data(), &error));

    if (!rtcCandidate) {
        callback(Exception { ExceptionCode::OperationError, String::fromUTF8(error.description) });
        return;
    }

    m_endpoint->addIceCandidate(WTFMove(rtcCandidate), WTFMove(callback));
}

Ref<RTCRtpReceiver> LibWebRTCPeerConnectionBackend::createReceiver(std::unique_ptr<LibWebRTCRtpReceiverBackend>&& backend)
{
    auto& document = downcast<Document>(*protectedPeerConnection()->scriptExecutionContext());

    auto source = backend->createSource(document);

    // Remote source is initially muted and will be unmuted when receiving the first packet.
    source->setMuted(true);
    auto trackID = source->persistentID();
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(document.logger(), WTFMove(source), WTFMove(trackID));
    auto remoteTrack = MediaStreamTrack::create(document, WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(*this, WTFMove(remoteTrack), WTFMove(backend));
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCPeerConnectionBackend::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_endpoint->createDataChannel(label, options);
}

static inline RefPtr<RTCRtpSender> findExistingSender(const Vector<RefPtr<RTCRtpTransceiver>>& transceivers, LibWebRTCRtpSenderBackend& senderBackend)
{
    ASSERT(senderBackend.rtcSender());
    for (auto& transceiver : transceivers) {
        auto& sender = transceiver->sender();
        if (!sender.isStopped() && senderBackend.rtcSender() == backendFromRTPSender(sender).rtcSender())
            return Ref { sender };
    }
    return nullptr;
}

ExceptionOr<Ref<RTCRtpSender>> LibWebRTCPeerConnectionBackend::addTrack(MediaStreamTrack& track, FixedVector<String>&& mediaStreamIds)
{
    auto senderBackend = makeUnique<LibWebRTCRtpSenderBackend>(*this, nullptr);
    if (!m_endpoint->addTrack(*senderBackend, track, mediaStreamIds))
        return Exception { ExceptionCode::TypeError, "Unable to add track"_s };

    Ref peerConnection = m_peerConnection.get();
    if (auto sender = findExistingSender(peerConnection->currentTransceivers(), *senderBackend)) {
        backendFromRTPSender(*sender).takeSource(*senderBackend);
        sender->setTrack(track);
        sender->setMediaStreamIds(mediaStreamIds);
        return sender.releaseNonNull();
    }

    auto transceiverBackend = m_endpoint->transceiverBackendFromSender(*senderBackend);

    auto sender = RTCRtpSender::create(peerConnection, track, WTFMove(senderBackend));
    sender->setMediaStreamIds(mediaStreamIds);
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend());
    auto transceiver = RTCRtpTransceiver::create(sender.copyRef(), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(WTFMove(transceiver));
    return sender;
}

template<typename T>
ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addTransceiverFromTrackOrKind(T&& trackOrKind, const RTCRtpTransceiverInit& init, IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    auto result = m_endpoint->addTransceiver(trackOrKind, init, ignoreNegotiationNeededFlag);
    if (result.hasException())
        return result.releaseException();

    auto backends = result.releaseReturnValue();
    Ref peerConnection = m_peerConnection.get();
    auto sender = RTCRtpSender::create(peerConnection, std::forward<T>(trackOrKind), WTFMove(backends.senderBackend));
    auto receiver = createReceiver(WTFMove(backends.receiverBackend));
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(backends.transceiverBackend));
    peerConnection->addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init, IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    return addTransceiverFromTrackOrKind(String { trackKind }, init, ignoreNegotiationNeededFlag);
}

ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&& track, const RTCRtpTransceiverInit& init)
{
    return addTransceiverFromTrackOrKind(WTFMove(track), init);
}

void LibWebRTCPeerConnectionBackend::setSenderSourceFromTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track)
{
    m_endpoint->setSenderSourceFromTrack(sender, track);
}

static inline LibWebRTCRtpTransceiverBackend& backendFromRTPTransceiver(RTCRtpTransceiver& transceiver)
{
    return static_cast<LibWebRTCRtpTransceiverBackend&>(*transceiver.backend());
}

RefPtr<RTCRtpTransceiver> LibWebRTCPeerConnectionBackend::existingTransceiver(Function<bool(LibWebRTCRtpTransceiverBackend&)>&& matchingFunction)
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (matchingFunction(backendFromRTPTransceiver(*transceiver)))
            return transceiver;
    }
    return nullptr;
}

Ref<RTCRtpTransceiver> LibWebRTCPeerConnectionBackend::newRemoteTransceiver(std::unique_ptr<LibWebRTCRtpTransceiverBackend>&& transceiverBackend, RealtimeMediaSource::Type type)
{
    Ref peerConnection = m_peerConnection.get();
    Ref sender = RTCRtpSender::create(peerConnection, type == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s, transceiverBackend->createSenderBackend(*this, nullptr));
    Ref receiver = createReceiver(transceiverBackend->createReceiverBackend());
    Ref transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

void LibWebRTCPeerConnectionBackend::collectTransceivers()
{
    m_endpoint->collectTransceivers();
}

void LibWebRTCPeerConnectionBackend::removeTrack(RTCRtpSender& sender)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Removing "_s, sender.trackKind(), " track with ID "_s, sender.trackId());
    m_endpoint->removeTrack(backendFromRTPSender(sender));
}

void LibWebRTCPeerConnectionBackend::applyRotationForOutgoingVideoSources()
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (!transceiver->sender().isStopped()) {
            if (auto* videoSource = backendFromRTPSender(transceiver->sender()).videoSource())
                videoSource->applyRotation();
        }
    }
}

std::optional<bool> LibWebRTCPeerConnectionBackend::canTrickleIceCandidates() const
{
    return m_endpoint->canTrickleIceCandidates();
}

void LibWebRTCPeerConnectionBackend::startGatheringStatLogs(Function<void(String&&)>&& callback)
{
    if (!m_rtcStatsLogCallback)
        m_endpoint->startRTCLogs();
    m_rtcStatsLogCallback = WTFMove(callback);
}

void LibWebRTCPeerConnectionBackend::stopGatheringStatLogs()
{
    if (m_rtcStatsLogCallback) {
        m_endpoint->stopRTCLogs();
        m_rtcStatsLogCallback = { };
    }
}

void LibWebRTCPeerConnectionBackend::provideStatLogs(String&& stats)
{
    if (m_rtcStatsLogCallback)
        m_rtcStatsLogCallback(WTFMove(stats));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
