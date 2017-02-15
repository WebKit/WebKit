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
    webrtc::PeerConnectionInterface::RTCConfiguration rtcConfiguration(webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

    if (configuration.iceTransportPolicy == PeerConnectionStates::IceTransportPolicy::Relay)
        rtcConfiguration.type = webrtc::PeerConnectionInterface::kRelay;

    if (configuration.bundlePolicy == PeerConnectionStates::BundlePolicy::MaxBundle)
        rtcConfiguration.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
    else if (configuration.bundlePolicy == PeerConnectionStates::BundlePolicy::MaxCompat)
        rtcConfiguration.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;

    for (auto& server : configuration.iceServers) {
        webrtc::PeerConnectionInterface::IceServer iceServer;
        iceServer.username = server.username.utf8().data();
        iceServer.password = server.credential.utf8().data();
        for (auto& url : server.urls)
            iceServer.urls.push_back({ url.string().utf8().data() });
        rtcConfiguration.servers.push_back(WTFMove(iceServer));
    }

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
    m_endpoint->stop();
}

void LibWebRTCPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate)
{
    if (!m_isRemoteDescriptionSet) {
        addIceCandidateFailed(Exception { INVALID_STATE_ERR, "No remote description set" });
        return;
    }

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

Ref<RTCRtpReceiver> LibWebRTCPeerConnectionBackend::createReceiver(const String&, const String& trackKind, const String& trackId)
{
    // FIXME: We need to create a source that will get fueled once we will receive OnAddStream.
    // For the moment, we create an empty one.
    auto remoteTrackPrivate = (trackKind == "audio") ? MediaStreamTrackPrivate::create(RealtimeIncomingAudioSource::create(nullptr, String(trackId))) : MediaStreamTrackPrivate::create(RealtimeIncomingVideoSource::create(nullptr, String(trackId)));
    auto remoteTrack = MediaStreamTrack::create(*m_peerConnection.scriptExecutionContext(), WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(WTFMove(remoteTrack));
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCPeerConnectionBackend::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_endpoint->createDataChannel(label, options);
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::localDescription() const
{
    return m_endpoint->localDescription();
}

RefPtr<RTCSessionDescription> LibWebRTCPeerConnectionBackend::remoteDescription() const
{
    return m_endpoint->remoteDescription();
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
