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

#include "DeprecatedGlobalSettings.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCStatsReport.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCProvider.h"
#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCRtpTransceiverBackend.h"
#include "LibWebRTCSctpTransportBackend.h"
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
#include <webrtc/rtc_base/physical_socket_server.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/p2p/client/basic_port_allocator.h>
#include <webrtc/pc/peer_connection_factory.h>
#include <webrtc/system_wrappers/include/field_trial.h>
#include <wtf/MainThread.h>
#include <wtf/SharedTask.h>

namespace WebCore {

LibWebRTCMediaEndpoint::LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client)
    : m_peerConnectionBackend(peerConnection)
    , m_peerConnectionFactory(client.factory())
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

    if (DeprecatedGlobalSettings::webRTCH264SimulcastEnabled())
        webrtc::field_trial::InitFieldTrialsFromString("WebRTC-H264Simulcast/Enabled/");
}

void LibWebRTCMediaEndpoint::restartIce()
{
    if (m_backend)
        m_backend->RestartIce();
}

bool LibWebRTCMediaEndpoint::setConfiguration(LibWebRTCProvider& client, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    configuration.crypto_options = webrtc::CryptoOptions { };
    configuration.crypto_options->srtp.enable_gcm_crypto_suites = true;

    if (!m_backend) {
        auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
        if (!m_rtcSocketFactory) {
            RegistrableDomain domain { document.url() };
            bool isFirstParty = domain == RegistrableDomain(document.firstPartyForCookies());
            m_rtcSocketFactory = client.createSocketFactory(document.userAgent(document.url()), isFirstParty, WTFMove(domain));
            if (!m_peerConnectionBackend.shouldFilterICECandidates() && m_rtcSocketFactory)
                m_rtcSocketFactory->disableRelay();
        }
        m_backend = client.createPeerConnection(document.identifier(), *this, m_rtcSocketFactory.get(), WTFMove(configuration));
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

bool LibWebRTCMediaEndpoint::isNegotiationNeeded(uint32_t eventId) const
{
    return m_backend ? m_backend->ShouldFireNegotiationNeededEvent(eventId) : false;
}

static inline webrtc::SdpType sessionDescriptionType(RTCSdpType sdpType)
{
    switch (sdpType) {
    case RTCSdpType::Offer:
        return webrtc::SdpType::kOffer;
    case RTCSdpType::Pranswer:
        return webrtc::SdpType::kPrAnswer;
    case RTCSdpType::Answer:
        return webrtc::SdpType::kAnswer;
    case RTCSdpType::Rollback:
        return webrtc::SdpType::kRollback;
    }
    ASSERT_NOT_REACHED();
    return webrtc::SdpType::kOffer;
}

void LibWebRTCMediaEndpoint::doSetLocalDescription(const RTCSessionDescription* description)
{
    ASSERT(m_backend);

    if (!description) {
        m_backend->SetLocalDescription(rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> { &m_setLocalSessionDescriptionObserver });
        return;
    }

    webrtc::SdpParseError error;
    auto sessionDescription = webrtc::CreateSessionDescription(sessionDescriptionType(description->type()), description->sdp().utf8().data(), &error);

    if (!sessionDescription) {
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, fromStdString(error.description) });
        return;
    }

    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=173783. Remove this test once fixed at LibWebRTC level.
    if (description->type() == RTCSdpType::Answer && !m_backend->pending_remote_description()) {
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
        return;
    }

    m_backend->SetLocalDescription(WTFMove(sessionDescription), rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> { &m_setLocalSessionDescriptionObserver });
}

void LibWebRTCMediaEndpoint::doSetRemoteDescription(const RTCSessionDescription& description)
{
    ASSERT(m_backend);

    webrtc::SdpParseError error;
    auto sessionDescription = webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error);
    if (!sessionDescription) {
        m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { SyntaxError, fromStdString(error.description) });
        return;
    }

    m_backend->SetRemoteDescription(WTFMove(sessionDescription), rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface> { &m_setRemoteSessionDescriptionObserver });

    startLoggingStats();
}

bool LibWebRTCMediaEndpoint::addTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track, const FixedVector<String>& mediaStreamIds)
{
    ASSERT(m_backend);

    LibWebRTCRtpSenderBackend::Source source;
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory->CreateAudioTrack(track.id().utf8().data(), audioSource.ptr());
        source = WTFMove(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory->CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        source = WTFMove(videoSource);
        break;
    }
    }

    sender.setSource(WTFMove(source));
    if (auto rtpSender = sender.rtcSender()) {
        rtpSender->SetTrack(rtcTrack.get());
        return true;
    }

    std::vector<std::string> ids;
    for (auto& id : mediaStreamIds)
        ids.push_back(id.utf8().data());

    auto newRTPSender = m_backend->AddTrack(WTFMove(rtcTrack), WTFMove(ids));
    if (!newRTPSender.ok())
        return false;
    sender.setRTCSender(newRTPSender.MoveValue());
    return true;
}

void LibWebRTCMediaEndpoint::removeTrack(LibWebRTCRtpSenderBackend& sender)
{
    ASSERT(m_backend);
    m_backend->RemoveTrackOrError(rtc::scoped_refptr<webrtc::RtpSenderInterface> { sender.rtcSender() });
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
    return LibWebRTCStatsCollector::create([promise = WTFMove(promise), protectedThis = Ref { *this }](auto&& rtcReport) mutable {
        ASSERT(isMainThread());
        if (protectedThis->isStopped())
            return;

        promise->resolve<IDLInterface<RTCStatsReport>>(LibWebRTCStatsCollector::createReport(rtcReport));
    });
}

void LibWebRTCMediaEndpoint::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    if (!m_backend) {
        callback({ });
        return;
    }
    auto collector = LibWebRTCStatsCollector::create([callback = WTFMove(callback)](auto&& rtcReport) mutable {
        ASSERT(isMainThread());
        if (rtcReport) {
            for (const auto& rtcStats : *rtcReport) {
                if (rtcStats.type() == webrtc::RTCInboundRTPStreamStats::kType) {
                    auto& inboundRTPStats = static_cast<const webrtc::RTCInboundRTPStreamStats&>(rtcStats);
                    if (inboundRTPStats.decoder_implementation.is_defined()) {
                        callback(fromStdString(*inboundRTPStats.decoder_implementation));
                        return;
                    }
                }
            }
        }
        callback({ });
    });
    m_backend->GetStats(collector.get());
}

void LibWebRTCMediaEndpoint::getStats(Ref<DeferredPromise>&& promise)
{
    if (m_backend)
        m_backend->GetStats(createStatsCollector(WTFMove(promise)).get());
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

void LibWebRTCMediaEndpoint::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState)
{
}

MediaStream& LibWebRTCMediaEndpoint::mediaStreamFromRTCStreamId(const String& id)
{
    auto mediaStream = m_remoteStreamsById.ensure(id, [id, this]() mutable {
        auto& document = downcast<Document>(*m_peerConnectionBackend.connection().scriptExecutionContext());
        auto stream = MediaStream::create(document, MediaStreamPrivate::create(document.logger(), { }, String(id)));
        return stream;
    });
    return *mediaStream.iterator->value;
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
        existingTransceiver = &m_peerConnectionBackend.newRemoteTransceiver(makeUnique<LibWebRTCRtpTransceiverBackend>(WTFMove(rtcTransceiver)), rtcReceiver->media_type() == cricket::MEDIA_TYPE_AUDIO ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);
    }
}

std::optional<bool> LibWebRTCMediaEndpoint::canTrickleIceCandidates() const
{
    if (!m_backend || !m_backend->can_trickle_ice_candidates())
        return { };
    return *m_backend->can_trickle_ice_candidates();
}

template<typename T>
ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::createTransceiverBackends(T&& trackOrKind, webrtc::RtpTransceiverInit&& init, LibWebRTCRtpSenderBackend::Source&& source)
{
    auto result = m_backend->AddTransceiver(WTFMove(trackOrKind), WTFMove(init));
    if (!result.ok())
        return toException(result.error());

    auto transceiver = makeUnique<LibWebRTCRtpTransceiverBackend>(result.MoveValue());
    return LibWebRTCMediaEndpoint::Backends { transceiver->createSenderBackend(m_peerConnectionBackend, WTFMove(source)), transceiver->createReceiverBackend(), WTFMove(transceiver) };
}

ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init)
{
    auto type = trackKind == "audio"_s ? cricket::MediaType::MEDIA_TYPE_AUDIO : cricket::MediaType::MEDIA_TYPE_VIDEO;
    return createTransceiverBackends(type, fromRtpTransceiverInit(init, type), nullptr);
}

std::pair<LibWebRTCRtpSenderBackend::Source, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>> LibWebRTCMediaEndpoint::createSourceAndRTCTrack(MediaStreamTrack& track)
{
    LibWebRTCRtpSenderBackend::Source source;
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory->CreateAudioTrack(track.id().utf8().data(), audioSource.ptr());
        source = WTFMove(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory->CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        source = WTFMove(videoSource);
        break;
    }
    }
    return std::make_pair(WTFMove(source), WTFMove(rtcTrack));
}

ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(MediaStreamTrack& track, const RTCRtpTransceiverInit& init)
{
    auto type = track.source().type() == RealtimeMediaSource::Type::Audio ? cricket::MediaType::MEDIA_TYPE_AUDIO : cricket::MediaType::MEDIA_TYPE_VIDEO;
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    return createTransceiverBackends(WTFMove(sourceAndTrack.second), fromRtpTransceiverInit(init, type), WTFMove(sourceAndTrack.first));
}

void LibWebRTCMediaEndpoint::setSenderSourceFromTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track)
{
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    sender.setSource(WTFMove(sourceAndTrack.first));
    sender.rtcSender()->SetTrack(sourceAndTrack.second.get());
}

std::unique_ptr<LibWebRTCRtpTransceiverBackend> LibWebRTCMediaEndpoint::transceiverBackendFromSender(LibWebRTCRtpSenderBackend& backend)
{
    for (auto& transceiver : m_backend->GetTransceivers()) {
        if (transceiver->sender().get() == backend.rtcSender())
            return makeUnique<LibWebRTCRtpTransceiverBackend>(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>(transceiver));
    }
    return nullptr;
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    auto init = LibWebRTCDataChannelHandler::fromRTCDataChannelInit(options);
    // FIXME: Forward or log error  if there is one.
    auto channel = m_backend->CreateDataChannelOrError(label.utf8().data(), &init);
    return channel.ok() ? makeUnique<LibWebRTCDataChannelHandler>(channel.MoveValue()) : nullptr;
}

void LibWebRTCMediaEndpoint::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{
    callOnMainThread([protectedThis = Ref { *this }, dataChannel = WTFMove(dataChannel)]() mutable {
        if (protectedThis->isStopped())
            return;
        auto channelHandler = makeUniqueRef<LibWebRTCDataChannelHandler>(WTFMove(dataChannel));
        auto label = channelHandler->label();
        auto dataChannelInit = channelHandler->dataChannelInit();
        protectedThis->m_peerConnectionBackend.newDataChannel(WTFMove(channelHandler), WTFMove(label), WTFMove(dataChannelInit));
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

void LibWebRTCMediaEndpoint::OnNegotiationNeededEvent(uint32_t eventId)
{
    callOnMainThread([protectedThis = Ref { *this }, eventId] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.markAsNeedingNegotiation(eventId);
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

void LibWebRTCMediaEndpoint::OnStandardizedIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state)
{
    auto connectionState = toRTCIceConnectionState(state);
    callOnMainThread([protectedThis = Ref { *this }, connectionState] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.connection().updateIceConnectionState(connectionState);
    });
}

static inline RTCIceGatheringState toRTCIceGatheringState(webrtc::PeerConnectionInterface::IceGatheringState state)
{
    switch (state) {
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringNew:
        return RTCIceGatheringState::New;
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering:
        return RTCIceGatheringState::Gathering;
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete:
        return RTCIceGatheringState::Complete;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void LibWebRTCMediaEndpoint::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state)
{
    callOnMainThread([protectedThis = Ref { *this }, state] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.iceGatheringStateChanged(toRTCIceGatheringState(state));
    });
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

static RTCSignalingState toRTCSignalingState(webrtc::PeerConnectionInterface::SignalingState state)
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

enum class GatherSignalingState { No, Yes };
static std::optional<PeerConnectionBackend::DescriptionStates> descriptionsFromPeerConnection(webrtc::PeerConnectionInterface* connection, GatherSignalingState gatherSignalingState = GatherSignalingState::No)
{
    if (!connection)
        return { };

    std::optional<RTCSdpType> currentLocalDescriptionSdpType, pendingLocalDescriptionSdpType, currentRemoteDescriptionSdpType, pendingRemoteDescriptionSdpType;
    std::string currentLocalDescriptionSdp, pendingLocalDescriptionSdp, currentRemoteDescriptionSdp, pendingRemoteDescriptionSdp;
    if (auto* description = connection->current_local_description()) {
        currentLocalDescriptionSdpType = fromSessionDescriptionType(*description);
        description->ToString(&currentLocalDescriptionSdp);
    }
    if (auto* description = connection->pending_local_description()) {
        pendingLocalDescriptionSdpType = fromSessionDescriptionType(*description);
        description->ToString(&pendingLocalDescriptionSdp);
    }
    if (auto* description = connection->current_remote_description()) {
        currentRemoteDescriptionSdpType = fromSessionDescriptionType(*description);
        description->ToString(&currentRemoteDescriptionSdp);
    }
    if (auto* description = connection->pending_remote_description()) {
        pendingRemoteDescriptionSdpType = fromSessionDescriptionType(*description);
        description->ToString(&pendingRemoteDescriptionSdp);
    }

    std::optional<RTCSignalingState> signalingState;
    if (gatherSignalingState == GatherSignalingState::Yes)
        signalingState = toRTCSignalingState(connection->signaling_state());
    return PeerConnectionBackend::DescriptionStates {
        signalingState,
        currentLocalDescriptionSdpType, fromStdString(currentLocalDescriptionSdp),
        pendingLocalDescriptionSdpType, fromStdString(pendingLocalDescriptionSdp),
        currentRemoteDescriptionSdpType, fromStdString(currentRemoteDescriptionSdp),
        pendingRemoteDescriptionSdpType, fromStdString(pendingRemoteDescriptionSdp)
    };
}

void LibWebRTCMediaEndpoint::addIceCandidate(std::unique_ptr<webrtc::IceCandidateInterface>&& candidate, PeerConnectionBackend::AddIceCandidateCallback&& callback)
{
    m_backend->AddIceCandidate(WTFMove(candidate), [task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTFMove(callback)), backend = m_backend](auto&& error) mutable {
        callOnMainThread([task = WTFMove(task), descriptions = crossThreadCopy(descriptionsFromPeerConnection(backend.get())), error = WTFMove(error)]() mutable {
            if (!error.ok()) {
                task->run(toException(error));
                return;
            }
            task->run(WTFMove(descriptions));
        });
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidate(const webrtc::IceCandidateInterface *rtcCandidate)
{
    ASSERT(rtcCandidate);

    std::string sdp;
    rtcCandidate->ToString(&sdp);

    auto sdpMLineIndex = safeCast<unsigned short>(rtcCandidate->sdp_mline_index());

    callOnMainThread([protectedThis = Ref { *this }, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get())), mid = fromStdString(rtcCandidate->sdp_mid()), sdp = fromStdString(sdp), sdpMLineIndex, url = fromStdString(rtcCandidate->server_url())]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.newICECandidate(WTFMove(sdp), WTFMove(mid), sdpMLineIndex, WTFMove(url), WTFMove(descriptions));
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

    callOnMainThread([protectedThis = Ref { *this }, sdp = fromStdString(sdp)]() mutable {
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
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferFailed(Exception { errorCode, WTFMove(errorMessage) });
        else
            protectedThis->m_peerConnectionBackend.createAnswerFailed(Exception { errorCode, WTFMove(errorMessage) });
    });
}

class SctpTransportState {
public:
    explicit SctpTransportState(rtc::scoped_refptr<webrtc::SctpTransportInterface>&&);
    std::unique_ptr<LibWebRTCSctpTransportBackend> createBackend();

private:
    rtc::scoped_refptr<webrtc::SctpTransportInterface> m_transport;
    webrtc::SctpTransportInformation m_information;
};

SctpTransportState::SctpTransportState(rtc::scoped_refptr<webrtc::SctpTransportInterface>&& transport)
    : m_transport(WTFMove(transport))
{
    if (m_transport)
        m_information = m_transport->Information();
}

std::unique_ptr<LibWebRTCSctpTransportBackend> SctpTransportState::createBackend()
{
    if (!m_transport)
        return nullptr;
    return makeUnique<LibWebRTCSctpTransportBackend>(WTFMove(m_transport), m_information.dtls_transport());
}

struct LibWebRTCMediaEndpointTransceiverState {
    String mid;
    Vector<String> receiverStreamIds;
    std::optional<RTCRtpTransceiverDirection> firedDirection;

    LibWebRTCMediaEndpointTransceiverState isolatedCopy() &&;
};

inline LibWebRTCMediaEndpointTransceiverState LibWebRTCMediaEndpointTransceiverState::isolatedCopy() &&
{
    return {
        WTFMove(mid).isolatedCopy(),
        crossThreadCopy(WTFMove(receiverStreamIds)),
        firedDirection
    };
}

static LibWebRTCMediaEndpointTransceiverState toLibWebRTCMediaEndpointTransceiverState(const webrtc::RtpTransceiverInterface& transceiver)
{
    String mid;
    if (auto rtcMid = transceiver.mid())
        mid = fromStdString(*rtcMid);
    std::optional<RTCRtpTransceiverDirection> firedDirection;
    if (auto rtcFiredDirection = transceiver.fired_direction())
        firedDirection = toRTCRtpTransceiverDirection(*rtcFiredDirection);

    auto rtcStreamIds = transceiver.receiver()->stream_ids();
    Vector<String> streamIds;
    streamIds.reserveInitialCapacity(rtcStreamIds.size());
    for (auto& streamId : rtcStreamIds)
        streamIds.uncheckedAppend(fromStdString(streamId));

    return { WTFMove(mid), WTFMove(streamIds), firedDirection };
}

static Vector<LibWebRTCMediaEndpointTransceiverState> transceiverStatesFromPeerConnection(webrtc::PeerConnectionInterface& connection)
{
    auto transceivers = connection.GetTransceivers();
    Vector<LibWebRTCMediaEndpointTransceiverState> states;
    states.reserveInitialCapacity(transceivers.size());
    for (auto& transceiver : transceivers)
        states.uncheckedAppend(toLibWebRTCMediaEndpointTransceiverState(*transceiver));

    return states;
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionSucceeded()
{
    if (!m_backend)
        return;

    callOnMainThread([protectedThis = Ref { *this }, this, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get(), GatherSignalingState::Yes)), rtcTransceiverStates = crossThreadCopy(transceiverStatesFromPeerConnection(*m_backend)), sctpState = SctpTransportState(m_backend->GetSctpTransport())]() mutable {
        if (protectedThis->isStopped())
            return;

        auto transceiverStates = WTF::map(rtcTransceiverStates, [this](auto& state) -> PeerConnectionBackend::TransceiverState {
            auto streams = WTF::map(state.receiverStreamIds, [this](auto& id) -> RefPtr<MediaStream> {
                return &mediaStreamFromRTCStreamId(id);
            });
            return { WTFMove(state.mid), WTFMove(streams), state.firedDirection };
        });
        protectedThis->m_peerConnectionBackend.setLocalDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), sctpState.createBackend());
    });
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setLocalDescriptionFailed(Exception { errorCode, WTFMove(errorMessage) });
    });
}
void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionSucceeded()
{
    if (!m_backend)
        return;

    callOnMainThread([protectedThis = Ref { *this }, this, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get(), GatherSignalingState::Yes)), rtcTransceiverStates = crossThreadCopy(transceiverStatesFromPeerConnection(*m_backend)), sctpState = SctpTransportState(m_backend->GetSctpTransport())]() mutable {
        if (protectedThis->isStopped())
            return;

        auto transceiverStates = WTF::map(rtcTransceiverStates, [this](auto& state) -> PeerConnectionBackend::TransceiverState {
            auto streams = WTF::map(state.receiverStreamIds, [this](auto& id) -> RefPtr<MediaStream> {
                return &mediaStreamFromRTCStreamId(id);
            });
            return { WTFMove(state.mid), WTFMove(streams), state.firedDirection };
        });
        protectedThis->m_peerConnectionBackend.setRemoteDescriptionSucceeded(WTFMove(descriptions), WTFMove(transceiverStates), sctpState.createBackend());
    });
}

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)] () mutable {
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

    String toJSONString() const { return String::fromLatin1(m_stats.ToJson().c_str()); }

private:
    const webrtc::RTCStats& m_stats;
};

void LibWebRTCMediaEndpoint::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
#if !RELEASE_LOG_DISABLED
    int64_t timestamp = report->timestamp_us();
    if (!m_statsFirstDeliveredTimestamp)
        m_statsFirstDeliveredTimestamp = timestamp;

    callOnMainThread([protectedThis = Ref { *this }, this, timestamp, report] {
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
