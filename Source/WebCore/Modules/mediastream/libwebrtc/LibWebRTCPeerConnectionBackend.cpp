/*
 * Copyright (C) 2017-2018 Apple Inc.
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

#if USE(LIBWEBRTC)

#include "Document.h"
#include "IceCandidate.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCMediaEndpoint.h"
#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCRtpTransceiverBackend.h"
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
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

static std::unique_ptr<PeerConnectionBackend> createLibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection)
{
    if (!LibWebRTCProvider::webRTCAvailable())
        return nullptr;

    auto* page = downcast<Document>(*peerConnection.scriptExecutionContext()).page();
    if (!page)
        return nullptr;

    return std::make_unique<LibWebRTCPeerConnectionBackend>(peerConnection, page->libWebRTCProvider());
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createLibWebRTCPeerConnectionBackend;

Optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext& context, const String& kind)
{
    auto* page = downcast<Document>(context).page();
    if (!page)
        return { };
    return page->libWebRTCProvider().receiverCapabilities(kind);
}

Optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext& context, const String& kind)
{
    auto* page = downcast<Document>(context).page();
    if (!page)
        return { };
    return page->libWebRTCProvider().senderCapabilities(kind);
}

LibWebRTCPeerConnectionBackend::LibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection, LibWebRTCProvider& provider)
    : PeerConnectionBackend(peerConnection)
    , m_endpoint(LibWebRTCMediaEndpoint::create(*this, provider))
{
}

LibWebRTCPeerConnectionBackend::~LibWebRTCPeerConnectionBackend() = default;

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

static webrtc::PeerConnectionInterface::RTCConfiguration configurationFromMediaEndpointConfiguration(MediaEndpointConfiguration&& configuration)
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

    rtcConfiguration.set_cpu_adaptation(false);
    // FIXME: Activate ice candidate pool size once it no longer bothers test bots.
    // rtcConfiguration.ice_candidate_pool_size = configuration.iceCandidatePoolSize;

    for (auto& pem : configuration.certificates) {
        rtcConfiguration.certificates.push_back(rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM {
            pem.privateKey.utf8().data(), pem.certificate.utf8().data()
        }));
    }

    return rtcConfiguration;
}

bool LibWebRTCPeerConnectionBackend::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    auto* page = downcast<Document>(*m_peerConnection.scriptExecutionContext()).page();
    if (!page)
        return false;

    return m_endpoint->setConfiguration(page->libWebRTCProvider(), configurationFromMediaEndpointConfiguration(WTFMove(configuration)));
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
    webrtc::RtpReceiverInterface* rtcReceiver = receiver.backend() ? static_cast<LibWebRTCRtpReceiverBackend*>(receiver.backend())->rtcReceiver() : nullptr;

    if (!rtcReceiver) {
        m_endpoint->getStats(WTFMove(promise));
        return;
    }
    m_endpoint->getStats(*rtcReceiver, WTFMove(promise));
}

void LibWebRTCPeerConnectionBackend::doSetLocalDescription(RTCSessionDescription& description)
{
    m_endpoint->doSetLocalDescription(description);
    if (!m_isLocalDescriptionSet) {
        if (m_isRemoteDescriptionSet) {
            for (auto& candidate : m_pendingCandidates)
                m_endpoint->addIceCandidate(*candidate);
            m_pendingCandidates.clear();
        }
        m_isLocalDescriptionSet = true;
    }
}

void LibWebRTCPeerConnectionBackend::doSetRemoteDescription(RTCSessionDescription& description)
{
    m_endpoint->doSetRemoteDescription(description);
    if (!m_isRemoteDescriptionSet) {
        if (m_isLocalDescriptionSet) {
            for (auto& candidate : m_pendingCandidates)
                m_endpoint->addIceCandidate(*candidate);
        }
        m_isRemoteDescriptionSet = true;
    }
}

void LibWebRTCPeerConnectionBackend::doCreateOffer(RTCOfferOptions&& options)
{
    m_endpoint->doCreateOffer(options);
}

void LibWebRTCPeerConnectionBackend::doCreateAnswer(RTCAnswerOptions&&)
{
    if (!m_isRemoteDescriptionSet) {
        createAnswerFailed(Exception { InvalidStateError, "No remote description set" });
        return;
    }
    m_endpoint->doCreateAnswer();
}

void LibWebRTCPeerConnectionBackend::doStop()
{
    m_endpoint->stop();
    m_pendingReceivers.clear();
}

void LibWebRTCPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate)
{
    webrtc::SdpParseError error;
    int sdpMLineIndex = candidate.sdpMLineIndex() ? candidate.sdpMLineIndex().value() : 0;
    std::unique_ptr<webrtc::IceCandidateInterface> rtcCandidate(webrtc::CreateIceCandidate(candidate.sdpMid().utf8().data(), sdpMLineIndex, candidate.candidate().utf8().data(), &error));

    if (!rtcCandidate) {
        addIceCandidateFailed(Exception { OperationError, String::fromUTF8(error.description.data(), error.description.length()) });
        return;
    }

    // libwebrtc does not like that ice candidates are set before the description.
    if (!m_isLocalDescriptionSet || !m_isRemoteDescriptionSet)
        m_pendingCandidates.append(WTFMove(rtcCandidate));
    else if (!m_endpoint->addIceCandidate(*rtcCandidate.get())) {
        ASSERT_NOT_REACHED();
        addIceCandidateFailed(Exception { OperationError, "Failed to apply the received candidate"_s });
        return;
    }
    addIceCandidateSucceeded();
}

Ref<RTCRtpReceiver> LibWebRTCPeerConnectionBackend::createReceiverForSource(Ref<RealtimeMediaSource>&& source, std::unique_ptr<RTCRtpReceiverBackend>&& backend)
{
    String trackID = source->persistentID();
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(WTFMove(source), WTFMove(trackID));
    auto remoteTrack = MediaStreamTrack::create(*m_peerConnection.scriptExecutionContext(), WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(*this, WTFMove(remoteTrack), WTFMove(backend));
}

static inline Ref<RealtimeMediaSource> createEmptySource(const String& trackKind, String&& trackId)
{
    // FIXME: trackKind should be an enumeration
    if (trackKind == "audio")
        return RealtimeIncomingAudioSource::create(nullptr, WTFMove(trackId));
    ASSERT(trackKind == "video");
    return RealtimeIncomingVideoSource::create(nullptr, WTFMove(trackId));
}

Ref<RTCRtpReceiver> LibWebRTCPeerConnectionBackend::createReceiver(const String& trackKind, const String& trackId)
{
    auto receiver = createReceiverForSource(createEmptySource(trackKind, String(trackId)), nullptr);
    m_pendingReceivers.append(receiver.copyRef());
    return receiver;
}

LibWebRTCPeerConnectionBackend::VideoReceiver LibWebRTCPeerConnectionBackend::videoReceiver(String&& trackId)
{
    // FIXME: Add to Vector a utility routine for that take-or-create pattern.
    // FIXME: We should be selecting the receiver based on track id.
    for (size_t cptr = 0; cptr < m_pendingReceivers.size(); ++cptr) {
        if (m_pendingReceivers[cptr]->track().source().type() == RealtimeMediaSource::Type::Video) {
            Ref<RTCRtpReceiver> receiver = m_pendingReceivers[cptr].copyRef();
            m_pendingReceivers.remove(cptr);
            Ref<RealtimeIncomingVideoSource> source = static_cast<RealtimeIncomingVideoSource&>(receiver->track().source());
            return { WTFMove(receiver), WTFMove(source) };
        }
    }
    auto source = RealtimeIncomingVideoSource::create(nullptr, WTFMove(trackId));
    auto receiver = createReceiverForSource(source.copyRef(), nullptr);

    auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
    auto transceiver = RTCRtpTransceiver::create(RTCRtpSender::create(*this, "video"_s, { }, WTFMove(senderBackend)), receiver.copyRef(), nullptr);
    transceiver->disableSendingDirection();
    m_peerConnection.addTransceiver(WTFMove(transceiver));

    return { WTFMove(receiver), WTFMove(source) };
}

LibWebRTCPeerConnectionBackend::AudioReceiver LibWebRTCPeerConnectionBackend::audioReceiver(String&& trackId)
{
    // FIXME: Add to Vector a utility routine for that take-or-create pattern.
    // FIXME: We should be selecting the receiver based on track id.
    for (size_t cptr = 0; cptr < m_pendingReceivers.size(); ++cptr) {
        if (m_pendingReceivers[cptr]->track().source().type() == RealtimeMediaSource::Type::Audio) {
            Ref<RTCRtpReceiver> receiver = m_pendingReceivers[cptr].copyRef();
            m_pendingReceivers.remove(cptr);
            Ref<RealtimeIncomingAudioSource> source = static_cast<RealtimeIncomingAudioSource&>(receiver->track().source());
            return { WTFMove(receiver), WTFMove(source) };
        }
    }
    auto source = RealtimeIncomingAudioSource::create(nullptr, WTFMove(trackId));
    auto receiver = createReceiverForSource(source.copyRef(), nullptr);

    auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
    auto transceiver = RTCRtpTransceiver::create(RTCRtpSender::create(*this, "audio"_s, { }, WTFMove(senderBackend)), receiver.copyRef(), nullptr);
    transceiver->disableSendingDirection();
    m_peerConnection.addTransceiver(WTFMove(transceiver));

    return { WTFMove(receiver), WTFMove(source) };
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCPeerConnectionBackend::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_endpoint->createDataChannel(label, options);
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::currentLocalDescription() const
{
    auto description = m_endpoint->currentLocalDescription();
    if (description)
        description->setSdp(filterSDP(String(description->sdp())));
    return description;
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::currentRemoteDescription() const
{
    return m_endpoint->currentRemoteDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::pendingLocalDescription() const
{
    auto description = m_endpoint->pendingLocalDescription();
    if (description)
        description->setSdp(filterSDP(String(description->sdp())));
    return description;
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::pendingRemoteDescription() const
{
    return m_endpoint->pendingRemoteDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::localDescription() const
{
    auto description = m_endpoint->localDescription();
    if (description)
        description->setSdp(filterSDP(String(description->sdp())));
    return description;
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::remoteDescription() const
{
    return m_endpoint->remoteDescription();
}

static inline RefPtr<RTCRtpSender> findExistingSender(const Vector<RefPtr<RTCRtpTransceiver>>& transceivers, LibWebRTCRtpSenderBackend& senderBackend)
{
    ASSERT(senderBackend.rtcSender());
    for (auto& transceiver : transceivers) {
        auto& sender = transceiver->sender();
        if (!sender.isStopped() && senderBackend.rtcSender() == backendFromRTPSender(sender).rtcSender())
            return makeRef(sender);
    }
    return nullptr;
}

ExceptionOr<Ref<RTCRtpSender>> LibWebRTCPeerConnectionBackend::addTrack(MediaStreamTrack& track, Vector<String>&& mediaStreamIds)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled()) {
        auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
        if (!m_endpoint->addTrack(*senderBackend, track, mediaStreamIds))
            return Exception { TypeError, "Unable to add track"_s };

        if (auto sender = findExistingSender(m_peerConnection.currentTransceivers(), *senderBackend)) {
            backendFromRTPSender(*sender).takeSource(*senderBackend);
            sender->setTrack(makeRef(track));
            sender->setMediaStreamIds(WTFMove(mediaStreamIds));
            return sender.releaseNonNull();
        }

        auto transceiverBackend = m_endpoint->transceiverBackendFromSender(*senderBackend);

        auto sender = RTCRtpSender::create(*this, makeRef(track), WTFMove(mediaStreamIds), WTFMove(senderBackend));
        auto receiver = createReceiverForSource(createEmptySource(track.kind(), createCanonicalUUIDString()), transceiverBackend->createReceiverBackend());
        auto transceiver = RTCRtpTransceiver::create(sender.copyRef(), WTFMove(receiver), WTFMove(transceiverBackend));
        m_peerConnection.addInternalTransceiver(WTFMove(transceiver));
        return WTFMove(sender);
    }

    RTCRtpSender* sender = nullptr;
    // Reuse an existing sender with the same track kind if it has never been used to send before.
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
        auto& existingSender = transceiver->sender();
        if (!existingSender.isStopped() && existingSender.trackKind() == track.kind() && existingSender.trackId().isNull() && !transceiver->hasSendingDirection()) {
            existingSender.setTrack(makeRef(track));
            existingSender.setMediaStreamIds(WTFMove(mediaStreamIds));
            transceiver->enableSendingDirection();
            sender = &existingSender;

            break;
        }
    }

    if (!sender) {
        const String& trackKind = track.kind();
        String trackId = createCanonicalUUIDString();

        auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
        auto newSender = RTCRtpSender::create(*this, makeRef(track), Vector<String> { mediaStreamIds }, WTFMove(senderBackend));
        auto receiver = createReceiver(trackKind, trackId);
        auto transceiver = RTCRtpTransceiver::create(WTFMove(newSender), WTFMove(receiver), nullptr);

        sender = &transceiver->sender();
        m_peerConnection.addInternalTransceiver(WTFMove(transceiver));
    }

    if (!m_endpoint->addTrack(backendFromRTPSender(*sender), track, mediaStreamIds))
        return Exception { TypeError, "Unable to add track"_s };

    return makeRef(*sender);
}

template<typename T>
ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addUnifiedPlanTransceiver(T&& trackOrKind, const RTCRtpTransceiverInit& init)
{
    auto backends = m_endpoint->addTransceiver(trackOrKind, init);
    if (!backends)
        return Exception { InvalidAccessError, "Unable to add transceiver"_s };

    auto sender = RTCRtpSender::create(*this, WTFMove(trackOrKind), Vector<String> { }, WTFMove(backends->senderBackend));
    auto receiver = createReceiverForSource(createEmptySource(sender->trackKind(), createCanonicalUUIDString()), WTFMove(backends->receiverBackend));
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(backends->transceiverBackend));
    m_peerConnection.addInternalTransceiver(transceiver.copyRef());
    return WTFMove(transceiver);
}

ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled())
        return addUnifiedPlanTransceiver(String { trackKind }, init);

    auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
    auto newSender = RTCRtpSender::create(*this, String(trackKind), Vector<String>(), WTFMove(senderBackend));
    return completeAddTransceiver(WTFMove(newSender), init, createCanonicalUUIDString(), trackKind);
}

ExceptionOr<Ref<RTCRtpTransceiver>> LibWebRTCPeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&& track, const RTCRtpTransceiverInit& init)
{
    if (RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled())
        return addUnifiedPlanTransceiver(WTFMove(track), init);

    auto senderBackend = std::make_unique<LibWebRTCRtpSenderBackend>(*this, nullptr);
    auto& backend = *senderBackend;
    auto sender = RTCRtpSender::create(*this, track.copyRef(), Vector<String>(), WTFMove(senderBackend));
    if (!m_endpoint->addTrack(backend, track, Vector<String> { }))
        return Exception { InvalidAccessError, "Unable to add track"_s };

    return completeAddTransceiver(WTFMove(sender), init, track->id(), track->kind());
}

void LibWebRTCPeerConnectionBackend::setSenderSourceFromTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track)
{
    m_endpoint->setSenderSourceFromTrack(sender, track);
}

static inline LibWebRTCRtpTransceiverBackend& backendFromRTPTransceiver(RTCRtpTransceiver& transceiver)
{
    return static_cast<LibWebRTCRtpTransceiverBackend&>(*transceiver.backend());
}

RTCRtpTransceiver* LibWebRTCPeerConnectionBackend::existingTransceiver(WTF::Function<bool(LibWebRTCRtpTransceiverBackend&)>&& matchingFunction)
{
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
        if (matchingFunction(backendFromRTPTransceiver(*transceiver)))
            return transceiver.get();
    }
    return nullptr;
}

RTCRtpTransceiver& LibWebRTCPeerConnectionBackend::newRemoteTransceiver(std::unique_ptr<LibWebRTCRtpTransceiverBackend>&& transceiverBackend, Ref<RealtimeMediaSource>&& receiverSource)
{
    auto sender = RTCRtpSender::create(*this, receiverSource->type() == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s, Vector<String> { }, transceiverBackend->createSenderBackend(*this, nullptr));
    auto receiver = createReceiverForSource(WTFMove(receiverSource), transceiverBackend->createReceiverBackend());
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(transceiverBackend));
    m_peerConnection.addInternalTransceiver(transceiver.copyRef());
    return transceiver.get();
}

Ref<RTCRtpTransceiver> LibWebRTCPeerConnectionBackend::completeAddTransceiver(Ref<RTCRtpSender>&& sender, const RTCRtpTransceiverInit& init, const String& trackId, const String& trackKind)
{
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), createReceiver(trackKind, trackId), nullptr);

    transceiver->setDirection(init.direction);

    m_peerConnection.addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

void LibWebRTCPeerConnectionBackend::collectTransceivers()
{
    m_endpoint->collectTransceivers();
}

void LibWebRTCPeerConnectionBackend::removeTrack(RTCRtpSender& sender)
{
    m_endpoint->removeTrack(backendFromRTPSender(sender));
}

void LibWebRTCPeerConnectionBackend::applyRotationForOutgoingVideoSources()
{
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
        if (!transceiver->sender().isStopped()) {
            if (auto* videoSource = backendFromRTPSender(transceiver->sender()).videoSource())
                videoSource->setApplyRotation(true);
        }
    }
}

bool LibWebRTCPeerConnectionBackend::shouldOfferAllowToReceive(const char* kind) const
{
    ASSERT(!RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled());
    for (const auto& transceiver : m_peerConnection.currentTransceivers()) {
        if (transceiver->sender().trackKind() != kind)
            continue;

        if (transceiver->direction() == RTCRtpTransceiverDirection::Recvonly)
            return true;

        if (transceiver->direction() != RTCRtpTransceiverDirection::Sendrecv)
            continue;

        auto* backend = static_cast<LibWebRTCRtpSenderBackend*>(transceiver->sender().backend());
        if (backend && !backend->rtcSender())
            return true;
    }
    return false;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
