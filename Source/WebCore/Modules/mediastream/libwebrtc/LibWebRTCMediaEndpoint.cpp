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
#include "LibWebRTCMediaEndpoint.h"

#if USE(LIBWEBRTC)

#include "EventNames.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCProvider.h"
#include "MediaStreamEvent.h"
#include "NotImplemented.h"
#include "PlatformStrategies.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCSessionDescription.h"
#include "RTCTrackEvent.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"
#include <webrtc/api/peerconnectionfactory.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/p2p/base/basicpacketsocketfactory.h>
#include <webrtc/p2p/client/basicportallocator.h>
#include <wtf/MainThread.h>

#include "CoreMediaSoftLink.h"

namespace WebCore {

LibWebRTCMediaEndpoint::LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client)
    : m_peerConnectionBackend(peerConnection)
    , m_backend(client.createPeerConnection(*this))
    , m_createSessionDescriptionObserver(*this)
    , m_setLocalSessionDescriptionObserver(*this)
    , m_setRemoteSessionDescriptionObserver(*this)
{
    ASSERT(m_backend);
}

static inline const char* sessionDescriptionType(RTCSessionDescription::SdpType sdpType)
{
    switch (sdpType) {
    case RTCSessionDescription::SdpType::Offer:
        return "offer";
    case RTCSessionDescription::SdpType::Pranswer:
        return "pranswer";
    case RTCSessionDescription::SdpType::Answer:
        return "answer";
    case RTCSessionDescription::SdpType::Rollback:
        return "rollback";
    }
}

static inline RTCSessionDescription::SdpType fromSessionDescriptionType(const webrtc::SessionDescriptionInterface& description)
{
    auto type = description.type();
    if (type == webrtc::SessionDescriptionInterface::kOffer)
        return RTCSessionDescription::SdpType::Offer;
    if (type == webrtc::SessionDescriptionInterface::kAnswer)
        return RTCSessionDescription::SdpType::Answer;
    ASSERT(type == webrtc::SessionDescriptionInterface::kPrAnswer);
    return RTCSessionDescription::SdpType::Pranswer;
}

static inline RefPtr<RTCSessionDescription> fromSessionDescription(const webrtc::SessionDescriptionInterface* description)
{
    if (!description)
        return nullptr;

    std::string sdp;
    description->ToString(&sdp);
    String sdpString(sdp.data(), sdp.size());

    return RTCSessionDescription::create(fromSessionDescriptionType(*description), WTFMove(sdpString));
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::localDescription() const
{
    // FIXME: We might want to create a new object only if the session actually changed.
    return fromSessionDescription(m_backend->local_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::remoteDescription() const
{
    // FIXME: We might want to create a new object only if the session actually changed.
    return fromSessionDescription(m_backend->remote_description());
}

void LibWebRTCMediaEndpoint::doSetLocalDescription(RTCSessionDescription& description)
{
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));

    if (!sessionDescription) {
        String errorMessage(error.description.data(), error.description.size());
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, WTFMove(errorMessage) });
        return;
    }
    m_backend->SetLocalDescription(&m_setLocalSessionDescriptionObserver, sessionDescription.release());
}

void LibWebRTCMediaEndpoint::doSetRemoteDescription(RTCSessionDescription& description)
{
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));
    if (!sessionDescription) {
        String errorMessage(error.description.data(), error.description.size());
        m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { OperationError, WTFMove(errorMessage) });
        return;
    }
    m_backend->SetRemoteDescription(&m_setRemoteSessionDescriptionObserver, sessionDescription.release());
}

static inline std::string streamId(RTCPeerConnection& connection)
{
    auto& senders = connection.getSenders();
    if (senders.size()) {
        for (RTCRtpSender& sender : senders) {
            auto* track = sender.track();
            if (track) {
                ASSERT(sender.mediaStreamIds().size() == 1);
                return std::string(sender.mediaStreamIds().first().utf8().data());
            }
        }
    }
    return "av_label";
}

void LibWebRTCMediaEndpoint::doCreateOffer()
{
    m_isInitiator = true;
    auto& senders = m_peerConnectionBackend.connection().getSenders();
    if (senders.size()) {
        // FIXME: We only support one stream for the moment.
        auto stream = LibWebRTCProvider::factory().CreateLocalMediaStream(streamId(m_peerConnectionBackend.connection()));
        for (RTCRtpSender& sender : senders) {
            auto* track = sender.track();
            if (track) {
                ASSERT(sender.mediaStreamIds().size() == 1);
                auto& source = track->source();
                if (source.type() == RealtimeMediaSource::Audio) {
                    auto trackSource = RealtimeOutgoingAudioSource::create(source);
                    auto rtcTrack = LibWebRTCProvider::factory().CreateAudioTrack(track->id().utf8().data(), trackSource.ptr());
                    trackSource->setTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>(rtcTrack));
                    m_peerConnectionBackend.addAudioSource(WTFMove(trackSource));
                    stream->AddTrack(WTFMove(rtcTrack));
                } else {
                    auto videoSource = RealtimeOutgoingVideoSource::create(source);
                    auto videoTrack = LibWebRTCProvider::factory().CreateVideoTrack(track->id().utf8().data(), videoSource.ptr());
                    m_peerConnectionBackend.addVideoSource(WTFMove(videoSource));
                    stream->AddTrack(WTFMove(videoTrack));
                }
            }
        }
        m_backend->AddStream(stream);
    }
    m_backend->CreateOffer(&m_createSessionDescriptionObserver, nullptr);
}

void LibWebRTCMediaEndpoint::doCreateAnswer()
{
    m_isInitiator = false;

    auto& senders = m_peerConnectionBackend.connection().getSenders();
    if (senders.size()) {
        // FIXME: We only support one stream for the moment.
        auto stream = LibWebRTCProvider::factory().CreateLocalMediaStream(streamId(m_peerConnectionBackend.connection()));
        for (RTCRtpSender& sender : senders) {
            auto* track = sender.track();
            if (track) {
                ASSERT(sender.mediaStreamIds().size() == 1);
                auto& source = track->source();
                if (source.type() == RealtimeMediaSource::Audio) {
                    auto trackSource = RealtimeOutgoingAudioSource::create(source);
                    auto rtcTrack = LibWebRTCProvider::factory().CreateAudioTrack(track->id().utf8().data(), trackSource.ptr());
                    trackSource->setTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>(rtcTrack));
                    m_peerConnectionBackend.addAudioSource(WTFMove(trackSource));
                    stream->AddTrack(WTFMove(rtcTrack));
                } else {
                    auto videoSource = RealtimeOutgoingVideoSource::create(source);
                    auto videoTrack = LibWebRTCProvider::factory().CreateVideoTrack(track->id().utf8().data(), videoSource.ptr());
                    m_peerConnectionBackend.addVideoSource(WTFMove(videoSource));
                    stream->AddTrack(WTFMove(videoTrack));
                }
            }
        }
        m_backend->AddStream(stream);
    }
    m_backend->CreateAnswer(&m_createSessionDescriptionObserver, nullptr);
}

void LibWebRTCMediaEndpoint::getStats(MediaStreamTrack* track, const DeferredPromise& promise)
{
    m_backend->GetStats(StatsCollector::create(*this, promise, track).get());
}

LibWebRTCMediaEndpoint::StatsCollector::StatsCollector(LibWebRTCMediaEndpoint& endpoint, const DeferredPromise& promise, MediaStreamTrack* track)
    : m_endpoint(endpoint)
    , m_promise(promise)
{
    if (track)
        m_id = track->id();
}

void LibWebRTCMediaEndpoint::StatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
    callOnMainThread([protectedThis = rtc::scoped_refptr<LibWebRTCMediaEndpoint::StatsCollector>(this), report] {
        if (protectedThis->m_endpoint.isStopped())
            return;

        // FIXME: Fulfill promise with the report
        UNUSED_PARAM(report);

        protectedThis->m_endpoint.m_peerConnectionBackend.getStatsFailed(protectedThis->m_promise, Exception { TypeError, ASCIILiteral("Stats API is not yet implemented") });
    });
}

static PeerConnectionStates::SignalingState signalingState(webrtc::PeerConnectionInterface::SignalingState state)
{
    switch (state) {
    case webrtc::PeerConnectionInterface::kStable:
        return PeerConnectionStates::SignalingState::Stable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
        return PeerConnectionStates::SignalingState::HaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
        return PeerConnectionStates::SignalingState::HaveLocalPrAnswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
        return PeerConnectionStates::SignalingState::HaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
        return PeerConnectionStates::SignalingState::HaveRemotePrAnswer;
    case webrtc::PeerConnectionInterface::kClosed:
        return PeerConnectionStates::SignalingState::Closed;
    }
}

void LibWebRTCMediaEndpoint::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState rtcState)
{
    auto state = signalingState(rtcState);
    callOnMainThread([protectedThis = makeRef(*this), state] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.updateSignalingState(state);
    });
}

static inline String trackId(webrtc::MediaStreamTrackInterface& videoTrack)
{
    return String(videoTrack.id().data(), videoTrack.id().size());
}

static inline Ref<MediaStreamTrack> createMediaStreamTrack(ScriptExecutionContext& context, Ref<RealtimeMediaSource>&& remoteSource)
{
    String trackId = remoteSource->id();
    return MediaStreamTrack::create(context, MediaStreamTrackPrivate::create(WTFMove(remoteSource), WTFMove(trackId)));
}

void LibWebRTCMediaEndpoint::addStream(webrtc::MediaStreamInterface& stream)
{
    MediaStreamTrackVector tracks;
    for (auto& videoTrack : stream.GetVideoTracks()) {
        ASSERT(videoTrack);
        String id = trackId(*videoTrack);
        auto remoteSource = RealtimeIncomingVideoSource::create(WTFMove(videoTrack), WTFMove(id));
        tracks.append(createMediaStreamTrack(*m_peerConnectionBackend.connection().scriptExecutionContext(), WTFMove(remoteSource)));
    }
    for (auto& audioTrack : stream.GetAudioTracks()) {
        ASSERT(audioTrack);
        String id = trackId(*audioTrack);
        auto remoteSource = RealtimeIncomingAudioSource::create(WTFMove(audioTrack), WTFMove(id));
        tracks.append(createMediaStreamTrack(*m_peerConnectionBackend.connection().scriptExecutionContext(), WTFMove(remoteSource)));
    }

    auto newStream = MediaStream::create(*m_peerConnectionBackend.connection().scriptExecutionContext(), WTFMove(tracks));
    m_peerConnectionBackend.connection().fireEvent(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, newStream.copyRef()));

    Vector<RefPtr<MediaStream>> streams;
    streams.append(newStream.copyRef());
    for (auto& track : newStream->getTracks())
        m_peerConnectionBackend.connection().fireEvent(RTCTrackEvent::create(eventNames().trackEvent, false, false, nullptr, track.get(), Vector<RefPtr<MediaStream>>(streams), nullptr));
}

void LibWebRTCMediaEndpoint::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    callOnMainThread([protectedThis = makeRef(*this), stream = WTFMove(stream)] {
        if (protectedThis->isStopped())
            return;
        ASSERT(stream);
        protectedThis->addStream(*stream.get());
    });
}

void LibWebRTCMediaEndpoint::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>)
{
    notImplemented();
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    webrtc::DataChannelInit init;
    init.ordered = options.ordered;
    init.maxRetransmitTime = options.maxRetransmitTime;
    init.maxRetransmits = options.maxRetransmits;
    init.protocol = options.protocol.utf8().data();
    init.negotiated = options.negotiated;
    init.id = options.id;

    return std::make_unique<LibWebRTCDataChannelHandler>(m_backend->CreateDataChannel(label.utf8().data(), &init));
}

void LibWebRTCMediaEndpoint::addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>&& dataChannel)
{
    auto protocol = dataChannel->protocol();
    auto label = dataChannel->label();

    RTCDataChannelInit init;
    init.ordered = dataChannel->ordered();
    init.maxRetransmitTime = dataChannel->maxRetransmitTime();
    init.maxRetransmits = dataChannel->maxRetransmits();
    init.protocol = String(protocol.data(), protocol.size());
    init.negotiated = dataChannel->negotiated();
    init.id = dataChannel->id();

    bool isOpened = dataChannel->state() == webrtc::DataChannelInterface::kOpen;

    auto handler =  std::make_unique<LibWebRTCDataChannelHandler>(WTFMove(dataChannel));
    ASSERT(m_peerConnectionBackend.connection().scriptExecutionContext());
    auto channel = RTCDataChannel::create(*m_peerConnectionBackend.connection().scriptExecutionContext(), WTFMove(handler), String(label.data(), label.size()), WTFMove(init));

    if (isOpened) {
        callOnMainThread([channel = channel.copyRef()] {
            // FIXME: We should be able to write channel->didChangeReadyState(...)
            RTCDataChannelHandlerClient& client = channel.get();
            client.didChangeReadyState(RTCDataChannel::ReadyStateOpen);
        });
    }

    m_peerConnectionBackend.connection().fireEvent(RTCDataChannelEvent::create(eventNames().datachannelEvent, false, false, WTFMove(channel)));
}

void LibWebRTCMediaEndpoint::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{
    callOnMainThread([protectedThis = makeRef(*this), dataChannel = WTFMove(dataChannel)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>(dataChannel));
    });
}

void LibWebRTCMediaEndpoint::stop()
{
    ASSERT(m_backend);
    m_backend->Close();
    m_backend = nullptr;
}

void LibWebRTCMediaEndpoint::OnRenegotiationNeeded()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.markAsNeedingNegotiation();
    });
}

static inline PeerConnectionStates::IceConnectionState iceConnectionState(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    switch (state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
        return PeerConnectionStates::IceConnectionState::New;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
        return PeerConnectionStates::IceConnectionState::Checking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
        return PeerConnectionStates::IceConnectionState::Connected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
        return PeerConnectionStates::IceConnectionState::Completed;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
        return PeerConnectionStates::IceConnectionState::Failed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
        return PeerConnectionStates::IceConnectionState::Disconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
        return PeerConnectionStates::IceConnectionState::Closed;
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
        ASSERT_NOT_REACHED();
        return PeerConnectionStates::IceConnectionState::New;
    }
}

void LibWebRTCMediaEndpoint::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    auto connectionState = iceConnectionState(state);
    callOnMainThread([protectedThis = makeRef(*this), connectionState] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_peerConnectionBackend.connection().internalIceConnectionState() != connectionState)
            protectedThis->m_peerConnectionBackend.connection().updateIceConnectionState(connectionState);
    });
}

void LibWebRTCMediaEndpoint::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state)
{
    if (state == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
        callOnMainThread([protectedThis = makeRef(*this)] {
            if (protectedThis->isStopped())
                return;
            protectedThis->m_peerConnectionBackend.doneGatheringCandidates();
        });
    }
}

void LibWebRTCMediaEndpoint::OnIceCandidate(const webrtc::IceCandidateInterface *rtcCandidate)
{
    ASSERT(rtcCandidate);

    std::string sdp;
    rtcCandidate->ToString(&sdp);
    String candidateSDP(sdp.data(), sdp.size());

    auto mid = rtcCandidate->sdp_mid();
    String candidateMid(mid.data(), mid.size());

    callOnMainThread([protectedThis = makeRef(*this), mid = WTFMove(candidateMid), sdp = WTFMove(candidateSDP)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.fireICECandidateEvent(RTCIceCandidate::create(String(sdp), String(mid), 0));
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&)
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCMediaEndpoint::createSessionDescriptionSucceeded(webrtc::SessionDescriptionInterface* description)
{
    std::string sdp;
    description->ToString(&sdp);
    String sdpString(sdp.data(), sdp.size());

    callOnMainThread([protectedThis = makeRef(*this), sdp = WTFMove(sdpString)] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferSucceeded(String(sdp));
        else
            protectedThis->m_peerConnectionBackend.createAnswerSucceeded(String(sdp));
    });
}

void LibWebRTCMediaEndpoint::createSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferFailed(Exception { OperationError, String(error) });
        else
            protectedThis->m_peerConnectionBackend.createAnswerFailed(Exception { OperationError, String(error) });
    });
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionSucceeded()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setLocalDescriptionSucceeded();
    });
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, String(error) });
    });
}

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionSucceeded()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setRemoteDescriptionSucceeded();
    });
}

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { OperationError, String(error) });
    });
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
