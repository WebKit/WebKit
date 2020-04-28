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
#include "LibWebRTCMediaEndpoint.h"

#if USE(LIBWEBRTC)

#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCStatsReport.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCRtpTransceiverBackend.h"
#include "LibWebRTCStatsCollector.h"
#include "LibWebRTCUtils.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "Performance.h"
#include "PlatformStrategies.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include "RTCOfferOptions.h"
#include "RTCPeerConnection.h"
#include "RTCSessionDescription.h"
#include "RTCStatsReport.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"
#include "RealtimeOutgoingAudioSource.h"
#include "RealtimeOutgoingVideoSource.h"
#include "RuntimeEnabledFeatures.h"
#include <webrtc/rtc_base/physical_socket_server.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/p2p/client/basic_port_allocator.h>
#include <webrtc/pc/peer_connection_factory.h>
#include <webrtc/system_wrappers/include/field_trial.h>
#include <wtf/MainThread.h>

namespace WebCore {

LibWebRTCMediaEndpoint::LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client)
    : m_peerConnectionBackend(peerConnection)
    , m_peerConnectionFactory(*client.factory())
    , m_createSessionDescriptionObserver(*this)
    , m_setLocalSessionDescriptionObserver(*this)
    , m_setRemoteSessionDescriptionObserver(*this)
    , m_statsLogTimer(*this, &LibWebRTCMediaEndpoint::gatherStatsForLogging)
#if !RELEASE_LOG_DISABLED
    , m_logger(peerConnection.logger())
    , m_logIdentifier(peerConnection.logIdentifier())
#endif
{
    ASSERT(isMainThread());
    ASSERT(client.factory());

    if (RuntimeEnabledFeatures::sharedFeatures().webRTCH264SimulcastEnabled())
        webrtc::field_trial::InitFieldTrialsFromString("WebRTC-H264Simulcast/Enabled/");
}

bool LibWebRTCMediaEndpoint::setConfiguration(LibWebRTCProvider& client, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

    if (!m_backend) {
        if (!m_rtcSocketFactory) {
            auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
            m_rtcSocketFactory = client.createSocketFactory(document.userAgent(document.url()));
        }
        m_backend = client.createPeerConnection(*this, m_rtcSocketFactory.get(), WTFMove(configuration));
        return !!m_backend;
    }
    auto oldConfiguration = m_backend->GetConfiguration();
    configuration.certificates = oldConfiguration.certificates;
    return m_backend->SetConfiguration(WTFMove(configuration)).ok();
}

void LibWebRTCMediaEndpoint::suspend()
{
    if (m_rtcSocketFactory)
        m_rtcSocketFactory->suspend();
}

void LibWebRTCMediaEndpoint::resume()
{
    if (m_rtcSocketFactory)
        m_rtcSocketFactory->resume();
}

static inline const char* sessionDescriptionType(RTCSdpType sdpType)
{
    switch (sdpType) {
    case RTCSdpType::Offer:
        return "offer";
    case RTCSdpType::Pranswer:
        return "pranswer";
    case RTCSdpType::Answer:
        return "answer";
    case RTCSdpType::Rollback:
        return "rollback";
    }

    ASSERT_NOT_REACHED();
    return "";
}

static inline RTCSdpType fromSessionDescriptionType(const webrtc::SessionDescriptionInterface& description)
{
    auto type = description.type();
    if (type == webrtc::SessionDescriptionInterface::kOffer)
        return RTCSdpType::Offer;
    if (type == webrtc::SessionDescriptionInterface::kAnswer)
        return RTCSdpType::Answer;
    ASSERT(type == webrtc::SessionDescriptionInterface::kPrAnswer);
    return RTCSdpType::Pranswer;
}

static inline RefPtr<RTCSessionDescription> fromSessionDescription(const webrtc::SessionDescriptionInterface* description)
{
    if (!description)
        return nullptr;

    std::string sdp;
    description->ToString(&sdp);

    return RTCSessionDescription::create(fromSessionDescriptionType(*description), fromStdString(sdp));
}

// FIXME: We might want to create a new object only if the session actually changed for all description getters.
RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::currentLocalDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->current_local_description()) : nullptr;
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::currentRemoteDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->current_remote_description()) : nullptr;
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::pendingLocalDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->pending_local_description()) : nullptr;
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::pendingRemoteDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->pending_remote_description()) : nullptr;
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::localDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->local_description()) : nullptr;
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::remoteDescription() const
{
    return m_backend ? fromSessionDescription(m_backend->remote_description()) : nullptr;
}

void LibWebRTCMediaEndpoint::doSetLocalDescription(RTCSessionDescription& description)
{
    ASSERT(m_backend);

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));

    if (!sessionDescription) {
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, fromStdString(error.description) });
        return;
    }

    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=173783. Remove this test once fixed at LibWebRTC level.
    if (description.type() == RTCSdpType::Answer && !m_backend->pending_remote_description()) {
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
        return;
    }

    m_backend->SetLocalDescription(&m_setLocalSessionDescriptionObserver, sessionDescription.release());
}

void LibWebRTCMediaEndpoint::doSetRemoteDescription(RTCSessionDescription& description)
{
    ASSERT(m_backend);

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));
    if (!sessionDescription) {
        m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { SyntaxError, fromStdString(error.description) });
        return;
    }
    m_backend->SetRemoteDescription(&m_setRemoteSessionDescriptionObserver, sessionDescription.release());

    startLoggingStats();
}

bool LibWebRTCMediaEndpoint::addTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track, const Vector<String>& mediaStreamIds)
{
    ASSERT(m_backend);

    LibWebRTCRtpSenderBackend::Source source;
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory.CreateAudioTrack(track.id().utf8().data(), audioSource.ptr());
        source = WTFMove(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory.CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        source = WTFMove(videoSource);
        break;
    }
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
        return false;
    }

    sender.setSource(WTFMove(source));
    if (auto rtpSender = sender.rtcSender()) {
        rtpSender->SetTrack(rtcTrack.get());
        return true;
    }

    std::vector<std::string> ids;
    for (auto& id : mediaStreamIds)
        ids.push_back(id.utf8().data());

    auto newRTPSender = m_backend->AddTrack(rtcTrack.get(), WTFMove(ids));
    if (!newRTPSender.ok())
        return false;
    sender.setRTCSender(newRTPSender.MoveValue());
    return true;
}

void LibWebRTCMediaEndpoint::removeTrack(LibWebRTCRtpSenderBackend& sender)
{
    ASSERT(m_backend);
    m_backend->RemoveTrack(sender.rtcSender());
    sender.clearSource();
}

void LibWebRTCMediaEndpoint::doCreateOffer(const RTCOfferOptions& options)
{
    ASSERT(m_backend);

    m_isInitiator = true;
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcOptions;
    rtcOptions.ice_restart = options.iceRestart;
    rtcOptions.voice_activity_detection = options.voiceActivityDetection;

    m_backend->CreateOffer(&m_createSessionDescriptionObserver, rtcOptions);
}

void LibWebRTCMediaEndpoint::doCreateAnswer()
{
    ASSERT(m_backend);

    m_isInitiator = false;
    m_backend->CreateAnswer(&m_createSessionDescriptionObserver, { });
}

rtc::scoped_refptr<LibWebRTCStatsCollector> LibWebRTCMediaEndpoint::createStatsCollector(Ref<DeferredPromise>&& promise)
{
    return LibWebRTCStatsCollector::create([promise = WTFMove(promise), protectedThis = makeRef(*this)](auto&& report) mutable {
        ASSERT(isMainThread());
        if (protectedThis->isStopped() || !report)
            return;

        promise->resolve<IDLInterface<RTCStatsReport>>(report.releaseNonNull());
    });
}

void LibWebRTCMediaEndpoint::getStats(Ref<DeferredPromise>&& promise)
{
    if (m_backend)
        m_backend->GetStats(createStatsCollector(WTFMove(promise)));
}

void LibWebRTCMediaEndpoint::getStats(webrtc::RtpReceiverInterface& receiver, Ref<DeferredPromise>&& promise)
{
    if (m_backend)
        m_backend->GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface>(&receiver), createStatsCollector(WTFMove(promise)));
}

void LibWebRTCMediaEndpoint::getStats(webrtc::RtpSenderInterface& sender, Ref<DeferredPromise>&& promise)
{
    if (m_backend)
        m_backend->GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface>(&sender), createStatsCollector(WTFMove(promise)));
}

static RTCSignalingState signalingState(webrtc::PeerConnectionInterface::SignalingState state)
{
    switch (state) {
    case webrtc::PeerConnectionInterface::kStable:
        return RTCSignalingState::Stable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
        return RTCSignalingState::HaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
        return RTCSignalingState::HaveLocalPranswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
        return RTCSignalingState::HaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
        return RTCSignalingState::HaveRemotePranswer;
    case webrtc::PeerConnectionInterface::kClosed:
        return RTCSignalingState::Stable;
    }

    ASSERT_NOT_REACHED();
    return RTCSignalingState::Stable;
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

MediaStream& LibWebRTCMediaEndpoint::mediaStreamFromRTCStream(webrtc::MediaStreamInterface& rtcStream)
{
    auto label = fromStdString(rtcStream.id());
    auto mediaStream = m_remoteStreamsById.ensure(label, [label, this]() mutable {
        auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
        return MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }, WTFMove(label)));
    });
    return *mediaStream.iterator->value;
}

void LibWebRTCMediaEndpoint::addPendingTrackEvent(Ref<RTCRtpReceiver>&& receiver, MediaStreamTrack& track, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& rtcStreams, RefPtr<RTCRtpTransceiver>&& transceiver)
{
    Vector<RefPtr<MediaStream>> streams;
    for (auto& rtcStream : rtcStreams) {
        auto& mediaStream = mediaStreamFromRTCStream(*rtcStream.get());
        streams.append(&mediaStream);
        mediaStream.addTrackFromPlatform(track);
    }
    auto streamIds = WTF::map(streams, [](auto& stream) -> String {
        return stream->id();
    });
    m_remoteStreamsFromRemoteTrack.add(&track, WTFMove(streamIds));

    m_peerConnectionBackend.addPendingTrackEvent({ WTFMove(receiver), makeRef(track), WTFMove(streams), WTFMove(transceiver) });
}

void LibWebRTCMediaEndpoint::collectTransceivers()
{
    if (!m_backend)
        return;

    for (auto& rtcTransceiver : m_backend->GetTransceivers()) {
        auto* existingTransceiver = m_peerConnectionBackend.existingTransceiver([&](auto& transceiverBackend) {
            return rtcTransceiver.get() == transceiverBackend.rtcTransceiver();
        });
        if (existingTransceiver)
            continue;

        auto rtcReceiver = rtcTransceiver->receiver();
        m_peerConnectionBackend.newRemoteTransceiver(makeUnique<LibWebRTCRtpTransceiverBackend>(WTFMove(rtcTransceiver)), rtcReceiver->media_type() == cricket::MEDIA_TYPE_AUDIO ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);
    }
}

void LibWebRTCMediaEndpoint::newTransceiver(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>&& rtcTransceiver)
{
    auto rtcReceiver = rtcTransceiver->receiver();
    auto* transceiver = m_peerConnectionBackend.existingTransceiver([&](auto& transceiverBackend) {
        return rtcTransceiver.get() == transceiverBackend.rtcTransceiver();
    });
    if (!transceiver)
        transceiver = &m_peerConnectionBackend.newRemoteTransceiver(makeUnique<LibWebRTCRtpTransceiverBackend>(WTFMove(rtcTransceiver)), rtcReceiver->media_type() == cricket::MEDIA_TYPE_AUDIO ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);

    addPendingTrackEvent(makeRef(transceiver->receiver()), transceiver->receiver().track(), rtcReceiver->streams(), makeRef(*transceiver));
}

void LibWebRTCMediaEndpoint::removeRemoteTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>&& receiver)
{
    auto* transceiver = m_peerConnectionBackend.existingTransceiver([&receiver](auto& transceiverBackend) {
        auto* rtcTransceiver = transceiverBackend.rtcTransceiver();
        return rtcTransceiver && receiver.get() == rtcTransceiver->receiver().get();
    });
    if (!transceiver)
        return;

    auto& track = transceiver->receiver().track();

    for (auto& id : m_remoteStreamsFromRemoteTrack.get(&track)) {
        if (auto stream = m_remoteStreamsById.get(id))
            stream->privateStream().removeTrack(track.privateTrack());
    }

    track.source().setMuted(true);
}

template<typename T>
Optional<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::createTransceiverBackends(T&& trackOrKind, const RTCRtpTransceiverInit& init, LibWebRTCRtpSenderBackend::Source&& source)
{
    auto result = m_backend->AddTransceiver(WTFMove(trackOrKind), fromRtpTransceiverInit(init));
    if (!result.ok())
        return WTF::nullopt;

    auto transceiver = makeUnique<LibWebRTCRtpTransceiverBackend>(result.MoveValue());
    return LibWebRTCMediaEndpoint::Backends { transceiver->createSenderBackend(m_peerConnectionBackend, WTFMove(source)), transceiver->createReceiverBackend(), WTFMove(transceiver) };
}

Optional<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init)
{
    auto type = trackKind == "audio" ? cricket::MediaType::MEDIA_TYPE_AUDIO : cricket::MediaType::MEDIA_TYPE_VIDEO;
    return createTransceiverBackends(type, init, nullptr);
}

std::pair<LibWebRTCRtpSenderBackend::Source, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>> LibWebRTCMediaEndpoint::createSourceAndRTCTrack(MediaStreamTrack& track)
{
    LibWebRTCRtpSenderBackend::Source source;
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
        break;
    case RealtimeMediaSource::Type::Audio: {
        auto audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory.CreateAudioTrack(track.id().utf8().data(), audioSource.ptr());
        source = WTFMove(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory.CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        source = WTFMove(videoSource);
        break;
    }
    }
    return std::make_pair(WTFMove(source), WTFMove(rtcTrack));
}

Optional<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(MediaStreamTrack& track, const RTCRtpTransceiverInit& init)
{
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    return createTransceiverBackends(WTFMove(sourceAndTrack.second), init, WTFMove(sourceAndTrack.first));
}

void LibWebRTCMediaEndpoint::setSenderSourceFromTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track)
{
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    sender.setSource(WTFMove(sourceAndTrack.first));
    sender.rtcSender()->SetTrack(WTFMove(sourceAndTrack.second));
}

std::unique_ptr<LibWebRTCRtpTransceiverBackend> LibWebRTCMediaEndpoint::transceiverBackendFromSender(LibWebRTCRtpSenderBackend& backend)
{
    for (auto& transceiver : m_backend->GetTransceivers()) {
        if (transceiver->sender().get() == backend.rtcSender())
            return makeUnique<LibWebRTCRtpTransceiverBackend>(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>(transceiver));
    }
    return nullptr;
}

void LibWebRTCMediaEndpoint::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{
    callOnMainThread([protectedThis = makeRef(*this), transceiver = WTFMove(transceiver)]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->newTransceiver(WTFMove(transceiver));
    });
}

void LibWebRTCMediaEndpoint::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
    callOnMainThread([protectedThis = makeRef(*this), receiver = WTFMove(receiver)]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->removeRemoteTrack(WTFMove(receiver));
    });
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    auto init = LibWebRTCDataChannelHandler::fromRTCDataChannelInit(options);
    auto channel = m_backend->CreateDataChannel(label.utf8().data(), &init);
    return channel ? makeUnique<LibWebRTCDataChannelHandler>(WTFMove(channel)) : nullptr;
}

void LibWebRTCMediaEndpoint::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{
    callOnMainThread([protectedThis = makeRef(*this), dataChannel = WTFMove(dataChannel)]() mutable {
        if (protectedThis->isStopped())
            return;
        auto& connection = protectedThis->m_peerConnectionBackend.connection();
        connection.dispatchEventWhenFeasible(LibWebRTCDataChannelHandler::channelEvent(*connection.document(), WTFMove(dataChannel)));
    });
}

void LibWebRTCMediaEndpoint::close()
{
    m_backend->Close();
    stopLoggingStats();
}

void LibWebRTCMediaEndpoint::stop()
{
    if (!m_backend)
        return;

    stopLoggingStats();

    m_backend->Close();
    m_backend = nullptr;
    m_remoteStreamsById.clear();
    m_remoteStreamsFromRemoteTrack.clear();
}

void LibWebRTCMediaEndpoint::OnRenegotiationNeeded()
{
    callOnMainThread([protectedThis = makeRef(*this)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.markAsNeedingNegotiation();
    });
}

static inline RTCIceConnectionState toRTCIceConnectionState(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    switch (state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
        return RTCIceConnectionState::New;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
        return RTCIceConnectionState::Checking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
        return RTCIceConnectionState::Connected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
        return RTCIceConnectionState::Completed;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
        return RTCIceConnectionState::Failed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
        return RTCIceConnectionState::Disconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
        return RTCIceConnectionState::Closed;
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
        break;
    }

    ASSERT_NOT_REACHED();
    return RTCIceConnectionState::New;
}

void LibWebRTCMediaEndpoint::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    auto connectionState = toRTCIceConnectionState(state);
    callOnMainThread([protectedThis = makeRef(*this), connectionState] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_peerConnectionBackend.connection().iceConnectionState() != connectionState)
            protectedThis->m_peerConnectionBackend.connection().updateIceConnectionState(connectionState);
    });
}

void LibWebRTCMediaEndpoint::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state)
{
    callOnMainThread([protectedThis = makeRef(*this), state] {
        if (protectedThis->isStopped())
            return;
        if (state == webrtc::PeerConnectionInterface::kIceGatheringComplete)
            protectedThis->m_peerConnectionBackend.doneGatheringCandidates();
        else if (state == webrtc::PeerConnectionInterface::kIceGatheringGathering)
            protectedThis->m_peerConnectionBackend.connection().updateIceGatheringState(RTCIceGatheringState::Gathering);
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidate(const webrtc::IceCandidateInterface *rtcCandidate)
{
    ASSERT(rtcCandidate);

    std::string sdp;
    rtcCandidate->ToString(&sdp);

    auto sdpMLineIndex = safeCast<unsigned short>(rtcCandidate->sdp_mline_index());

    callOnMainThread([protectedThis = makeRef(*this), mid = fromStdString(rtcCandidate->sdp_mid()), sdp = fromStdString(sdp), sdpMLineIndex, url = fromStdString(rtcCandidate->server_url())]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.newICECandidate(WTFMove(sdp), WTFMove(mid), sdpMLineIndex, WTFMove(url));
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&)
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCMediaEndpoint::createSessionDescriptionSucceeded(std::unique_ptr<webrtc::SessionDescriptionInterface>&& description)
{
    std::string sdp;
    description->ToString(&sdp);

    callOnMainThread([protectedThis = makeRef(*this), sdp = fromStdString(sdp)]() mutable {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferSucceeded(WTFMove(sdp));
        else
            protectedThis->m_peerConnectionBackend.createAnswerSucceeded(WTFMove(sdp));
    });
}

void LibWebRTCMediaEndpoint::createSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = makeRef(*this), errorCode, errorMessage = String(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferFailed(Exception { errorCode, WTFMove(errorMessage) });
        else
            protectedThis->m_peerConnectionBackend.createAnswerFailed(Exception { errorCode, WTFMove(errorMessage) });
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

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = makeRef(*this), errorCode, errorMessage = String(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setLocalDescriptionFailed(Exception { errorCode, WTFMove(errorMessage) });
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

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = makeRef(*this), errorCode, errorMessage = String(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { errorCode, WTFMove(errorMessage) });
    });
}

void LibWebRTCMediaEndpoint::gatherStatsForLogging()
{
    m_backend->GetStats(this);
}

class RTCStatsLogger {
public:
    explicit RTCStatsLogger(const webrtc::RTCStats& stats)
        : m_stats(stats)
    {
    }

    String toJSONString() const { return String(m_stats.ToJson().c_str()); }

private:
    const webrtc::RTCStats& m_stats;
};

void LibWebRTCMediaEndpoint::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
#if !RELEASE_LOG_DISABLED
    int64_t timestamp = report->timestamp_us();
    if (!m_statsFirstDeliveredTimestamp)
        m_statsFirstDeliveredTimestamp = timestamp;

    callOnMainThread([protectedThis = makeRef(*this), this, timestamp, report] {
        if (m_backend && m_statsLogTimer.repeatInterval() != statsLogInterval(timestamp)) {
            m_statsLogTimer.stop();
            m_statsLogTimer.startRepeating(statsLogInterval(timestamp));
        }

        for (auto iterator = report->begin(); iterator != report->end(); ++iterator) {
            if (logger().willLog(logChannel(), WTFLogLevel::Debug)) {
                // Stats are very verbose, let's only display them in inspector console in verbose mode.
                logger().debug(LogWebRTC,
                    Logger::LogSiteIdentifier("LibWebRTCMediaEndpoint", "OnStatsDelivered", logIdentifier()),
                    RTCStatsLogger { *iterator });
            } else {
                logger().logAlways(LogWebRTCStats,
                    Logger::LogSiteIdentifier("LibWebRTCMediaEndpoint", "OnStatsDelivered", logIdentifier()),
                    RTCStatsLogger { *iterator });
            }
        }
    });
#else
    UNUSED_PARAM(report);
#endif
}

void LibWebRTCMediaEndpoint::startLoggingStats()
{
#if !RELEASE_LOG_DISABLED
    if (m_statsLogTimer.isActive())
        m_statsLogTimer.stop();
    m_statsLogTimer.startRepeating(statsLogInterval(0));
#endif
}

void LibWebRTCMediaEndpoint::stopLoggingStats()
{
    m_statsLogTimer.stop();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& LibWebRTCMediaEndpoint::logChannel() const
{
    return LogWebRTC;
}

Seconds LibWebRTCMediaEndpoint::statsLogInterval(int64_t reportTimestamp) const
{
    if (logger().willLog(logChannel(), WTFLogLevel::Info))
        return 2_s;

    if (reportTimestamp - m_statsFirstDeliveredTimestamp > 15000000)
        return 10_s;

    return 4_s;
}
#endif

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::RTCStatsLogger> {
    static String toString(const WebCore::RTCStatsLogger& logger)
    {
        return String(logger.toJSONString());
    }
};

}; // namespace WTF


#endif // USE(LIBWEBRTC)
