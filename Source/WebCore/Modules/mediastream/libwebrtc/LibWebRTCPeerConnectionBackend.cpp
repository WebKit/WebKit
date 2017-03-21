/*
 * Copyright (C) 2017 Apple Inc.
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
#include "JSRTCStatsReport.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCMediaEndpoint.h"
#include "MediaEndpointConfiguration.h"
#include "Page.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCRtpReceiver.h"
#include "RTCSessionDescription.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"

namespace WebCore {

static std::unique_ptr<PeerConnectionBackend> createLibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection)
{
    if (!LibWebRTCProvider::webRTCAvailable())
        return nullptr;
    return std::make_unique<LibWebRTCPeerConnectionBackend>(peerConnection);
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createLibWebRTCPeerConnectionBackend;

static inline LibWebRTCProvider& libWebRTCProvider(RTCPeerConnection& peerConnection)
{
    ASSERT(peerConnection.scriptExecutionContext()->isDocument());
    auto* page = static_cast<Document*>(peerConnection.scriptExecutionContext())->page();
    return page->libWebRTCProvider();
}

LibWebRTCPeerConnectionBackend::LibWebRTCPeerConnectionBackend(RTCPeerConnection& peerConnection)
    : PeerConnectionBackend(peerConnection)
    , m_endpoint(LibWebRTCMediaEndpoint::create(*this, libWebRTCProvider(peerConnection)))
{
}

LibWebRTCPeerConnectionBackend::~LibWebRTCPeerConnectionBackend()
{
}

static webrtc::PeerConnectionInterface::RTCConfiguration configurationFromMediaEndpointConfiguration(MediaEndpointConfiguration&& configuration)
{
    webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration;

    if (configuration.iceTransportPolicy == RTCIceTransportPolicy::Relay)
        rtcConfiguration.type = webrtc::PeerConnectionInterface::kRelay;

    // FIXME: Support PeerConnectionStates::BundlePolicy::MaxBundle.
    // LibWebRTC does not like it and will fail to set any configuration field otherwise.
    // See https://bugs.webkit.org/show_bug.cgi?id=169389.

    if (configuration.bundlePolicy == RTCBundlePolicy::MaxCompat)
        rtcConfiguration.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;

    for (auto& server : configuration.iceServers) {
        webrtc::PeerConnectionInterface::IceServer iceServer;
        iceServer.username = server.username.utf8().data();
        iceServer.password = server.credential.utf8().data();
        for (auto& url : server.urls)
            iceServer.urls.push_back({ url.string().utf8().data() });
        rtcConfiguration.servers.push_back(WTFMove(iceServer));
    }

    rtcConfiguration.ice_candidate_pool_size = configuration.iceCandidatePoolSize;

    return rtcConfiguration;
}

void LibWebRTCPeerConnectionBackend::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    m_endpoint->backend().SetConfiguration(configurationFromMediaEndpointConfiguration(WTFMove(configuration)));
}

void LibWebRTCPeerConnectionBackend::getStats(MediaStreamTrack* track, Ref<DeferredPromise>&& promise)
{
    if (m_endpoint->isStopped())
        return;

    auto& statsPromise = promise.get();
    m_statsPromises.add(&statsPromise, WTFMove(promise));
    m_endpoint->getStats(track, statsPromise);
}

void LibWebRTCPeerConnectionBackend::getStatsSucceeded(const DeferredPromise& promise, Ref<RTCStatsReport>&& report)
{
    auto statsPromise = m_statsPromises.take(&promise);
    ASSERT(statsPromise);
    statsPromise.value()->resolve<IDLInterface<RTCStatsReport>>(WTFMove(report));
}

void LibWebRTCPeerConnectionBackend::getStatsFailed(const DeferredPromise& promise, Exception&& exception)
{
    auto statsPromise = m_statsPromises.take(&promise);
    ASSERT(statsPromise);
    statsPromise.value()->reject(WTFMove(exception));
}

void LibWebRTCPeerConnectionBackend::doSetLocalDescription(RTCSessionDescription& description)
{
    m_endpoint->doSetLocalDescription(description);
    if (!m_isLocalDescriptionSet) {
        if (m_isRemoteDescriptionSet) {
            while (m_pendingCandidates.size())
                m_endpoint->addIceCandidate(*m_pendingCandidates.takeLast().release());
        }
        m_isLocalDescriptionSet = true;
    }
}

void LibWebRTCPeerConnectionBackend::doSetRemoteDescription(RTCSessionDescription& description)
{
    m_endpoint->doSetRemoteDescription(description);
    if (!m_isRemoteDescriptionSet) {
        if (m_isLocalDescriptionSet) {
            while (m_pendingCandidates.size())
                m_endpoint->addIceCandidate(*m_pendingCandidates.takeLast().release());
        }
        m_isRemoteDescriptionSet = true;
    }
}

void LibWebRTCPeerConnectionBackend::doCreateOffer(RTCOfferOptions&&)
{
    m_endpoint->doCreateOffer();
}

void LibWebRTCPeerConnectionBackend::doCreateAnswer(RTCAnswerOptions&&)
{
    if (!m_isRemoteDescriptionSet) {
        createAnswerFailed(Exception { INVALID_STATE_ERR, "No remote description set" });
        return;
    }
    m_endpoint->doCreateAnswer();
}

void LibWebRTCPeerConnectionBackend::doStop()
{
    for (auto& source : m_audioSources)
        source->stop();
    for (auto& source : m_videoSources)
        source->stop();

    m_endpoint->stop();

    m_remoteStreams.clear();
    m_pendingReceivers.clear();
}

void LibWebRTCPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate)
{
    webrtc::SdpParseError error;
    int sdpMLineIndex = candidate.sdpMLineIndex() ? candidate.sdpMLineIndex().value() : 0;
    std::unique_ptr<webrtc::IceCandidateInterface> rtcCandidate(webrtc::CreateIceCandidate(candidate.sdpMid().utf8().data(), sdpMLineIndex, candidate.candidate().utf8().data(), &error));

    if (!rtcCandidate) {
        String message(error.description.data(), error.description.size());
        addIceCandidateFailed(Exception { OperationError, WTFMove(message) });
        return;
    }

    // libwebrtc does not like that ice candidates are set before the description.
    if (!m_isLocalDescriptionSet || !m_isRemoteDescriptionSet)
        m_pendingCandidates.append(WTFMove(rtcCandidate));
    else if (!m_endpoint->addIceCandidate(*rtcCandidate.get())) {
        ASSERT_NOT_REACHED();
        addIceCandidateFailed(Exception { OperationError, ASCIILiteral("Failed to apply the received candidate") });
        return;
    }
    addIceCandidateSucceeded();
}

void LibWebRTCPeerConnectionBackend::addAudioSource(Ref<RealtimeOutgoingAudioSource>&& source)
{
    m_audioSources.append(WTFMove(source));
}

void LibWebRTCPeerConnectionBackend::addVideoSource(Ref<RealtimeOutgoingVideoSource>&& source)
{
    m_videoSources.append(WTFMove(source));
}

static inline Ref<RTCRtpReceiver> createReceiverForSource(ScriptExecutionContext& context, Ref<RealtimeMediaSource>&& source)
{
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(WTFMove(source));
    auto remoteTrack = MediaStreamTrack::create(context, WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(WTFMove(remoteTrack));
}

static inline Ref<RealtimeMediaSource> createEmptySource(const String& trackKind, String&& trackId)
{
    // FIXME: trackKind should be an enumeration
    if (trackKind == "audio")
        return RealtimeIncomingAudioSource::create(nullptr, WTFMove(trackId));
    ASSERT(trackKind == "video");
    return RealtimeIncomingVideoSource::create(nullptr, WTFMove(trackId));
}

Ref<RTCRtpReceiver> LibWebRTCPeerConnectionBackend::createReceiver(const String&, const String& trackKind, const String& trackId)
{
    auto receiver = createReceiverForSource(*m_peerConnection.scriptExecutionContext(), createEmptySource(trackKind, String(trackId)));
    m_pendingReceivers.append(receiver.copyRef());
    return receiver;
}

LibWebRTCPeerConnectionBackend::VideoReceiver LibWebRTCPeerConnectionBackend::videoReceiver(String&& trackId)
{
    // FIXME: Add to Vector a utility routine for that take-or-create pattern.
    // FIXME: We should be selecting the receiver based on track id.
    for (size_t cptr = 0; cptr < m_pendingReceivers.size(); ++cptr) {
        if (m_pendingReceivers[cptr]->track()->source().type() == RealtimeMediaSource::Type::Video) {
            Ref<RTCRtpReceiver> receiver = m_pendingReceivers[cptr].copyRef();
            m_pendingReceivers.remove(cptr);
            Ref<RealtimeIncomingVideoSource> source = static_cast<RealtimeIncomingVideoSource&>(receiver->track()->source());
            return { WTFMove(receiver), WTFMove(source) };
        }
    }
    auto source = RealtimeIncomingVideoSource::create(nullptr, WTFMove(trackId));
    auto receiver = createReceiverForSource(*m_peerConnection.scriptExecutionContext(), source.copyRef());
    return { WTFMove(receiver), WTFMove(source) };
}

LibWebRTCPeerConnectionBackend::AudioReceiver LibWebRTCPeerConnectionBackend::audioReceiver(String&& trackId)
{
    // FIXME: Add to Vector a utility routine for that take-or-create pattern.
    // FIXME: We should be selecting the receiver based on track id.
    for (size_t cptr = 0; cptr < m_pendingReceivers.size(); ++cptr) {
        if (m_pendingReceivers[cptr]->track()->source().type() == RealtimeMediaSource::Type::Audio) {
            Ref<RTCRtpReceiver> receiver = m_pendingReceivers[cptr].copyRef();
            m_pendingReceivers.remove(cptr);
            Ref<RealtimeIncomingAudioSource> source = static_cast<RealtimeIncomingAudioSource&>(receiver->track()->source());
            return { WTFMove(receiver), WTFMove(source) };
        }
    }
    auto source = RealtimeIncomingAudioSource::create(nullptr, WTFMove(trackId));
    auto receiver = createReceiverForSource(*m_peerConnection.scriptExecutionContext(), source.copyRef());
    return { WTFMove(receiver), WTFMove(source) };
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCPeerConnectionBackend::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_endpoint->createDataChannel(label, options);
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::currentLocalDescription() const
{
    return m_endpoint->currentLocalDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::currentRemoteDescription() const
{
    return m_endpoint->currentRemoteDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::pendingLocalDescription() const
{
    return m_endpoint->pendingLocalDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::pendingRemoteDescription() const
{
    return m_endpoint->pendingRemoteDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::localDescription() const
{
    return m_endpoint->localDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::remoteDescription() const
{
    return m_endpoint->remoteDescription();
}

void LibWebRTCPeerConnectionBackend::notifyAddedTrack(RTCRtpSender& sender)
{
    ASSERT(sender.track());
    m_endpoint->addTrack(*sender.track(), sender.mediaStreamIds());
}

void LibWebRTCPeerConnectionBackend::removeRemoteStream(MediaStream* mediaStream)
{
    m_remoteStreams.removeFirstMatching([mediaStream](const auto& item) {
        return item.get() == mediaStream;
    });
}

void LibWebRTCPeerConnectionBackend::addRemoteStream(Ref<MediaStream>&& mediaStream)
{
    m_remoteStreams.append(WTFMove(mediaStream));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
