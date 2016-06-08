/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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

#include "config.h"

#if ENABLE(WEB_RTC)
#include "MediaEndpointPeerConnection.h"

#include "Event.h"
#include "JSRTCSessionDescription.h"
#include "MediaEndpointSessionConfiguration.h"
#include "MediaStreamTrack.h"
#include "PeerMediaDescription.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCRtpTransceiver.h"
#include "SDPProcessor.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;
using namespace PeerConnectionStates;

static std::unique_ptr<PeerConnectionBackend> createMediaEndpointPeerConnection(PeerConnectionBackendClient* client)
{
    return std::unique_ptr<PeerConnectionBackend>(new MediaEndpointPeerConnection(client));
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createMediaEndpointPeerConnection;

static String randomString(size_t length)
{
    const size_t size = ceil(length * 3 / 4);
    unsigned char randomValues[size];
    cryptographicallyRandomValues(randomValues, size);
    return base64Encode(randomValues, size);
}

MediaEndpointPeerConnection::MediaEndpointPeerConnection(PeerConnectionBackendClient* client)
    : m_client(client)
    , m_sdpProcessor(std::unique_ptr<SDPProcessor>(new SDPProcessor(m_client->scriptExecutionContext())))
    , m_cname(randomString(16))
    , m_iceUfrag(randomString(4))
    , m_icePassword(randomString(22))
{
    m_mediaEndpoint = MediaEndpoint::create(*this);
    ASSERT(m_mediaEndpoint);

    m_defaultAudioPayloads = m_mediaEndpoint->getDefaultAudioPayloads();
    m_defaultVideoPayloads = m_mediaEndpoint->getDefaultVideoPayloads();

    // Tasks (see runTask()) will be deferred until we get the DTLS fingerprint.
    m_mediaEndpoint->generateDtlsInfo();
}

static RTCRtpTransceiver* matchTransceiver(const RtpTransceiverVector& transceivers, const std::function<bool(RTCRtpTransceiver&)>& matchFunction)
{
    for (auto& transceiver : transceivers) {
        if (matchFunction(*transceiver))
            return transceiver.get();
    }
    return nullptr;
}

static RTCRtpTransceiver* matchTransceiverByMid(const RtpTransceiverVector& transceivers, const String& mid)
{
    return matchTransceiver(transceivers, [&mid] (RTCRtpTransceiver& current) {
        return current.mid() == mid;
    });
}

static bool hasUnassociatedTransceivers(const RtpTransceiverVector& transceivers)
{
    return matchTransceiver(transceivers, [] (RTCRtpTransceiver& current) {
        return current.mid().isNull() && !current.stopped();
    });
}

void MediaEndpointPeerConnection::runTask(NoncopyableFunction<void ()>&& task)
{
    if (m_dtlsFingerprint.isNull()) {
        // Only one task needs to be deferred since it will hold off any others until completed.
        ASSERT(!m_initialDeferredTask);
        m_initialDeferredTask = WTFMove(task);
    } else
        callOnMainThread(WTFMove(task));
}

void MediaEndpointPeerConnection::startRunningTasks()
{
    if (!m_initialDeferredTask)
        return;

    m_initialDeferredTask();
    m_initialDeferredTask = nullptr;
}

void MediaEndpointPeerConnection::createOffer(RTCOfferOptions& options, SessionDescriptionPromise&& promise)
{
    runTask([this, protectedOptions = RefPtr<RTCOfferOptions>(&options), protectedPromise = WTFMove(promise)]() mutable {
        createOfferTask(*protectedOptions, protectedPromise);
    });
}

void MediaEndpointPeerConnection::createOfferTask(RTCOfferOptions&, SessionDescriptionPromise& promise)
{
    ASSERT(!m_dtlsFingerprint.isEmpty());

    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    RefPtr<MediaEndpointSessionConfiguration> configurationSnapshot = localDescription ?
        localDescription->configuration()->clone() : MediaEndpointSessionConfiguration::create();

    configurationSnapshot->setSessionVersion(m_sdpOfferSessionVersion++);

    auto transceivers = RtpTransceiverVector(m_client->getTransceivers());

    // Remove any transceiver objects from transceivers that can be matched to an existing media description.
    for (auto& mediaDescription : configurationSnapshot->mediaDescriptions()) {
        if (!mediaDescription->port()) {
            // This media description should be recycled.
            continue;
        }

        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, mediaDescription->mid());
        if (!transceiver)
            continue;

        mediaDescription->setMode(transceiver->directionString());
        if (transceiver->hasSendingDirection()) {
            RTCRtpSender& sender = *transceiver->sender();

            mediaDescription->setMediaStreamId(sender.mediaStreamIds()[0]);
            mediaDescription->setMediaStreamTrackId(sender.trackId());
        }

        transceivers.removeFirst(transceiver);
    }

    // Add media descriptions for remaining transceivers.
    for (auto& transceiver : transceivers) {
        RefPtr<PeerMediaDescription> mediaDescription = PeerMediaDescription::create();
        RTCRtpSender& sender = *transceiver->sender();

        mediaDescription->setMode(transceiver->directionString());
        mediaDescription->setMid(transceiver->provisionalMid());
        mediaDescription->setMediaStreamId(sender.mediaStreamIds()[0]);
        mediaDescription->setType(sender.trackKind());
        mediaDescription->setPayloads(sender.trackKind() == "audio" ? m_defaultAudioPayloads : m_defaultVideoPayloads);
        mediaDescription->setDtlsFingerprintHashFunction(m_dtlsFingerprintFunction);
        mediaDescription->setDtlsFingerprint(m_dtlsFingerprint);
        mediaDescription->setCname(m_cname);
        mediaDescription->addSsrc(cryptographicallyRandomNumber());
        mediaDescription->setIceUfrag(m_iceUfrag);
        mediaDescription->setIcePassword(m_icePassword);

        if (sender.track())
            mediaDescription->setMediaStreamTrackId(sender.trackId());

        configurationSnapshot->addMediaDescription(WTFMove(mediaDescription));
    }

    String sdpString;
    SDPProcessor::Result result = m_sdpProcessor->generate(*configurationSnapshot, sdpString);
    if (result != SDPProcessor::Result::Success) {
        LOG_ERROR("SDPProcessor internal error");
        return;
    }

    promise.resolve(RTCSessionDescription::create(RTCSessionDescription::SdpType::Offer, sdpString));
}

void MediaEndpointPeerConnection::createAnswer(RTCAnswerOptions& options, SessionDescriptionPromise&& promise)
{
    UNUSED_PARAM(options);

    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

void MediaEndpointPeerConnection::setLocalDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    runTask([this, protectedDescription = RefPtr<RTCSessionDescription>(&description), protectedPromise = WTFMove(promise)]() mutable {
        setLocalDescriptionTask(WTFMove(protectedDescription), protectedPromise);
    });
}

void MediaEndpointPeerConnection::setLocalDescriptionTask(RefPtr<RTCSessionDescription>&& description, VoidPromise& promise)
{
    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    ExceptionCodeWithMessage exception;
    auto newDescription = MediaEndpointSessionDescription::create(WTFMove(description), *m_sdpProcessor, exception);
    if (exception.code) {
        promise.reject(exception.code, exception.message);
        return;
    }

    if (!localDescriptionTypeValidForState(newDescription->type())) {
        promise.reject(INVALID_STATE_ERR, "Description type incompatible with current signaling state");
        return;
    }

    const RtpTransceiverVector& transceivers = m_client->getTransceivers();
    const MediaDescriptionVector& mediaDescriptions = newDescription->configuration()->mediaDescriptions();
    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    unsigned previousNumberOfMediaDescriptions = localDescription ? localDescription->configuration()->mediaDescriptions().size() : 0;
    bool hasNewMediaDescriptions = mediaDescriptions.size() > previousNumberOfMediaDescriptions;
    bool isInitiator = newDescription->type() == RTCSessionDescription::SdpType::Offer;

    if (hasNewMediaDescriptions) {
        MediaEndpoint::UpdateResult result = m_mediaEndpoint->updateReceiveConfiguration(newDescription->configuration(), isInitiator);

        if (result == MediaEndpoint::UpdateResult::SuccessWithIceRestart) {
            if (m_client->internalIceGatheringState() != IceGatheringState::Gathering)
                m_client->updateIceGatheringState(IceGatheringState::Gathering);

            if (m_client->internalIceConnectionState() != IceConnectionState::Completed)
                m_client->updateIceConnectionState(IceConnectionState::Connected);

            LOG_ERROR("ICE restart is not implemented");
            notImplemented();

        } else if (result == MediaEndpoint::UpdateResult::Failed) {
            promise.reject(OperationError, "Unable to apply session description");
            return;
        }

        // Associate media descriptions with transceivers (set provisional mid to 'final' mid).
        for (unsigned i = previousNumberOfMediaDescriptions; i < mediaDescriptions.size(); ++i) {
            PeerMediaDescription& mediaDescription = *mediaDescriptions[i];

            RTCRtpTransceiver* transceiver = matchTransceiver(transceivers, [&mediaDescription] (RTCRtpTransceiver& current) {
                return current.provisionalMid() == mediaDescription.mid();
            });
            if (transceiver)
                transceiver->setMid(transceiver->provisionalMid());
        }
    }

    if (!hasUnassociatedTransceivers(transceivers))
        clearNegotiationNeededState();

    SignalingState newSignalingState;

    // Update state and local descriptions according to setLocal/RemoteDescription processing model
    switch (newDescription->type()) {
    case RTCSessionDescription::SdpType::Offer:
        m_pendingLocalDescription = newDescription;
        newSignalingState = SignalingState::HaveLocalOffer;
        break;

    case RTCSessionDescription::SdpType::Answer:
        m_currentLocalDescription = newDescription;
        m_pendingLocalDescription = nullptr;
        newSignalingState = SignalingState::Stable;
        break;

    case RTCSessionDescription::SdpType::Rollback:
        m_pendingLocalDescription = nullptr;
        newSignalingState = SignalingState::Stable;
        break;

    case RTCSessionDescription::SdpType::Pranswer:
        m_pendingLocalDescription = newDescription;
        newSignalingState = SignalingState::HaveLocalPrAnswer;
        break;
    }

    if (newSignalingState != m_client->internalSignalingState()) {
        m_client->setSignalingState(newSignalingState);
        m_client->fireEvent(Event::create(eventNames().signalingstatechangeEvent, false, false));
    }

    if (m_client->internalIceGatheringState() == IceGatheringState::New && mediaDescriptions.size())
        m_client->updateIceGatheringState(IceGatheringState::Gathering);

    if (m_client->internalSignalingState() == SignalingState::Stable && m_negotiationNeeded)
        m_client->scheduleNegotiationNeededEvent();

    promise.resolve(nullptr);
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::localDescription() const
{
    return createRTCSessionDescription(internalLocalDescription());
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentLocalDescription() const
{
    return createRTCSessionDescription(m_currentLocalDescription.get());
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingLocalDescription() const
{
    return createRTCSessionDescription(m_pendingLocalDescription.get());
}

void MediaEndpointPeerConnection::setRemoteDescription(RTCSessionDescription& description, VoidPromise&& promise)
{
    UNUSED_PARAM(description);

    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::remoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingRemoteDescription() const
{
    notImplemented();

    return nullptr;
}

void MediaEndpointPeerConnection::setConfiguration(RTCConfiguration& configuration)
{
    UNUSED_PARAM(configuration);

    notImplemented();
}

void MediaEndpointPeerConnection::addIceCandidate(RTCIceCandidate& rtcCandidate, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(rtcCandidate);

    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

void MediaEndpointPeerConnection::getStats(MediaStreamTrack*, PeerConnection::StatsPromise&& promise)
{
    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

RefPtr<RTCRtpReceiver> MediaEndpointPeerConnection::createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId)
{
    RealtimeMediaSource::Type sourceType = trackKind == "audio" ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video;

    // Create a muted remote source that will be unmuted once media starts arriving.
    auto remoteSource = m_mediaEndpoint->createMutedRemoteSource(transceiverMid, sourceType);
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(WTFMove(remoteSource), trackId);
    auto remoteTrack = MediaStreamTrack::create(*m_client->scriptExecutionContext(), *remoteTrackPrivate);

    return RTCRtpReceiver::create(WTFMove(remoteTrack));
}

void MediaEndpointPeerConnection::replaceTrack(RTCRtpSender& sender, MediaStreamTrack& withTrack, PeerConnection::VoidPromise&& promise)
{
    UNUSED_PARAM(sender);
    UNUSED_PARAM(withTrack);
    UNUSED_PARAM(promise);

    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

void MediaEndpointPeerConnection::stop()
{
    notImplemented();
}

void MediaEndpointPeerConnection::markAsNeedingNegotiation()
{
    notImplemented();
}

bool MediaEndpointPeerConnection::localDescriptionTypeValidForState(RTCSessionDescription::SdpType type) const
{
    switch (m_client->internalSignalingState()) {
    case SignalingState::Stable:
        return type == RTCSessionDescription::SdpType::Offer;
    case SignalingState::HaveLocalOffer:
        return type == RTCSessionDescription::SdpType::Offer;
    case SignalingState::HaveRemoteOffer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    case SignalingState::HaveLocalPrAnswer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    default:
        return false;
    };

    ASSERT_NOT_REACHED();
    return false;
}

MediaEndpointSessionDescription* MediaEndpointPeerConnection::internalLocalDescription() const
{
    return m_pendingLocalDescription ? m_pendingLocalDescription.get() : m_currentLocalDescription.get();
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::createRTCSessionDescription(MediaEndpointSessionDescription* description) const
{
    return description ? description->toRTCSessionDescription(*m_sdpProcessor) : nullptr;
}

void MediaEndpointPeerConnection::gotDtlsFingerprint(const String& fingerprint, const String& fingerprintFunction)
{
    ASSERT(isMainThread());

    m_dtlsFingerprint = fingerprint;
    m_dtlsFingerprintFunction = fingerprintFunction;

    startRunningTasks();
}

void MediaEndpointPeerConnection::gotIceCandidate(unsigned mdescIndex, RefPtr<IceCandidate>&& candidate)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);
    UNUSED_PARAM(candidate);

    notImplemented();
}

void MediaEndpointPeerConnection::doneGatheringCandidates(unsigned mdescIndex)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);

    notImplemented();
}

void MediaEndpointPeerConnection::gotRemoteSource(unsigned mdescIndex, RefPtr<RealtimeMediaSource>&& source)
{
    ASSERT(isMainThread());

    UNUSED_PARAM(mdescIndex);
    UNUSED_PARAM(source);

    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
