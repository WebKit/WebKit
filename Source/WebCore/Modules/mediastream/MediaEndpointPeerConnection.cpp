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

#include "EventNames.h"
#include "JSRTCSessionDescription.h"
#include "MediaEndpointSessionConfiguration.h"
#include "MediaStream.h"
#include "MediaStreamEvent.h"
#include "MediaStreamTrack.h"
#include "PeerMediaDescription.h"
#include "RTCConfiguration.h"
#include "RTCIceCandidate.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCRtpTransceiver.h"
#include "RTCTrackEvent.h"
#include "SDPProcessor.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;
using namespace PeerConnectionStates;

// We use base64 to generate the random strings so we need a size that avoids padding to get ice-chars.
static const size_t cnameSize = 18;
// Size range from 4 to 256 ice-chars defined in RFC 5245.
static const size_t iceUfragSize = 6;
// Size range from 22 to 256 ice-chars defined in RFC 5245.
static const size_t icePasswordSize = 24;

static std::unique_ptr<PeerConnectionBackend> createMediaEndpointPeerConnection(PeerConnectionBackendClient* client)
{
    return std::unique_ptr<PeerConnectionBackend>(new MediaEndpointPeerConnection(client));
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createMediaEndpointPeerConnection;

static String randomString(size_t size)
{
    unsigned char randomValues[size];
    cryptographicallyRandomValues(randomValues, size);
    return base64Encode(randomValues, size);
}

MediaEndpointPeerConnection::MediaEndpointPeerConnection(PeerConnectionBackendClient* client)
    : m_client(client)
    , m_mediaEndpoint(MediaEndpoint::create(*this))
    , m_sdpProcessor(std::unique_ptr<SDPProcessor>(new SDPProcessor(m_client->scriptExecutionContext())))
    , m_cname(randomString(cnameSize))
    , m_iceUfrag(randomString(iceUfragSize))
    , m_icePassword(randomString(icePasswordSize))
{
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

void MediaEndpointPeerConnection::runTask(Function<void ()>&& task)
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

    auto description = MediaEndpointSessionDescription::create(RTCSessionDescription::SdpType::Offer, WTFMove(configurationSnapshot));
    promise.resolve(*description->toRTCSessionDescription(*m_sdpProcessor));
}

void MediaEndpointPeerConnection::createAnswer(RTCAnswerOptions& options, SessionDescriptionPromise&& promise)
{
    runTask([this, protectedOptions = RefPtr<RTCAnswerOptions>(&options), protectedPromise = WTFMove(promise)]() mutable {
        createAnswerTask(*protectedOptions, protectedPromise);
    });
}

void MediaEndpointPeerConnection::createAnswerTask(RTCAnswerOptions&, SessionDescriptionPromise& promise)
{
    ASSERT(!m_dtlsFingerprint.isEmpty());

    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    if (!internalRemoteDescription()) {
        promise.reject(INVALID_STATE_ERR, "No remote description set");
        return;
    }

    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    RefPtr<MediaEndpointSessionConfiguration> configurationSnapshot = localDescription ?
        localDescription->configuration()->clone() : MediaEndpointSessionConfiguration::create();

    configurationSnapshot->setSessionVersion(m_sdpAnswerSessionVersion++);

    auto transceivers = RtpTransceiverVector(m_client->getTransceivers());
    const MediaDescriptionVector& remoteMediaDescriptions = internalRemoteDescription()->configuration()->mediaDescriptions();

    for (unsigned i = 0; i < remoteMediaDescriptions.size(); ++i) {
        PeerMediaDescription& remoteMediaDescription = *remoteMediaDescriptions[i];

        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, remoteMediaDescription.mid());
        if (!transceiver) {
            LOG_ERROR("Could not find a matching transceiver for remote description while creating answer");
            continue;
        }

        if (i >= configurationSnapshot->mediaDescriptions().size()) {
            auto newMediaDescription = PeerMediaDescription::create();

            RTCRtpSender& sender = *transceiver->sender();
            if (sender.track()) {
                if (sender.mediaStreamIds().size())
                    newMediaDescription->setMediaStreamId(sender.mediaStreamIds()[0]);
                newMediaDescription->setMediaStreamTrackId(sender.trackId());
                newMediaDescription->addSsrc(cryptographicallyRandomNumber());
            }

            newMediaDescription->setMode(transceiver->directionString());
            newMediaDescription->setType(remoteMediaDescription.type());
            newMediaDescription->setMid(remoteMediaDescription.mid());
            newMediaDescription->setDtlsSetup(remoteMediaDescription.dtlsSetup() == "active" ? "passive" : "active");
            newMediaDescription->setDtlsFingerprintHashFunction(m_dtlsFingerprintFunction);
            newMediaDescription->setDtlsFingerprint(m_dtlsFingerprint);
            newMediaDescription->setCname(m_cname);
            newMediaDescription->setIceUfrag(m_iceUfrag);
            newMediaDescription->setIcePassword(m_icePassword);

            configurationSnapshot->addMediaDescription(WTFMove(newMediaDescription));
        }

        PeerMediaDescription& localMediaDescription = *configurationSnapshot->mediaDescriptions()[i];

        localMediaDescription.setPayloads(remoteMediaDescription.payloads());
        localMediaDescription.setRtcpMux(remoteMediaDescription.rtcpMux());

        if (!localMediaDescription.ssrcs().size())
            localMediaDescription.addSsrc(cryptographicallyRandomNumber());

        if (localMediaDescription.dtlsSetup() == "actpass")
            localMediaDescription.setDtlsSetup("passive");

        transceivers.removeFirst(transceiver);
    }

    // Unassociated (non-stopped) transceivers need to be negotiated in a follow-up offer.
    if (hasUnassociatedTransceivers(transceivers))
        markAsNeedingNegotiation();

    auto description = MediaEndpointSessionDescription::create(RTCSessionDescription::SdpType::Answer, WTFMove(configurationSnapshot));
    promise.resolve(*description->toRTCSessionDescription(*m_sdpProcessor));
}

static RealtimeMediaSourceMap createSourceMap(const MediaDescriptionVector& remoteMediaDescriptions, unsigned localMediaDescriptionCount, const RtpTransceiverVector& transceivers)
{
    RealtimeMediaSourceMap sourceMap;

    for (unsigned i = 0; i < remoteMediaDescriptions.size() && i < localMediaDescriptionCount; ++i) {
        PeerMediaDescription& remoteMediaDescription = *remoteMediaDescriptions[i];
        if (remoteMediaDescription.type() != "audio" && remoteMediaDescription.type() != "video")
            continue;

        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, remoteMediaDescription.mid());
        if (transceiver) {
            if (transceiver->hasSendingDirection() && transceiver->sender()->track())
                sourceMap.set(transceiver->mid(), &transceiver->sender()->track()->source());
            break;
        }
    }

    return sourceMap;
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

    if (internalRemoteDescription()) {
        MediaEndpointSessionConfiguration* remoteConfiguration = internalRemoteDescription()->configuration();
        RealtimeMediaSourceMap sendSourceMap = createSourceMap(remoteConfiguration->mediaDescriptions(), mediaDescriptions.size(), transceivers);

        if (m_mediaEndpoint->updateSendConfiguration(remoteConfiguration, sendSourceMap, isInitiator) == MediaEndpoint::UpdateResult::Failed) {
            promise.reject(OperationError, "Unable to apply session description");
            return;
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
        m_currentRemoteDescription = m_pendingRemoteDescription;
        m_pendingLocalDescription = nullptr;
        m_pendingRemoteDescription = nullptr;
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
    runTask([this, protectedDescription = RefPtr<RTCSessionDescription>(&description), protectedPromise = WTFMove(promise)]() mutable {
        setRemoteDescriptionTask(WTFMove(protectedDescription), protectedPromise);
    });
}

void MediaEndpointPeerConnection::setRemoteDescriptionTask(RefPtr<RTCSessionDescription>&& description, VoidPromise& promise)
{
    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    ExceptionCodeWithMessage exception;
    auto newDescription = MediaEndpointSessionDescription::create(WTFMove(description), *m_sdpProcessor, exception);
    if (exception.code) {
        promise.reject(exception.code, exception.message);
        return;
    }

    if (!remoteDescriptionTypeValidForState(newDescription->type())) {
        promise.reject(INVALID_STATE_ERR, "Description type incompatible with current signaling state");
        return;
    }

    const MediaDescriptionVector& mediaDescriptions = newDescription->configuration()->mediaDescriptions();
    for (auto& mediaDescription : mediaDescriptions) {
        if (mediaDescription->type() != "audio" && mediaDescription->type() != "video")
            continue;

        mediaDescription->setPayloads(m_mediaEndpoint->filterPayloads(mediaDescription->payloads(),
            mediaDescription->type() == "audio" ? m_defaultAudioPayloads : m_defaultVideoPayloads));
    }

    bool isInitiator = newDescription->type() == RTCSessionDescription::SdpType::Answer;
    const RtpTransceiverVector& transceivers = m_client->getTransceivers();

    RealtimeMediaSourceMap sendSourceMap;
    if (internalLocalDescription())
        sendSourceMap = createSourceMap(mediaDescriptions, internalLocalDescription()->configuration()->mediaDescriptions().size(), transceivers);

    if (m_mediaEndpoint->updateSendConfiguration(newDescription->configuration(), sendSourceMap, isInitiator) == MediaEndpoint::UpdateResult::Failed) {
        promise.reject(OperationError, "Unable to apply session description");
        return;
    }

    // One legacy MediaStreamEvent will be fired for every new MediaStream created as this remote description is set.
    Vector<RefPtr<MediaStreamEvent>> legacyMediaStreamEvents;

    for (auto mediaDescription : mediaDescriptions) {
        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, mediaDescription->mid());
        if (!transceiver) {
            bool receiveOnlyFlag = false;

            if (mediaDescription->mode() == "sendrecv" || mediaDescription->mode() == "recvonly") {
                // Try to match an existing transceiver.
                transceiver = matchTransceiver(transceivers, [&mediaDescription] (RTCRtpTransceiver& current) {
                    return !current.stopped() && current.mid().isNull() && current.sender()->trackKind() == mediaDescription->type();
                });

                if (transceiver)
                    transceiver->setMid(mediaDescription->mid());
                else
                    receiveOnlyFlag = true;
            }

            if (!transceiver) {
                auto sender = RTCRtpSender::create(mediaDescription->type(), Vector<String>(), m_client->senderClient());
                auto receiver = createReceiver(mediaDescription->mid(), mediaDescription->type(), mediaDescription->mediaStreamTrackId());

                auto newTransceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver));
                newTransceiver->setMid(mediaDescription->mid());
                if (receiveOnlyFlag)
                    newTransceiver->disableSendingDirection();

                transceiver = newTransceiver.ptr();
                m_client->addTransceiver(WTFMove(newTransceiver));
            }
        }

        if (mediaDescription->mode() == "sendrecv" || mediaDescription->mode() == "sendonly") {
            RTCRtpReceiver& receiver = *transceiver->receiver();
            if (receiver.isDispatched())
                continue;
            receiver.setDispatched(true);

            Vector<String> mediaStreamIds;
            if (!mediaDescription->mediaStreamId().isEmpty())
                mediaStreamIds.append(mediaDescription->mediaStreamId());

            // A remote track can be associated with 0..* MediaStreams. We create a new stream for
            // a track in case of an unrecognized stream id, or just add the track if the stream
            // already exists.
            HashMap<String, RefPtr<MediaStream>> trackEventMediaStreams;
            for (auto& id : mediaStreamIds) {
                if (m_remoteStreamMap.contains(id)) {
                    RefPtr<MediaStream> stream = m_remoteStreamMap.get(id);
                    stream->addTrack(*receiver.track());
                    trackEventMediaStreams.add(id, WTFMove(stream));
                } else {
                    auto newStream = MediaStream::create(*m_client->scriptExecutionContext(), MediaStreamTrackVector({ receiver.track() }));
                    m_remoteStreamMap.add(id, newStream.copyRef());
                    legacyMediaStreamEvents.append(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, newStream.copyRef()));
                    trackEventMediaStreams.add(id, WTFMove(newStream));
                }
            }

            Vector<RefPtr<MediaStream>> streams;
            copyValuesToVector(trackEventMediaStreams, streams);

            m_client->fireEvent(RTCTrackEvent::create(eventNames().trackEvent, false, false,
                &receiver, receiver.track(), WTFMove(streams), transceiver));
        }
    }

    // Fire legacy addstream events.
    for (auto& event : legacyMediaStreamEvents)
        m_client->fireEvent(*event);

    SignalingState newSignalingState;

    // Update state and local descriptions according to setLocal/RemoteDescription processing model
    switch (newDescription->type()) {
    case RTCSessionDescription::SdpType::Offer:
        m_pendingRemoteDescription = newDescription;
        newSignalingState = SignalingState::HaveRemoteOffer;
        break;

    case RTCSessionDescription::SdpType::Answer:
        m_currentRemoteDescription = newDescription;
        m_currentLocalDescription = m_pendingLocalDescription;
        m_pendingRemoteDescription = nullptr;
        m_pendingLocalDescription = nullptr;
        newSignalingState = SignalingState::Stable;
        break;

    case RTCSessionDescription::SdpType::Rollback:
        m_pendingRemoteDescription = nullptr;
        newSignalingState = SignalingState::Stable;
        break;

    case RTCSessionDescription::SdpType::Pranswer:
        m_pendingRemoteDescription = newDescription;
        newSignalingState = SignalingState::HaveRemotePrAnswer;
        break;
    }

    if (newSignalingState != m_client->internalSignalingState()) {
        m_client->setSignalingState(newSignalingState);
        m_client->fireEvent(Event::create(eventNames().signalingstatechangeEvent, false, false));
    }

    promise.resolve(nullptr);
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::remoteDescription() const
{
    return createRTCSessionDescription(internalRemoteDescription());
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::currentRemoteDescription() const
{
    return createRTCSessionDescription(m_currentRemoteDescription.get());
}

RefPtr<RTCSessionDescription> MediaEndpointPeerConnection::pendingRemoteDescription() const
{
    return createRTCSessionDescription(m_pendingRemoteDescription.get());
}

void MediaEndpointPeerConnection::setConfiguration(RTCConfiguration& configuration)
{
    Vector<RefPtr<IceServerInfo>> iceServers;
    for (auto& server : configuration.iceServers())
        iceServers.append(IceServerInfo::create(server->urls(), server->credential(), server->username()));

    m_mediaEndpoint->setConfiguration(MediaEndpointConfiguration::create(iceServers, configuration.iceTransportPolicy(), configuration.bundlePolicy()));
}

void MediaEndpointPeerConnection::addIceCandidate(RTCIceCandidate& rtcCandidate, PeerConnection::VoidPromise&& promise)
{
    runTask([this, protectedCandidate = RefPtr<RTCIceCandidate>(&rtcCandidate), protectedPromise = WTFMove(promise)]() mutable {
        addIceCandidateTask(*protectedCandidate, protectedPromise);
    });
}

void MediaEndpointPeerConnection::addIceCandidateTask(RTCIceCandidate& rtcCandidate, PeerConnection::VoidPromise& promise)
{
    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    if (!internalRemoteDescription()) {
        promise.reject(INVALID_STATE_ERR, "No remote description set");
        return;
    }

    const MediaDescriptionVector& remoteMediaDescriptions = internalRemoteDescription()->configuration()->mediaDescriptions();
    PeerMediaDescription* targetMediaDescription = nullptr;

    // When identifying the target media description, sdpMid takes precedence over sdpMLineIndex
    // if both are present.
    if (!rtcCandidate.sdpMid().isNull()) {
        const String& mid = rtcCandidate.sdpMid();
        for (auto& description : remoteMediaDescriptions) {
            if (description->mid() == mid) {
                targetMediaDescription = description.get();
                break;
            }
        }

        if (!targetMediaDescription) {
            promise.reject(OperationError, "sdpMid did not match any media description");
            return;
        }
    } else if (rtcCandidate.sdpMLineIndex()) {
        unsigned short sdpMLineIndex = rtcCandidate.sdpMLineIndex().value();
        if (sdpMLineIndex >= remoteMediaDescriptions.size()) {
            promise.reject(OperationError, "sdpMLineIndex is out of range");
            return;
        }
        targetMediaDescription = remoteMediaDescriptions[sdpMLineIndex].get();
    } else {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr<IceCandidate> candidate;
    SDPProcessor::Result result = m_sdpProcessor->parseCandidateLine(rtcCandidate.candidate(), candidate);
    if (result != SDPProcessor::Result::Success) {
        if (result == SDPProcessor::Result::ParseError)
            promise.reject(OperationError, "Invalid candidate content");
        else
            LOG_ERROR("SDPProcessor internal error");
        return;
    }

    targetMediaDescription->addIceCandidate(candidate.copyRef());

    m_mediaEndpoint->addRemoteCandidate(*candidate, targetMediaDescription->mid(), targetMediaDescription->iceUfrag(),
        targetMediaDescription->icePassword());

    promise.resolve(nullptr);
}

void MediaEndpointPeerConnection::getStats(MediaStreamTrack*, PeerConnection::StatsPromise&& promise)
{
    notImplemented();

    promise.reject(NOT_SUPPORTED_ERR);
}

Vector<RefPtr<MediaStream>> MediaEndpointPeerConnection::getRemoteStreams() const
{
    Vector<RefPtr<MediaStream>> remoteStreams;
    copyValuesToVector(m_remoteStreamMap, remoteStreams);
    return remoteStreams;
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

void MediaEndpointPeerConnection::replaceTrack(RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& withTrack, PeerConnection::VoidPromise&& promise)
{
    RTCRtpTransceiver* transceiver = matchTransceiver(m_client->getTransceivers(), [&sender] (RTCRtpTransceiver& current) {
        return current.sender() == &sender;
    });
    ASSERT(transceiver);

    const String& mid = transceiver->mid();
    if (mid.isNull()) {
        // Transceiver is not associated with a media description yet.
        sender.setTrack(WTFMove(withTrack));
        promise.resolve(nullptr);
        return;
    }

    runTask([this, protectedSender = RefPtr<RTCRtpSender>(&sender), mid, protectedTrack = WTFMove(withTrack), protectedPromise = WTFMove(promise)]() mutable {
        replaceTrackTask(*protectedSender, mid, WTFMove(protectedTrack), protectedPromise);
    });
}

void MediaEndpointPeerConnection::replaceTrackTask(RTCRtpSender& sender, const String& mid, RefPtr<MediaStreamTrack>&& withTrack, PeerConnection::VoidPromise& promise)
{
    if (m_client->internalSignalingState() == SignalingState::Closed)
        return;

    m_mediaEndpoint->replaceSendSource(withTrack->source(), mid);

    sender.setTrack(WTFMove(withTrack));
    promise.resolve(nullptr);
}

void MediaEndpointPeerConnection::stop()
{
    notImplemented();
}

void MediaEndpointPeerConnection::markAsNeedingNegotiation()
{
    if (m_negotiationNeeded)
        return;

    m_negotiationNeeded = true;

    if (m_client->internalSignalingState() == SignalingState::Stable)
        m_client->scheduleNegotiationNeededEvent();
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

bool MediaEndpointPeerConnection::remoteDescriptionTypeValidForState(RTCSessionDescription::SdpType type) const
{
    switch (m_client->internalSignalingState()) {
    case SignalingState::Stable:
        return type == RTCSessionDescription::SdpType::Offer;
    case SignalingState::HaveLocalOffer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    case SignalingState::HaveRemoteOffer:
        return type == RTCSessionDescription::SdpType::Offer;
    case SignalingState::HaveRemotePrAnswer:
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

MediaEndpointSessionDescription* MediaEndpointPeerConnection::internalRemoteDescription() const
{
    return m_pendingRemoteDescription ? m_pendingRemoteDescription.get() : m_currentRemoteDescription.get();
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

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
