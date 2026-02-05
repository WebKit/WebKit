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

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "DeprecatedGlobalSettings.h"
#include "Document.h"
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
#include "RegistrableDomain.h"
#include "Settings.h"
#include <webrtc/api/stats/rtcstats_objects.h>
#include <webrtc/rtc_base/physical_socket_server.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
#include <webrtc/p2p/client/basic_port_allocator.h>
IGNORE_CLANG_WARNINGS_END
#include <webrtc/pc/peer_connection_factory.h>
#include <webrtc/system_wrappers/include/field_trial.h>
#include <wtf/MainThread.h>
#include <wtf/SetForScope.h>
#include <wtf/SharedTask.h>

namespace WebCore {

static void prepareConfiguration(webrtc::PeerConnectionInterface::RTCConfiguration& configuration)
{
    configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    configuration.crypto_options.srtp.enable_gcm_crypto_suites = true;
}

RefPtr<LibWebRTCMediaEndpoint> LibWebRTCMediaEndpoint::create(RTCPeerConnection& peerConnection, LibWebRTCProvider& client, Document& document, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration, bool shouldEnableServiceClass)
{
    prepareConfiguration(configuration);

    Ref endpoint = adoptRef(*new LibWebRTCMediaEndpoint(peerConnection, client, document, shouldEnableServiceClass));
    RefPtr backend = toRefPtr(client.createPeerConnection(document.identifier(), endpoint, endpoint->m_rtcSocketFactory.get(), WTF::move(configuration)));
    if (!backend)
        return { };

    lazyInitialize(endpoint->m_backend, backend.releaseNonNull());
    return endpoint;
}

static std::unique_ptr<LibWebRTCProvider::SuspendableSocketFactory> createLibWebRTCMediaEndpointSocketFactory(LibWebRTCProvider& client, Document& document, bool shouldEnableServiceClass)
{
    RegistrableDomain domain { document.url() };
    bool isFirstParty = domain == RegistrableDomain(document.firstPartyForCookies());
    auto rtcSocketFactory = client.createSocketFactory(document.userAgent(document.url()), document.identifier(), isFirstParty, WTF::move(domain), shouldEnableServiceClass);
    if (!client.isSupportingMDNS() && rtcSocketFactory)
        rtcSocketFactory->disableRelay();
    return rtcSocketFactory;
}

LibWebRTCMediaEndpoint::LibWebRTCMediaEndpoint(RTCPeerConnection& peerConnection, LibWebRTCProvider& client, Document& document, bool shouldEnableServiceClass)
    : m_peerConnectionFactory(*client.factory())
    , m_createSessionDescriptionObserver(*this)
    , m_setLocalSessionDescriptionObserver(*this)
    , m_setRemoteSessionDescriptionObserver(*this)
    , m_statsLogTimer(*this, &LibWebRTCMediaEndpoint::gatherStatsForLogging)
    , m_rtcSocketFactory(createLibWebRTCMediaEndpointSocketFactory(client, document, shouldEnableServiceClass))
#if !RELEASE_LOG_DISABLED
    , m_logger(peerConnection.logger())
    , m_logIdentifier(peerConnection.logIdentifier())
#endif
{
    ASSERT(isMainThread());
    ASSERT(client.factory());

#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(peerConnection);
#endif

    client.setUseL4S(document.settings().webRTCL4SEnabled());
}

LibWebRTCMediaEndpoint::~LibWebRTCMediaEndpoint()
{
    if (!m_backend)
        return;

    // We move backend to the signalling thread in case this is the last reference, as deallocating backend could block on the signalling thread.
    Ref backend = const_cast<RefPtr<webrtc::PeerConnectionInterface>*>(&m_backend)->releaseNonNull();
    LibWebRTCProvider::callOnWebRTCSignalingThread([backend = WTF::move(backend)] { });
}

void LibWebRTCMediaEndpoint::setPeerConnectionBackend(LibWebRTCPeerConnectionBackend& peerConnectionBackend)
{
    ASSERT(!m_peerConnectionBackend);
    m_peerConnectionBackend = peerConnectionBackend;
}

void LibWebRTCMediaEndpoint::restartIce()
{
    if (!m_isStopped)
        m_backend->RestartIce();
}

bool LibWebRTCMediaEndpoint::setConfiguration(webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    prepareConfiguration(configuration);

    auto oldConfiguration = m_backend->GetConfiguration();
    configuration.certificates = oldConfiguration.certificates;
    return m_backend->SetConfiguration(WTF::move(configuration)).ok();
}

void LibWebRTCMediaEndpoint::suspend()
{
    stopLoggingStats();
    if (m_rtcSocketFactory)
        m_rtcSocketFactory->suspend();
}

void LibWebRTCMediaEndpoint::resume()
{
    startLoggingStats();
    if (m_rtcSocketFactory)
        m_rtcSocketFactory->resume();
}

void LibWebRTCMediaEndpoint::disableSocketRelay()
{
    if (m_rtcSocketFactory)
        m_rtcSocketFactory->disableRelay();
}

bool LibWebRTCMediaEndpoint::isNegotiationNeeded(uint32_t eventId) const
{
    return !m_isStopped ? m_backend->ShouldFireNegotiationNeededEvent(eventId) : false;
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
        m_backend->SetLocalDescription(webrtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> { &m_setLocalSessionDescriptionObserver });
        return;
    }

    webrtc::SdpParseError error;
    auto sessionDescription = webrtc::CreateSessionDescription(sessionDescriptionType(description->type()), description->sdp().utf8().data(), &error);

    if (!sessionDescription) {
        protectedPeerConnectionBackend()->setLocalDescriptionFailed(Exception { ExceptionCode::OperationError, fromStdString(error.description) });
        return;
    }

    // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=173783. Remove this test once fixed at LibWebRTC level.
    if (description->type() == RTCSdpType::Answer && !m_backend->pending_remote_description()) {
        protectedPeerConnectionBackend()->setLocalDescriptionFailed(Exception { ExceptionCode::InvalidStateError, "Failed to set local answer sdp: no pending remote description."_s });
        return;
    }

    m_backend->SetLocalDescription(WTF::move(sessionDescription), webrtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> { &m_setLocalSessionDescriptionObserver });
}

void LibWebRTCMediaEndpoint::doSetRemoteDescription(const RTCSessionDescription& description)
{
    ASSERT(!m_isStopped);

    webrtc::SdpParseError error;
    auto sessionDescription = webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error);
    if (!sessionDescription) {
        protectedPeerConnectionBackend()->setRemoteDescriptionFailed(Exception { ExceptionCode::SyntaxError, fromStdString(error.description) });
        return;
    }

    m_backend->SetRemoteDescription(WTF::move(sessionDescription), webrtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface> { &m_setRemoteSessionDescriptionObserver });

    startLoggingStats();
}

bool LibWebRTCMediaEndpoint::addTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track, const FixedVector<String>& mediaStreamIds)
{
    ASSERT(!m_isStopped);

    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, track.privateTrack().type() == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s, " track with id "_s, track.id());

    LibWebRTCRtpSenderBackend::Source source;
    webrtc::scoped_refptr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = m_peerConnectionFactory->CreateAudioTrack(track.id().utf8().data(), audioSource.ptr());
        source = WTF::move(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());

        RefPtr context = protectedPeerConnectionBackend()->protectedConnection()->scriptExecutionContext();
        if (context && context->settingsValues().peerConnectionVideoScalingAdaptationDisabled)
            videoSource->disableVideoScaling();

        rtcTrack = m_peerConnectionFactory->CreateVideoTrack(webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> (videoSource.ptr()), track.id().utf8().data());
        source = WTF::move(videoSource);
        break;
    }
    }

    sender.setSource(WTF::move(source));
    if (RefPtr rtpSender = sender.rtcSender()) {
        rtpSender->SetTrack(rtcTrack.get());
        return true;
    }

    std::vector<std::string> ids;
    for (auto& id : mediaStreamIds)
        ids.push_back(id.utf8().data());

    auto newRTPSender = m_backend->AddTrack(WTF::move(rtcTrack), WTF::move(ids));
    if (!newRTPSender.ok())
        return false;
    sender.setRTCSender(toRefPtr(newRTPSender.MoveValue()));
    return true;
}

void LibWebRTCMediaEndpoint::removeTrack(LibWebRTCRtpSenderBackend& sender)
{
    ASSERT(!m_isStopped);

    m_backend->RemoveTrackOrError(webrtc::scoped_refptr { sender.rtcSender() });
    sender.clearSource();
}

void LibWebRTCMediaEndpoint::doCreateOffer(const RTCOfferOptions& options)
{
    ASSERT(!m_isStopped);

    m_isInitiator = true;
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcOptions;
    rtcOptions.ice_restart = options.iceRestart;
    rtcOptions.voice_activity_detection = options.voiceActivityDetection;

    m_backend->CreateOffer(&m_createSessionDescriptionObserver, rtcOptions);
}

void LibWebRTCMediaEndpoint::doCreateAnswer()
{
    ASSERT(!m_isStopped);

    m_isInitiator = false;

    m_backend->CreateAnswer(&m_createSessionDescriptionObserver, { });
}

webrtc::scoped_refptr<LibWebRTCStatsCollector> LibWebRTCMediaEndpoint::createStatsCollector(Ref<DeferredPromise>&& promise)
{
    return LibWebRTCStatsCollector::create([promise = WTF::move(promise), protectedThis = Ref { *this }](auto&& rtcReport) mutable {
        ASSERT(isMainThread());
        if (protectedThis->isStopped())
            return;

        ActiveDOMObject::queueTaskKeepingObjectAlive(protectedThis->protectedPeerConnectionBackend()->protectedConnection().get(), TaskSource::Networking, [promise = WTF::move(promise), rtcReport](auto&) {
            promise->resolve<IDLInterface<RTCStatsReport>>(LibWebRTCStatsCollector::createReport(rtcReport));
        });
    });
}

void LibWebRTCMediaEndpoint::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    if (m_isStopped) {
        callback({ });
        return;
    }
    auto collector = LibWebRTCStatsCollector::create([callback = WTF::move(callback)](auto&& rtcReport) mutable {
        ASSERT(isMainThread());
        if (rtcReport) {
            for (const auto& rtcStats : *rtcReport) {
                if (rtcStats.type() == webrtc::RTCInboundRtpStreamStats::kType) {
                    auto& inboundRTPStats = downcast<webrtc::RTCInboundRtpStreamStats>(rtcStats);
                    if (inboundRTPStats.decoder_implementation) {
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
    if (!m_isStopped)
        m_backend->GetStats(createStatsCollector(WTF::move(promise)).get());
}

void LibWebRTCMediaEndpoint::getStats(webrtc::RtpReceiverInterface& receiver, Ref<DeferredPromise>&& promise)
{
    if (!m_isStopped)
        m_backend->GetStats(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>(&receiver), createStatsCollector(WTF::move(promise)));
}

void LibWebRTCMediaEndpoint::getStats(webrtc::RtpSenderInterface& sender, Ref<DeferredPromise>&& promise)
{
    if (!m_isStopped)
        m_backend->GetStats(webrtc::scoped_refptr<webrtc::RtpSenderInterface>(&sender), createStatsCollector(WTF::move(promise)));
}

void LibWebRTCMediaEndpoint::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState)
{
}

void LibWebRTCMediaEndpoint::collectTransceivers()
{
    if (m_isStopped)
        return;

    Ref peerConnectionBackend = *m_peerConnectionBackend.get();
    for (webrtc::scoped_refptr rtcTransceiver : m_backend->GetTransceivers()) {
        RefPtr existingTransceiver = peerConnectionBackend->existingTransceiver([&](auto& transceiverBackend) {
            return rtcTransceiver.get() == transceiverBackend.rtcTransceiver();
        });
        if (existingTransceiver)
            continue;

        Ref rtcReceiver = toRef(rtcTransceiver->receiver());
        existingTransceiver = peerConnectionBackend->newRemoteTransceiver(makeUnique<LibWebRTCRtpTransceiverBackend>(toRef(WTF::move(rtcTransceiver))), rtcReceiver->media_type() == webrtc::MediaType::AUDIO ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video);
    }
}

std::optional<bool> LibWebRTCMediaEndpoint::canTrickleIceCandidates() const
{
    if (!m_backend->can_trickle_ice_candidates())
        return { };
    return *m_backend->can_trickle_ice_candidates();
}

template<typename T>
ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::createTransceiverBackends(T&& trackOrKind, webrtc::RtpTransceiverInit&& init, LibWebRTCRtpSenderBackend::Source&& source, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    bool shouldIgnoreNegotiationNeededSignal = ignoreNegotiationNeededFlag == PeerConnectionBackend::IgnoreNegotiationNeededFlag::Yes ? true : false;
    SetForScope scopedIgnoreNegotiationNeededSignal(m_shouldIgnoreNegotiationNeededSignal, shouldIgnoreNegotiationNeededSignal, false);

    auto result = m_backend->AddTransceiver(std::forward<T>(trackOrKind), WTF::move(init));
    if (!result.ok())
        return toException(result.error());

    auto transceiver = makeUnique<LibWebRTCRtpTransceiverBackend>(toRef(result.MoveValue()));
    return LibWebRTCMediaEndpoint::Backends { transceiver->createSenderBackend(*protectedPeerConnectionBackend(), WTF::move(source)), transceiver->createReceiverBackend(), WTF::move(transceiver) };
}

ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    auto direction = convertEnumerationToString(init.direction);
    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, trackKind, " ", direction, " transceiver"_s);
    auto type = trackKind == "audio"_s ? webrtc::MediaType::AUDIO : webrtc::MediaType::VIDEO;
    return createTransceiverBackends(type, fromRtpTransceiverInit(init, type), nullptr, ignoreNegotiationNeededFlag);
}

std::pair<LibWebRTCRtpSenderBackend::Source, Ref<webrtc::MediaStreamTrackInterface>> LibWebRTCMediaEndpoint::createSourceAndRTCTrack(MediaStreamTrack& track)
{
    LibWebRTCRtpSenderBackend::Source source;
    RefPtr<webrtc::MediaStreamTrackInterface> rtcTrack;
    switch (track.privateTrack().type()) {
    case RealtimeMediaSource::Type::Audio: {
        Ref audioSource = RealtimeOutgoingAudioSource::create(track.privateTrack());
        rtcTrack = toRef(m_peerConnectionFactory->CreateAudioTrack(track.id().utf8().data(), audioSource.ptr()));
        source = WTF::move(audioSource);
        break;
    }
    case RealtimeMediaSource::Type::Video: {
        Ref videoSource = RealtimeOutgoingVideoSource::create(track.privateTrack());

        RefPtr context = protectedPeerConnectionBackend()->protectedConnection()->scriptExecutionContext();
        if (context && context->settingsValues().peerConnectionVideoScalingAdaptationDisabled)
            videoSource->disableVideoScaling();

        rtcTrack = toRef(m_peerConnectionFactory->CreateVideoTrack(webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> (videoSource.ptr()), track.id().utf8().data()));
        source = WTF::move(videoSource);
        break;
    }
    }
    return std::make_pair(WTF::move(source), rtcTrack.releaseNonNull());
}

ExceptionOr<LibWebRTCMediaEndpoint::Backends> LibWebRTCMediaEndpoint::addTransceiver(MediaStreamTrack& track, const RTCRtpTransceiverInit& init, PeerConnectionBackend::IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    auto direction = convertEnumerationToString(init.direction);
    ALWAYS_LOG(LOGIDENTIFIER, "Adding "_s, track.kind().string(), " ", direction, " transceiver for track "_s, track.id());

    auto type = track.source().type() == RealtimeMediaSource::Type::Audio ? webrtc::MediaType::AUDIO : webrtc::MediaType::VIDEO;
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    return createTransceiverBackends(webrtc::scoped_refptr { sourceAndTrack.second.ptr() }, fromRtpTransceiverInit(init, type), WTF::move(sourceAndTrack.first), ignoreNegotiationNeededFlag);
}

void LibWebRTCMediaEndpoint::setSenderSourceFromTrack(LibWebRTCRtpSenderBackend& sender, MediaStreamTrack& track)
{
    auto sourceAndTrack = createSourceAndRTCTrack(track);
    sender.setSource(WTF::move(sourceAndTrack.first));
    sender.protectedRTCSender()->SetTrack(sourceAndTrack.second.ptr());
}

std::unique_ptr<LibWebRTCRtpTransceiverBackend> LibWebRTCMediaEndpoint::transceiverBackendFromSender(LibWebRTCRtpSenderBackend& backend)
{
    for (auto& transceiver : m_backend->GetTransceivers()) {
        if (transceiver->sender().get() == backend.rtcSender())
            return makeUnique<LibWebRTCRtpTransceiverBackend>(toRef(webrtc::scoped_refptr { transceiver }));
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

void LibWebRTCMediaEndpoint::OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{
    callOnMainThread([protectedThis = Ref { *this }, dataChannel = WTF::move(dataChannel)]() mutable {
        if (protectedThis->isStopped())
            return;
        auto channelHandler = makeUniqueRef<LibWebRTCDataChannelHandler>(WTF::move(dataChannel));
        auto label = channelHandler->label();
        auto dataChannelInit = channelHandler->dataChannelInit();
        protectedThis->protectedPeerConnectionBackend()->newDataChannel(WTF::move(channelHandler), WTF::move(label), WTF::move(dataChannelInit));
    });
}

void LibWebRTCMediaEndpoint::close()
{
    m_backend->Close();
    stopLoggingStats();
    m_isClosed = true;
}

void LibWebRTCMediaEndpoint::stop()
{
    if (!m_backend || m_isStopped)
        return;

    m_isStopped = true;
    if (!m_isClosed)
        close();

    for (Ref stream : m_remoteStreamsById.values())
        stream->inactivate();
    m_remoteStreamsById.clear();
}

void LibWebRTCMediaEndpoint::OnNegotiationNeededEvent(uint32_t eventId)
{
    if (m_shouldIgnoreNegotiationNeededSignal)
        return;
    callOnMainThread([protectedThis = Ref { *this }, eventId] {
        if (protectedThis->isStopped())
            return;
        protectedThis->protectedPeerConnectionBackend()->markAsNeedingNegotiation(eventId);
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
        Ref connection = protectedThis->protectedPeerConnectionBackend()->connection();
        connection->updateIceConnectionState(connectionState);
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
        protectedThis->protectedPeerConnectionBackend()->iceGatheringStateChanged(toRTCIceGatheringState(state));
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

enum class GatherSignalingState : bool { No, Yes };
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

void LibWebRTCMediaEndpoint::addIceCandidate(std::unique_ptr<webrtc::IceCandidate>&& candidate, PeerConnectionBackend::AddIceCandidateCallback&& callback)
{
    m_backend->AddIceCandidate(WTF::move(candidate), [task = createSharedTask<PeerConnectionBackend::AddIceCandidateCallbackFunction>(WTF::move(callback)), backend = m_backend]<typename Error> (Error&& error) mutable {
        callOnMainThread([task = WTF::move(task), descriptions = crossThreadCopy(descriptionsFromPeerConnection(backend.get())), error = std::forward<Error>(error)] () mutable {
            if (!error.ok()) {
                task->run(toException(error));
                return;
            }
            task->run(WTF::move(descriptions));
        });
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidate(const webrtc::IceCandidate *rtcCandidate)
{
    ASSERT(rtcCandidate);

    std::string sdp;
    rtcCandidate->ToString(&sdp);

    auto sdpMLineIndex = safeCast<unsigned short>(rtcCandidate->sdp_mline_index());

    callOnMainThread([protectedThis = Ref { *this }, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get())), mid = fromStdString(rtcCandidate->sdp_mid()), sdp = fromStdString(sdp), sdpMLineIndex, url = fromStdString(rtcCandidate->server_url())]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->protectedPeerConnectionBackend()->newICECandidate(WTF::move(sdp), WTF::move(mid), sdpMLineIndex, WTF::move(url), WTF::move(descriptions));
    });
}

void LibWebRTCMediaEndpoint::createSessionDescriptionSucceeded(std::unique_ptr<webrtc::SessionDescriptionInterface>&& description)
{
    std::string sdp;
    description->ToString(&sdp);

    callOnMainThread([protectedThis = Ref { *this }, sdp = fromStdString(sdp)]() mutable {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->protectedPeerConnectionBackend()->createOfferSucceeded(WTF::move(sdp));
        else
            protectedThis->protectedPeerConnectionBackend()->createAnswerSucceeded(WTF::move(sdp));
    });
}

void LibWebRTCMediaEndpoint::createSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->protectedPeerConnectionBackend()->createOfferFailed(Exception { errorCode, WTF::move(errorMessage) });
        else
            protectedThis->protectedPeerConnectionBackend()->createAnswerFailed(Exception { errorCode, WTF::move(errorMessage) });
    });
}

class SctpTransportState {
public:
    explicit SctpTransportState(webrtc::scoped_refptr<webrtc::SctpTransportInterface>&&);
    std::unique_ptr<LibWebRTCSctpTransportBackend> createBackend();
    std::optional<double> maxMessageSize() const;

private:
    RefPtr<webrtc::SctpTransportInterface> m_transport;
    webrtc::SctpTransportInformation m_information;
};

SctpTransportState::SctpTransportState(webrtc::scoped_refptr<webrtc::SctpTransportInterface>&& transport)
    : m_transport(toRefPtr(WTF::move(transport)))
{
    if (RefPtr transport = m_transport)
        m_information = m_transport->Information();
}

std::unique_ptr<LibWebRTCSctpTransportBackend> SctpTransportState::createBackend()
{
    if (!m_transport)
        return nullptr;
    return makeUnique<LibWebRTCSctpTransportBackend>(m_transport.releaseNonNull(), toRef(m_information.dtls_transport()));
}

std::optional<double> SctpTransportState::maxMessageSize() const
{
    return m_information.MaxMessageSize() ? std::make_optional(*m_information.MaxMessageSize()) : std::nullopt;
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
        WTF::move(mid).isolatedCopy(),
        crossThreadCopy(WTF::move(receiverStreamIds)),
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
    auto streamIds = WTF::map(rtcStreamIds, [](auto& streamId) {
        return fromStdString(streamId);
    });

    return { WTF::move(mid), WTF::move(streamIds), firedDirection };
}

static Vector<LibWebRTCMediaEndpointTransceiverState> transceiverStatesFromPeerConnection(webrtc::PeerConnectionInterface& connection)
{
    auto transceivers = connection.GetTransceivers();
    return WTF::map(transceivers, [](auto& transceiver) {
        return toLibWebRTCMediaEndpointTransceiverState(*transceiver);
    });
}


Vector<Ref<MediaStream>> LibWebRTCMediaEndpoint::mediaStreamsFromRTCStreamIds(const Vector<String>& receiverStreamIds)
{
    Ref document = downcast<Document>(*protectedPeerConnectionBackend()->protectedConnection()->scriptExecutionContext());
    return WTF::map(receiverStreamIds, [this, &document](auto& id) {
        auto addResult = m_remoteStreamsById.ensure(id, [id, &document]() {
            return MediaStream::create(document, MediaStreamPrivate::create(document->logger(), { }, String(id)), MediaStream::AllowEventTracks::Yes);
        });
        return addResult.iterator->value;
    });
}


PeerConnectionBackend::TransceiverStates LibWebRTCMediaEndpoint::generateTransceiverStates(const Vector<LibWebRTCMediaEndpointTransceiverState>& rtcTransceiverStates)
{
    return WTF::map(rtcTransceiverStates, [this](auto& state) -> PeerConnectionBackend::TransceiverState {
        return { state.mid, mediaStreamsFromRTCStreamIds(state.receiverStreamIds), state.firedDirection };
    });
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionSucceeded()
{
    if (isStopped())
        return;

    callOnMainThread([protectedThis = Ref { *this }, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get(), GatherSignalingState::Yes)), rtcTransceiverStates = crossThreadCopy(transceiverStatesFromPeerConnection(*m_backend)), sctpState = SctpTransportState(m_backend->GetSctpTransport())]() mutable {
        if (protectedThis->isStopped())
            return;

        protectedThis->protectedPeerConnectionBackend()->setLocalDescriptionSucceeded(WTF::move(descriptions), protectedThis->generateTransceiverStates(rtcTransceiverStates), sctpState.createBackend(), sctpState.maxMessageSize());
    });
}

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->protectedPeerConnectionBackend()->setLocalDescriptionFailed(Exception { errorCode, WTF::move(errorMessage) });
    });
}

RefPtr<LibWebRTCPeerConnectionBackend> LibWebRTCMediaEndpoint::protectedPeerConnectionBackend() const
{
    return m_peerConnectionBackend.get();
}

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionSucceeded()
{
    if (isStopped())
        return;

    callOnMainThread([protectedThis = Ref { *this }, descriptions = crossThreadCopy(descriptionsFromPeerConnection(m_backend.get(), GatherSignalingState::Yes)), rtcTransceiverStates = crossThreadCopy(transceiverStatesFromPeerConnection(*m_backend)), sctpState = SctpTransportState(m_backend->GetSctpTransport())]() mutable {
        if (protectedThis->isStopped())
            return;

        protectedThis->protectedPeerConnectionBackend()->setRemoteDescriptionSucceeded(WTF::move(descriptions), protectedThis->generateTransceiverStates(rtcTransceiverStates), sctpState.createBackend(), sctpState.maxMessageSize());
    });
}

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionFailed(ExceptionCode errorCode, const char* errorMessage)
{
    callOnMainThread([protectedThis = Ref { *this }, errorCode, errorMessage = String::fromLatin1(errorMessage)] () mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->protectedPeerConnectionBackend()->setRemoteDescriptionFailed(Exception { errorCode, WTF::move(errorMessage) });
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

    String toJSONString() const
    {
        if (m_jsonString.isNull())
            m_jsonString = String::fromLatin1(m_stats.ToJson().c_str());
        return m_jsonString;
    }

private:
    const webrtc::RTCStats& m_stats;
    mutable String m_jsonString;
};

void LibWebRTCMediaEndpoint::OnStatsDelivered(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
#if !RELEASE_LOG_DISABLED
    int64_t timestamp = report->timestamp().us_or(0);
    if (!m_statsFirstDeliveredTimestamp)
        m_statsFirstDeliveredTimestamp = timestamp;

    callOnMainThread([protectedThis = Ref { *this }, this, timestamp, report] {
        if (protectedThis->isStopped())
            return;

        if (m_statsLogTimer.repeatInterval() != statsLogInterval(timestamp)) {
            m_statsLogTimer.stop();
            m_statsLogTimer.startRepeating(statsLogInterval(timestamp));
        }

        for (auto iterator = report->begin(); iterator != report->end(); ++iterator) {
            RTCStatsLogger statsLogger { *iterator };
            Ref backend = *m_peerConnectionBackend.get();
            if (m_isGatheringRTCLogs) {
                auto event = backend->generateJSONLogEvent(String::fromLatin1(iterator->ToJson().c_str()), true);
                backend->provideStatLogs(WTF::move(event));
            }

#if PLATFORM(WPE) || PLATFORM(GTK)
            if (backend->isJSONLogStreamingEnabled()) {
                auto event = backend->generateJSONLogEvent(String::fromLatin1(iterator->ToJson().c_str()), false);
                backend->emitJSONLogEvent(WTF::move(event));
            }
#endif

            // Stats are very verbose, let's only display them in inspector console in verbose mode.
            logger().toObservers(LogWebRTC, WTFLogLevel::Debug, Logger::LogSiteIdentifier("LibWebRTCMediaEndpoint"_s, "OnStatsDelivered"_s, logIdentifier()), statsLogger);

            RELEASE_LOG_FORWARDABLE(WebRTCStats, LIBWEBRTCMEDIAENDPOINT_ONSTATSDELIVERED, logIdentifier(), statsLogger.toJSONString().utf8());
        }
    });
#else // !RELEASE_LOG_DISABLED
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
    if (m_isGatheringRTCLogs)
        return 1_s;

    if (logger().willLog(logChannel(), WTFLogLevel::Info))
        return 2_s;

    if (reportTimestamp - m_statsFirstDeliveredTimestamp > 15000000)
        return 10_s;

    return 4_s;
}
#endif

void LibWebRTCMediaEndpoint::startRTCLogs()
{
    m_isGatheringRTCLogs = true;
    startLoggingStats();
}

void LibWebRTCMediaEndpoint::stopRTCLogs()
{
    m_isGatheringRTCLogs = false;
}

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


#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
