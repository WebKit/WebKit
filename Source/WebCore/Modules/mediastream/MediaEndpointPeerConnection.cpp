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
#include "MediaEndpointSessionDescription.h"
#include "MediaStream.h"
#include "MediaStreamEvent.h"
#include "MediaStreamTrack.h"
#include "NotImplemented.h"
#include "PeerMediaDescription.h"
#include "RTCConfiguration.h"
#include "RTCIceCandidate.h"
#include "RTCIceCandidateEvent.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCPeerConnection.h"
#include "RTCRtpTransceiver.h"
#include "RTCTrackEvent.h"
#include "SDPProcessor.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;
using namespace PeerConnectionStates;

using MediaDescriptionVector = Vector<PeerMediaDescription>;
using RtpTransceiverVector = Vector<RefPtr<RTCRtpTransceiver>>;

// We use base64 to generate the random strings so we need a size that avoids padding to get ice-chars.
static const size_t cnameSize = 18;
// Size range from 4 to 256 ice-chars defined in RFC 5245.
static const size_t iceUfragSize = 6;
// Size range from 22 to 256 ice-chars defined in RFC 5245.
static const size_t icePasswordSize = 24;

#if !USE(LIBWEBRTC)
static std::unique_ptr<PeerConnectionBackend> createMediaEndpointPeerConnection(RTCPeerConnection& peerConnection)
{
    return std::unique_ptr<PeerConnectionBackend>(new MediaEndpointPeerConnection(peerConnection));
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createMediaEndpointPeerConnection;
#endif

static String randomString(size_t size)
{
    unsigned char randomValues[size];
    cryptographicallyRandomValues(randomValues, size);
    return base64Encode(randomValues, size);
}

MediaEndpointPeerConnection::MediaEndpointPeerConnection(RTCPeerConnection& peerConnection)
    : PeerConnectionBackend(peerConnection)
    , m_mediaEndpoint(MediaEndpoint::create(*this))
    , m_sdpProcessor(std::make_unique<SDPProcessor>(m_peerConnection.scriptExecutionContext()))
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

void MediaEndpointPeerConnection::doCreateOffer(RTCOfferOptions&& options)
{
    runTask([this, protectedOptions = WTFMove(options)]() mutable {
        createOfferTask(protectedOptions);
    });
}

void MediaEndpointPeerConnection::createOfferTask(const RTCOfferOptions&)
{
    ASSERT(!m_dtlsFingerprint.isEmpty());

    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    RefPtr<MediaEndpointSessionConfiguration> configurationSnapshot = localDescription ?
        localDescription->configuration()->clone() : MediaEndpointSessionConfiguration::create();

    configurationSnapshot->setSessionVersion(m_sdpOfferSessionVersion++);

    auto transceivers = RtpTransceiverVector(m_peerConnection.getTransceivers());

    // Remove any transceiver objects from transceivers that can be matched to an existing media description.
    for (auto& mediaDescription : configurationSnapshot->mediaDescriptions()) {
        if (!mediaDescription.port) {
            // This media description should be recycled.
            continue;
        }

        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, mediaDescription.mid);
        if (!transceiver)
            continue;

        mediaDescription.mode = transceiver->directionString();
        if (transceiver->hasSendingDirection()) {
            auto& sender = transceiver->sender();

            mediaDescription.mediaStreamId = sender.mediaStreamIds()[0];
            mediaDescription.mediaStreamTrackId = sender.trackId();
        }

        transceivers.removeFirst(transceiver);
    }

    // Add media descriptions for remaining transceivers.
    for (auto& transceiver : transceivers) {
        PeerMediaDescription mediaDescription;
        auto& sender = transceiver->sender();

        mediaDescription.mode = transceiver->directionString();
        mediaDescription.mid = transceiver->provisionalMid();
        mediaDescription.mediaStreamId = sender.mediaStreamIds()[0];
        mediaDescription.type = sender.trackKind();
        mediaDescription.payloads = sender.trackKind() == "audio" ? m_defaultAudioPayloads : m_defaultVideoPayloads;
        mediaDescription.dtlsFingerprintHashFunction = m_dtlsFingerprintFunction;
        mediaDescription.dtlsFingerprint = m_dtlsFingerprint;
        mediaDescription.cname = m_cname;
        mediaDescription.addSsrc(cryptographicallyRandomNumber());
        mediaDescription.iceUfrag = m_iceUfrag;
        mediaDescription.icePassword = m_icePassword;

        if (sender.track())
            mediaDescription.mediaStreamTrackId = sender.trackId();

        configurationSnapshot->addMediaDescription(WTFMove(mediaDescription));
    }

    String sdp;
    SDPProcessor::Result result = m_sdpProcessor->generate(*configurationSnapshot, sdp);
    if (result != SDPProcessor::Result::Success) {
        createOfferFailed(Exception { OperationError, "SDPProcessor internal error" });
        return;
    }
    createOfferSucceeded(WTFMove(sdp));
}

void MediaEndpointPeerConnection::doCreateAnswer(RTCAnswerOptions&& options)
{
    runTask([this, protectedOptions = WTFMove(options)]() mutable {
        createAnswerTask(protectedOptions);
    });
}

void MediaEndpointPeerConnection::createAnswerTask(const RTCAnswerOptions&)
{
    ASSERT(!m_dtlsFingerprint.isEmpty());

    if (!internalRemoteDescription()) {
        createAnswerFailed(Exception { INVALID_STATE_ERR, "No remote description set" });
        return;
    }

    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    RefPtr<MediaEndpointSessionConfiguration> configurationSnapshot = localDescription ?
        localDescription->configuration()->clone() : MediaEndpointSessionConfiguration::create();

    configurationSnapshot->setSessionVersion(m_sdpAnswerSessionVersion++);

    auto transceivers = RtpTransceiverVector(m_peerConnection.getTransceivers());
    auto& remoteMediaDescriptions = internalRemoteDescription()->configuration()->mediaDescriptions();

    for (unsigned i = 0; i < remoteMediaDescriptions.size(); ++i) {
        auto& remoteMediaDescription = remoteMediaDescriptions[i];

        auto* transceiver = matchTransceiverByMid(transceivers, remoteMediaDescription.mid);
        if (!transceiver) {
            LOG_ERROR("Could not find a matching transceiver for remote description while creating answer");
            continue;
        }

        if (i >= configurationSnapshot->mediaDescriptions().size()) {
            PeerMediaDescription newMediaDescription;

            auto& sender = transceiver->sender();
            if (sender.track()) {
                if (sender.mediaStreamIds().size())
                    newMediaDescription.mediaStreamId = sender.mediaStreamIds()[0];
                newMediaDescription.mediaStreamTrackId = sender.trackId();
                newMediaDescription.addSsrc(cryptographicallyRandomNumber());
            }

            newMediaDescription.mode = transceiver->directionString();
            newMediaDescription.type = remoteMediaDescription.type;
            newMediaDescription.mid = remoteMediaDescription.mid;
            newMediaDescription.dtlsSetup = remoteMediaDescription.dtlsSetup == "active" ? "passive" : "active";
            newMediaDescription.dtlsFingerprintHashFunction = m_dtlsFingerprintFunction;
            newMediaDescription.dtlsFingerprint = m_dtlsFingerprint;
            newMediaDescription.cname = m_cname;
            newMediaDescription.iceUfrag = m_iceUfrag;
            newMediaDescription.icePassword = m_icePassword;

            configurationSnapshot->addMediaDescription(WTFMove(newMediaDescription));
        }

        PeerMediaDescription& localMediaDescription = configurationSnapshot->mediaDescriptions()[i];

        localMediaDescription.payloads = remoteMediaDescription.payloads;
        localMediaDescription.rtcpMux = remoteMediaDescription.rtcpMux;

        if (!localMediaDescription.ssrcs.size())
            localMediaDescription.addSsrc(cryptographicallyRandomNumber());

        if (localMediaDescription.dtlsSetup == "actpass")
            localMediaDescription.dtlsSetup = "passive";

        transceivers.removeFirst(transceiver);
    }

    // Unassociated (non-stopped) transceivers need to be negotiated in a follow-up offer.
    if (hasUnassociatedTransceivers(transceivers))
        markAsNeedingNegotiation();

    String sdp;
    SDPProcessor::Result result = m_sdpProcessor->generate(*configurationSnapshot, sdp);
    if (result != SDPProcessor::Result::Success) {
        createAnswerFailed(Exception { OperationError, "SDPProcessor internal error" });
        return;
    }
    createAnswerSucceeded(WTFMove(sdp));
}

static RealtimeMediaSourceMap createSourceMap(const MediaDescriptionVector& remoteMediaDescriptions, unsigned localMediaDescriptionCount, const RtpTransceiverVector& transceivers)
{
    RealtimeMediaSourceMap sourceMap;

    for (unsigned i = 0; i < remoteMediaDescriptions.size() && i < localMediaDescriptionCount; ++i) {
        auto& remoteMediaDescription = remoteMediaDescriptions[i];
        if (remoteMediaDescription.type != "audio" && remoteMediaDescription.type != "video")
            continue;

        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, remoteMediaDescription.mid);
        if (transceiver) {
            if (transceiver->hasSendingDirection() && transceiver->sender().track())
                sourceMap.set(transceiver->mid(), &transceiver->sender().track()->source());
        }
    }

    return sourceMap;
}

void MediaEndpointPeerConnection::doSetLocalDescription(RTCSessionDescription& description)
{
    runTask([this, protectedDescription = RefPtr<RTCSessionDescription>(&description)]() mutable {
        setLocalDescriptionTask(WTFMove(protectedDescription));
    });
}

void MediaEndpointPeerConnection::setLocalDescriptionTask(RefPtr<RTCSessionDescription>&& description)
{
    if (m_peerConnection.internalSignalingState() == SignalingState::Closed)
        return;

    auto result = MediaEndpointSessionDescription::create(WTFMove(description), *m_sdpProcessor);
    if (result.hasException()) {
        setLocalDescriptionFailed(result.releaseException());
        return;
    }
    auto newDescription = result.releaseReturnValue();

    const RtpTransceiverVector& transceivers = m_peerConnection.getTransceivers();
    const MediaDescriptionVector& mediaDescriptions = newDescription->configuration()->mediaDescriptions();
    MediaEndpointSessionDescription* localDescription = internalLocalDescription();
    unsigned previousNumberOfMediaDescriptions = localDescription ? localDescription->configuration()->mediaDescriptions().size() : 0;
    bool hasNewMediaDescriptions = mediaDescriptions.size() > previousNumberOfMediaDescriptions;
    bool isInitiator = newDescription->type() == RTCSessionDescription::SdpType::Offer;

    if (hasNewMediaDescriptions) {
        MediaEndpoint::UpdateResult result = m_mediaEndpoint->updateReceiveConfiguration(newDescription->configuration(), isInitiator);

        if (result == MediaEndpoint::UpdateResult::SuccessWithIceRestart) {
            if (m_peerConnection.internalIceGatheringState() != IceGatheringState::Gathering)
                m_peerConnection.updateIceGatheringState(IceGatheringState::Gathering);

            if (m_peerConnection.internalIceConnectionState() != IceConnectionState::Completed)
                m_peerConnection.updateIceConnectionState(IceConnectionState::Connected);

            LOG_ERROR("ICE restart is not implemented");
            notImplemented();

        } else if (result == MediaEndpoint::UpdateResult::Failed) {
            setLocalDescriptionFailed(Exception { OperationError, "Unable to apply session description" });
            return;
        }

        // Associate media descriptions with transceivers (set provisional mid to 'final' mid).
        for (auto& mediaDescription : mediaDescriptions) {
            RTCRtpTransceiver* transceiver = matchTransceiver(transceivers, [&mediaDescription] (RTCRtpTransceiver& current) {
                return current.provisionalMid() == mediaDescription.mid;
            });
            if (transceiver)
                transceiver->setMid(transceiver->provisionalMid());
        }
    }

    if (internalRemoteDescription()) {
        MediaEndpointSessionConfiguration* remoteConfiguration = internalRemoteDescription()->configuration();
        RealtimeMediaSourceMap sendSourceMap = createSourceMap(remoteConfiguration->mediaDescriptions(), mediaDescriptions.size(), transceivers);

        if (m_mediaEndpoint->updateSendConfiguration(remoteConfiguration, sendSourceMap, isInitiator) == MediaEndpoint::UpdateResult::Failed) {
            setLocalDescriptionFailed(Exception { OperationError, "Unable to apply session description" });
            return;
        }
    }

    if (!hasUnassociatedTransceivers(transceivers))
        clearNegotiationNeededState();

    SignalingState newSignalingState;

    // Update state and local descriptions according to setLocal/RemoteDescription processing model
    switch (newDescription->type()) {
    case RTCSessionDescription::SdpType::Offer:
        m_pendingLocalDescription = WTFMove(newDescription);
        newSignalingState = SignalingState::HaveLocalOffer;
        break;

    case RTCSessionDescription::SdpType::Answer:
        m_currentLocalDescription = WTFMove(newDescription);
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
        m_pendingLocalDescription = WTFMove(newDescription);
        newSignalingState = SignalingState::HaveLocalPrAnswer;
        break;
    }

    updateSignalingState(newSignalingState);

    if (m_peerConnection.internalIceGatheringState() == IceGatheringState::New && mediaDescriptions.size())
        m_peerConnection.updateIceGatheringState(IceGatheringState::Gathering);

    markAsNeedingNegotiation();
    setLocalDescriptionSucceeded();
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

void MediaEndpointPeerConnection::doSetRemoteDescription(RTCSessionDescription& description)
{
    runTask([this, protectedDescription = RefPtr<RTCSessionDescription>(&description)]() mutable {
        setRemoteDescriptionTask(WTFMove(protectedDescription));
    });
}

void MediaEndpointPeerConnection::setRemoteDescriptionTask(RefPtr<RTCSessionDescription>&& description)
{
    auto result = MediaEndpointSessionDescription::create(WTFMove(description), *m_sdpProcessor);
    if (result.hasException()) {
        setRemoteDescriptionFailed(result.releaseException());
        return;
    }
    auto newDescription = result.releaseReturnValue();

    auto& mediaDescriptions = newDescription->configuration()->mediaDescriptions();
    for (auto& mediaDescription : mediaDescriptions) {
        if (mediaDescription.type != "audio" && mediaDescription.type != "video")
            continue;

        mediaDescription.payloads = m_mediaEndpoint->filterPayloads(mediaDescription.payloads, mediaDescription.type == "audio" ? m_defaultAudioPayloads : m_defaultVideoPayloads);
    }

    bool isInitiator = newDescription->type() == RTCSessionDescription::SdpType::Answer;
    const RtpTransceiverVector& transceivers = m_peerConnection.getTransceivers();

    RealtimeMediaSourceMap sendSourceMap;
    if (internalLocalDescription())
        sendSourceMap = createSourceMap(mediaDescriptions, internalLocalDescription()->configuration()->mediaDescriptions().size(), transceivers);

    if (m_mediaEndpoint->updateSendConfiguration(newDescription->configuration(), sendSourceMap, isInitiator) == MediaEndpoint::UpdateResult::Failed) {
        setRemoteDescriptionFailed(Exception { OperationError, "Unable to apply session description" });
        return;
    }

    // One legacy MediaStreamEvent will be fired for every new MediaStream created as this remote description is set.
    Vector<RefPtr<MediaStreamEvent>> legacyMediaStreamEvents;

    for (auto& mediaDescription : mediaDescriptions) {
        RTCRtpTransceiver* transceiver = matchTransceiverByMid(transceivers, mediaDescription.mid);
        if (!transceiver) {
            bool receiveOnlyFlag = false;

            if (mediaDescription.mode == "sendrecv" || mediaDescription.mode == "recvonly") {
                // Try to match an existing transceiver.
                transceiver = matchTransceiver(transceivers, [&mediaDescription] (RTCRtpTransceiver& current) {
                    return !current.stopped() && current.mid().isNull() && current.sender().trackKind() == mediaDescription.type;
                });

                if (transceiver) {
                    // This transceiver was created locally with a provisional mid. Its real mid will now be set by the remote
                    // description so we need to update the mid of the transceiver's muted source to preserve the association.
                    transceiver->setMid(mediaDescription.mid);
                    m_mediaEndpoint->replaceMutedRemoteSourceMid(transceiver->provisionalMid(), mediaDescription.mid);
                } else
                    receiveOnlyFlag = true;
            }

            if (!transceiver) {
                auto sender = RTCRtpSender::create(mediaDescription.type, Vector<String>(), m_peerConnection.senderClient());
                auto receiver = createReceiver(mediaDescription.mid, mediaDescription.type, mediaDescription.mediaStreamTrackId);

                auto newTransceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver));
                newTransceiver->setMid(mediaDescription.mid);
                if (receiveOnlyFlag)
                    newTransceiver->disableSendingDirection();

                transceiver = newTransceiver.ptr();
                m_peerConnection.addTransceiver(WTFMove(newTransceiver));
            }
        }

        if (mediaDescription.mode == "sendrecv" || mediaDescription.mode == "sendonly") {
            auto& receiver = transceiver->receiver();
            if (receiver.isDispatched())
                continue;
            receiver.setDispatched(true);

            Vector<String> mediaStreamIds;
            if (!mediaDescription.mediaStreamId.isEmpty())
                mediaStreamIds.append(mediaDescription.mediaStreamId);

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
                    auto newStream = MediaStream::create(*m_peerConnection.scriptExecutionContext(), MediaStreamTrackVector({ receiver.track() }));
                    m_remoteStreamMap.add(id, newStream.copyRef());
                    legacyMediaStreamEvents.append(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, newStream.copyRef()));
                    trackEventMediaStreams.add(id, WTFMove(newStream));
                }
            }

            Vector<RefPtr<MediaStream>> streams;
            copyValuesToVector(trackEventMediaStreams, streams);

            m_peerConnection.fireEvent(RTCTrackEvent::create(eventNames().trackEvent, false, false,
                &receiver, receiver.track(), WTFMove(streams), transceiver));
        }
    }

    // Fire legacy addstream events.
    for (auto& event : legacyMediaStreamEvents)
        m_peerConnection.fireEvent(*event);

    SignalingState newSignalingState;

    // Update state and local descriptions according to setLocal/RemoteDescription processing model
    switch (newDescription->type()) {
    case RTCSessionDescription::SdpType::Offer:
        m_pendingRemoteDescription = WTFMove(newDescription);
        newSignalingState = SignalingState::HaveRemoteOffer;
        break;

    case RTCSessionDescription::SdpType::Answer:
        m_currentRemoteDescription = WTFMove(newDescription);
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
        m_pendingRemoteDescription = WTFMove(newDescription);
        newSignalingState = SignalingState::HaveRemotePrAnswer;
        break;
    }

    updateSignalingState(newSignalingState);
    setRemoteDescriptionSucceeded();
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

void MediaEndpointPeerConnection::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    m_mediaEndpoint->setConfiguration(WTFMove(configuration));
}

void MediaEndpointPeerConnection::doAddIceCandidate(RTCIceCandidate& rtcCandidate)
{
    runTask([this, protectedCandidate = RefPtr<RTCIceCandidate>(&rtcCandidate)]() mutable {
        addIceCandidateTask(*protectedCandidate);
    });
}

void MediaEndpointPeerConnection::addIceCandidateTask(RTCIceCandidate& rtcCandidate)
{
    if (!internalRemoteDescription()) {
        addIceCandidateFailed(Exception { INVALID_STATE_ERR, "No remote description set" });
        return;
    }

    auto& remoteMediaDescriptions = internalRemoteDescription()->configuration()->mediaDescriptions();
    PeerMediaDescription* targetMediaDescription = nullptr;

    // When identifying the target media description, sdpMid takes precedence over sdpMLineIndex
    // if both are present.
    if (!rtcCandidate.sdpMid().isNull()) {
        const String& mid = rtcCandidate.sdpMid();
        for (auto& description : remoteMediaDescriptions) {
            if (description.mid == mid) {
                targetMediaDescription = &description;
                break;
            }
        }

        if (!targetMediaDescription) {
            addIceCandidateFailed(Exception { OperationError, "sdpMid did not match any media description" });
            return;
        }
    } else if (rtcCandidate.sdpMLineIndex()) {
        unsigned short sdpMLineIndex = rtcCandidate.sdpMLineIndex().value();
        if (sdpMLineIndex >= remoteMediaDescriptions.size()) {
            addIceCandidateFailed(Exception { OperationError, "sdpMLineIndex is out of range" });
            return;
        }
        targetMediaDescription = &remoteMediaDescriptions[sdpMLineIndex];
    } else {
        ASSERT_NOT_REACHED();
        return;
    }

    auto result = m_sdpProcessor->parseCandidateLine(rtcCandidate.candidate());
    if (result.parsingStatus() != SDPProcessor::Result::Success) {
        if (result.parsingStatus() == SDPProcessor::Result::ParseError)
            addIceCandidateFailed(Exception { OperationError, "Invalid candidate content" });
        else
            LOG_ERROR("SDPProcessor internal error");
        return;
    }

    ASSERT(targetMediaDescription);
    m_mediaEndpoint->addRemoteCandidate(result.candidate(), targetMediaDescription->mid, targetMediaDescription->iceUfrag, targetMediaDescription->icePassword);

    targetMediaDescription->addIceCandidate(WTFMove(result.candidate()));

    addIceCandidateSucceeded();
}

void MediaEndpointPeerConnection::getStats(MediaStreamTrack* track, Ref<DeferredPromise>&& promise)
{
    m_mediaEndpoint->getStats(track, WTFMove(promise));
}

Vector<RefPtr<MediaStream>> MediaEndpointPeerConnection::getRemoteStreams() const
{
    Vector<RefPtr<MediaStream>> remoteStreams;
    copyValuesToVector(m_remoteStreamMap, remoteStreams);
    return remoteStreams;
}

Ref<RTCRtpReceiver> MediaEndpointPeerConnection::createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId)
{
    RealtimeMediaSource::Type sourceType = trackKind == "audio" ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video;

    // Create a muted remote source that will be unmuted once media starts arriving.
    auto remoteSource = m_mediaEndpoint->createMutedRemoteSource(transceiverMid, sourceType);
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(WTFMove(remoteSource), String(trackId));
    auto remoteTrack = MediaStreamTrack::create(*m_peerConnection.scriptExecutionContext(), WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(WTFMove(remoteTrack));
}

std::unique_ptr<RTCDataChannelHandler> MediaEndpointPeerConnection::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_mediaEndpoint->createDataChannelHandler(label, options);
}

void MediaEndpointPeerConnection::replaceTrack(RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& withTrack, DOMPromise<void>&& promise)
{
    RTCRtpTransceiver* transceiver = matchTransceiver(m_peerConnection.getTransceivers(), [&sender] (RTCRtpTransceiver& current) {
        return &current.sender() == &sender;
    });
    ASSERT(transceiver);

    const String& mid = transceiver->mid();
    if (mid.isNull()) {
        // Transceiver is not associated with a media description yet.
        sender.setTrack(WTFMove(withTrack));
        promise.resolve();
        return;
    }

    runTask([this, protectedSender = RefPtr<RTCRtpSender>(&sender), mid, protectedTrack = WTFMove(withTrack), protectedPromise = WTFMove(promise)]() mutable {
        replaceTrackTask(*protectedSender, mid, WTFMove(protectedTrack), protectedPromise);
    });
}

void MediaEndpointPeerConnection::replaceTrackTask(RTCRtpSender& sender, const String& mid, RefPtr<MediaStreamTrack>&& withTrack, DOMPromise<void>& promise)
{
    if (m_peerConnection.internalSignalingState() == SignalingState::Closed)
        return;

    m_mediaEndpoint->replaceSendSource(withTrack->source(), mid);

    sender.setTrack(WTFMove(withTrack));
    promise.resolve();
}

void MediaEndpointPeerConnection::doStop()
{
    m_mediaEndpoint->stop();
}

void MediaEndpointPeerConnection::emulatePlatformEvent(const String& action)
{
    m_mediaEndpoint->emulatePlatformEvent(action);
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

void MediaEndpointPeerConnection::gotIceCandidate(const String& mid, IceCandidate&& candidate)
{
    ASSERT(isMainThread());

    auto& mediaDescriptions = internalLocalDescription()->configuration()->mediaDescriptions();
    size_t mediaDescriptionIndex = notFound;

    for (size_t i = 0; i < mediaDescriptions.size(); ++i) {
        if (mediaDescriptions[i].mid == mid) {
            mediaDescriptionIndex = i;
            break;
        }
    }
    ASSERT(mediaDescriptionIndex != notFound);

    String candidateLine;
    auto result = m_sdpProcessor->generateCandidateLine(candidate, candidateLine);
    if (result != SDPProcessor::Result::Success) {
        LOG_ERROR("SDPProcessor internal error");
        return;
    }

    mediaDescriptions[mediaDescriptionIndex].addIceCandidate(WTFMove(candidate));

    fireICECandidateEvent(RTCIceCandidate::create(candidateLine, mid, mediaDescriptionIndex));
}

void MediaEndpointPeerConnection::doneGatheringCandidates(const String& mid)
{
    ASSERT(isMainThread());

    RtpTransceiverVector transceivers = RtpTransceiverVector(m_peerConnection.getTransceivers());
    RTCRtpTransceiver* notifyingTransceiver = matchTransceiverByMid(transceivers, mid);
    ASSERT(notifyingTransceiver);

    notifyingTransceiver->iceTransport().setGatheringState(RTCIceTransport::GatheringState::Complete);

    // Don't notify the script if there are transceivers still gathering.
    RTCRtpTransceiver* stillGatheringTransceiver = matchTransceiver(transceivers, [] (RTCRtpTransceiver& current) {
        return !current.stopped() && !current.mid().isNull()
            && current.iceTransport().gatheringState() != RTCIceTransport::GatheringState::Complete;
    });
    if (!stillGatheringTransceiver)
        PeerConnectionBackend::doneGatheringCandidates();
}

static RTCIceTransport::TransportState deriveAggregatedIceConnectionState(const Vector<RTCIceTransport::TransportState>& states)
{
    unsigned newCount = 0;
    unsigned checkingCount = 0;
    unsigned connectedCount = 0;
    unsigned completedCount = 0;
    unsigned failedCount = 0;
    unsigned disconnectedCount = 0;
    unsigned closedCount = 0;

    for (auto& state : states) {
        switch (state) {
        case RTCIceTransport::TransportState::New: ++newCount; break;
        case RTCIceTransport::TransportState::Checking: ++checkingCount; break;
        case RTCIceTransport::TransportState::Connected: ++connectedCount; break;
        case RTCIceTransport::TransportState::Completed: ++completedCount; break;
        case RTCIceTransport::TransportState::Failed: ++failedCount; break;
        case RTCIceTransport::TransportState::Disconnected: ++disconnectedCount; break;
        case RTCIceTransport::TransportState::Closed: ++closedCount; break;
        }
    }

    // The aggregated RTCIceConnectionState is derived from the RTCIceTransportState of all RTCIceTransports.
    if ((newCount > 0 && !checkingCount && !failedCount && !disconnectedCount) || (closedCount == states.size()))
        return RTCIceTransport::TransportState::New;

    if (checkingCount > 0 && !failedCount && !disconnectedCount)
        return RTCIceTransport::TransportState::Checking;

    if ((connectedCount + completedCount + closedCount) == states.size() && connectedCount > 0)
        return RTCIceTransport::TransportState::Connected;

    if ((completedCount + closedCount) == states.size() && completedCount > 0)
        return RTCIceTransport::TransportState::Completed;

    if (failedCount > 0)
        return RTCIceTransport::TransportState::Failed;

    if (disconnectedCount > 0) // Any failed caught above.
        return RTCIceTransport::TransportState::Disconnected;

    ASSERT_NOT_REACHED();
    return RTCIceTransport::TransportState::New;
}

void MediaEndpointPeerConnection::iceTransportStateChanged(const String& mid, MediaEndpoint::IceTransportState mediaEndpointIceTransportState)
{
    ASSERT(isMainThread());

    RTCRtpTransceiver* transceiver = matchTransceiverByMid(m_peerConnection.getTransceivers(), mid);
    ASSERT(transceiver);

    RTCIceTransport::TransportState transportState = static_cast<RTCIceTransport::TransportState>(mediaEndpointIceTransportState);
    transceiver->iceTransport().setTransportState(transportState);

    // Determine if the script needs to be notified.
    Vector<RTCIceTransport::TransportState> transportStates;
    for (auto& transceiver : m_peerConnection.getTransceivers())
        transportStates.append(transceiver->iceTransport().transportState());

    RTCIceTransport::TransportState derivedState = deriveAggregatedIceConnectionState(transportStates);
    m_peerConnection.updateIceConnectionState(static_cast<IceConnectionState>(derivedState));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
