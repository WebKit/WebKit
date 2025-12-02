/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "PeerConnectionBackend.h"

#if ENABLE(WEB_RTC)

#include "DocumentPage.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCCertificate.h"
#include "Logging.h"
#include "Page.h"
#include "RTCDtlsTransport.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionIceEvent.h"
#include "RTCRtpCapabilities.h"
#include "RTCSctpTransportBackend.h"
#include "RTCSessionDescriptionInit.h"
#include "RTCTrackEvent.h"
#include "WebRTCProvider.h"
#include <algorithm>
#include <wtf/EnumTraits.h>
#include <wtf/FilePrintStream.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#if USE(GSTREAMER_WEBRTC)
#include "GStreamerWebRTCUtils.h"
#endif

#if USE(LIBWEBRTC)
#include "LibWebRTCCertificateGenerator.h"
#include "LibWebRTCProvider.h"
#endif

namespace WebCore {

#if USE(LIBWEBRTC) || USE(GSTREAMER_WEBRTC)

std::optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext& context, const String& kind)
{
    auto* page = downcast<Document>(context).page();
    if (!page)
        return { };
    return page->webRTCProvider().receiverCapabilities(kind);
}

std::optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext& context, const String& kind)
{
    auto* page = downcast<Document>(context).page();
    if (!page)
        return { };
    return page->webRTCProvider().senderCapabilities(kind);
}

#else

static std::unique_ptr<PeerConnectionBackend> createNoPeerConnectionBackend(RTCPeerConnection&, MediaEndpointConfiguration&&)
{
    return nullptr;
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createNoPeerConnectionBackend;

std::optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext&, const String&)
{
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext&, const String&)
{
    ASSERT_NOT_REACHED();
    return { };
}
#endif // USE(LIBWEBRTC) || USE(GSTREAMER_WEBRTC)

#if PLATFORM(WPE) || PLATFORM(GTK)
class JSONFileHandler {
public:
    JSONFileHandler(String&& path)
        : m_path(WTFMove(path))
    {
        Locker lock(m_clientsLock);
        open(true);
    }

    void log(String&& event)
    {
        Locker lock(m_clientsLock);
        if (m_logFile)
            m_logFile->println(WTFMove(event));
    }

    void addClient(uint64_t identifier)
    {
        Locker lock(m_clientsLock);
        m_clients.append(identifier);
        if (!m_logFile)
            open(false);
    }

    void removeClient(uint64_t identifier)
    {
        Locker lock(m_clientsLock);
        if (!m_clients.contains(identifier))
            return;

        m_clients.removeFirst(identifier);
        if (m_clients.isEmpty())
            m_logFile = nullptr;
    }

private:
    void open(bool overwrite)
    {
        ASSERT(!m_logFile);
        ASSERT(m_clientsLock.isHeld());

        m_logFile = FilePrintStream::open(m_path.utf8().data(), overwrite ? "w" : "a");

        // Prefer unbuffered output, so that we get a full log upon crash or deadlock.
        setvbuf(m_logFile->file(), nullptr, _IONBF, 0);
    }

    String m_path;
    Lock m_clientsLock;
    std::unique_ptr<FilePrintStream> m_logFile WTF_GUARDED_BY_LOCK(m_clientsLock);
    Vector<uint64_t> m_clients WTF_GUARDED_BY_LOCK(m_clientsLock);
};

JSONFileHandler& jsonFileHandler()
{
    auto path = String::fromUTF8(getenv("WEBKIT_WEBRTC_JSON_EVENTS_FILE"));
    ASSERT(!path.isEmpty());
    static NeverDestroyed<JSONFileHandler> sharedInstance(WTFMove(path));
    return sharedInstance;
}
#endif

PeerConnectionBackend::PeerConnectionBackend(RTCPeerConnection& peerConnection)
    : m_peerConnection(peerConnection)
#if !RELEASE_LOG_DISABLED
    , m_logger(peerConnection.logger())
    , m_logIdentifier(peerConnection.logIdentifier())
#endif
{
#if USE(LIBWEBRTC)
    RefPtr document = peerConnection.document();
    if (auto* page = document ? document->page() : nullptr)
        m_shouldFilterICECandidates = page->webRTCProvider().isSupportingMDNS();
#endif

#if RELEASE_LOG_DISABLED
    m_logIdentifierString = makeString(hex(reinterpret_cast<uintptr_t>(this)));
#endif

#if !RELEASE_LOG_DISABLED && (PLATFORM(WPE) || PLATFORM(GTK))
    m_jsonFilePath = String::fromUTF8(getenv("WEBKIT_WEBRTC_JSON_EVENTS_FILE"));
    if (!m_jsonFilePath.isEmpty())
        jsonFileHandler().addClient(m_logIdentifier);

    m_logger->addMessageHandlerObserver(*this);
    ALWAYS_LOG(LOGIDENTIFIER, "PeerConnection created"_s);
#endif
}

PeerConnectionBackend::~PeerConnectionBackend()
{
#if !RELEASE_LOG_DISABLED && (PLATFORM(WPE) || PLATFORM(GTK))
    ALWAYS_LOG(LOGIDENTIFIER, "Disposing PeerConnection"_s);
    m_logger->removeMessageHandlerObserver(*this);

    if (isJSONLogStreamingEnabled())
        jsonFileHandler().removeClient(m_logIdentifier);
#endif
}

#if !RELEASE_LOG_DISABLED && (PLATFORM(WPE) || PLATFORM(GTK))
void PeerConnectionBackend::handleLogMessage(const WTFLogChannel& channel, WTFLogLevel, Vector<JSONLogValue>&& values)
{
    auto name = StringView::fromLatin1(channel.name);
    if (name != "WebRTC"_s)
        return;

    // Ignore logs containing only the call site information or JSON logs.
    if (values.size() < 2 || values[1].type == JSONLogValue::Type::JSON)
        return;

    if (!isJSONLogStreamingEnabled())
        return;

    // Parse "foo::bar(hexidentifier) "
    auto& callSite = values[0].value;
    auto leftParenthesisIndex = callSite.reverseFind('(');
    if (leftParenthesisIndex == notFound)
        return;

    auto rightParenthesisIndex = callSite.reverseFind(')');
    if (rightParenthesisIndex == notFound)
        return;

    if (!m_logIdentifierString)
        m_logIdentifierString = makeString(m_logIdentifier);

    auto identifier = callSite.substring(leftParenthesisIndex + 1, rightParenthesisIndex - leftParenthesisIndex - 1);
    if (identifier != m_logIdentifierString)
        return;

    String event;

    // Check if the third message is a multi-lines string, concatenating such message would look ugly in log events.
    if (values.size() >= 3 && values[2].value.find("\r\n"_s) != notFound)
        event = generateJSONLogEvent(MessageLogEvent { values[1].value, { byteCast<uint8_t>(values[2].value.span8()) } }, false);
    else {
        StringBuilder builder;
        for (auto& value : values.subvector(1))
            builder.append(WTF::makeStringByReplacingAll(value.value, '\"', '\''));
        event = generateJSONLogEvent(MessageLogEvent { builder.toString(), { } }, false);
    }
    emitJSONLogEvent(WTFMove(event));
}
#endif // !RELEASE_LOG_DISABLED && (PLATFORM(WPE) || PLATFORM(GTK))

void PeerConnectionBackend::createOffer(RTCOfferOptions&& options, CreateCallback&& callback)
{
    ASSERT(!m_offerAnswerCallback);
    ASSERT(!m_peerConnection->isClosed());

    m_offerAnswerCallback = WTFMove(callback);
    doCreateOffer(WTFMove(options));
}

void PeerConnectionBackend::createOfferSucceeded(String&& sdp)
{
    ASSERT(isMainThread());

#if !RELEASE_LOG_DISABLED
    logger().toObservers(LogWebRTC, WTFLogLevel::Always, LOGIDENTIFIER, "SDP offer created:\n", sdp);
    RELEASE_LOG_FORWARDABLE(WebRTC, PEERCONNECTIONBACKEND_CREATEOFFERSUCCEEDED, logIdentifier(), sdp.utf8());
#endif

    ASSERT(m_offerAnswerCallback);
    validateSDP(sdp);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), sdp = WTFMove(sdp)](auto&) mutable {
        callback(RTCSessionDescriptionInit { RTCSdpType::Offer, sdp });
    });
}

void PeerConnectionBackend::createOfferFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, exception.message());

    ASSERT(m_offerAnswerCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), exception = WTFMove(exception)](auto&) mutable {
        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::createAnswer(RTCAnswerOptions&& options, CreateCallback&& callback)
{
    ASSERT(!m_offerAnswerCallback);
    ASSERT(!m_peerConnection->isClosed());

    m_offerAnswerCallback = WTFMove(callback);
    doCreateAnswer(WTFMove(options));
}

void PeerConnectionBackend::createAnswerSucceeded(String&& sdp)
{
    ASSERT(isMainThread());

#if !RELEASE_LOG_DISABLED
    logger().toObservers(LogWebRTC, WTFLogLevel::Always, LOGIDENTIFIER, "SDP answer created:\n", sdp);
    RELEASE_LOG_FORWARDABLE(WebRTC, PEERCONNECTIONBACKEND_CREATEANSWERSUCCEEDED, logIdentifier(), sdp.utf8());
#endif

    ASSERT(m_offerAnswerCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), sdp = WTFMove(sdp)](auto&) mutable {
        callback(RTCSessionDescriptionInit { RTCSdpType::Answer, sdp });
    });
}

void PeerConnectionBackend::createAnswerFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, exception.message());

    ASSERT(m_offerAnswerCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), exception = WTFMove(exception)](auto&) mutable {
        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::setLocalDescription(const RTCSessionDescription* sessionDescription, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection->isClosed());

    m_isProcessingLocalDescriptionAnswer = sessionDescription && (sessionDescription->type() == RTCSdpType::Answer || sessionDescription->type() == RTCSdpType::Pranswer);
    m_setDescriptionCallback = WTFMove(callback);
    doSetLocalDescription(sessionDescription);
}

struct MediaStreamAndTrackItem {
    Ref<MediaStream> stream;
    Ref<MediaStreamTrack> track;
};

// https://w3c.github.io/webrtc-pc/#set-associated-remote-streams
static void setAssociatedRemoteStreams(RTCRtpReceiver& receiver, const PeerConnectionBackend::TransceiverState& state, Vector<MediaStreamAndTrackItem>& addList, Vector<MediaStreamAndTrackItem>& removeList)
{
    for (auto& currentStream : receiver.associatedStreams()) {
        if (currentStream && std::ranges::none_of(state.receiverStreams, [&currentStream](auto& stream) { return stream->id() == currentStream->id(); }))
            removeList.append({ Ref { *currentStream }, Ref { receiver.track() } });
    }

    for (auto& stream : state.receiverStreams) {
        if (std::ranges::none_of(receiver.associatedStreams(), [&stream](auto& currentStream) { return stream->id() == currentStream->id(); }))
            addList.append({ stream, Ref { receiver.track() } });
    }

    receiver.setAssociatedStreams(WTF::map(state.receiverStreams, [](auto& stream) { return WeakPtr { stream.get() }; }));
}

static bool isDirectionReceiving(RTCRtpTransceiverDirection direction)
{
    return direction == RTCRtpTransceiverDirection::Sendrecv || direction == RTCRtpTransceiverDirection::Recvonly;
}

// https://w3c.github.io/webrtc-pc/#process-remote-tracks
static void processRemoteTracks(RTCRtpTransceiver& transceiver, PeerConnectionBackend::TransceiverState&& state, Vector<MediaStreamAndTrackItem>& addList, Vector<MediaStreamAndTrackItem>& removeList, Vector<Ref<RTCTrackEvent>>& trackEventList, Vector<Ref<MediaStreamTrack>>& muteTrackList)
{
    auto addListSize = addList.size();
    auto& receiver = transceiver.receiver();
    setAssociatedRemoteStreams(receiver, state, addList, removeList);
    if ((state.firedDirection && isDirectionReceiving(*state.firedDirection) && (!transceiver.firedDirection() || !isDirectionReceiving(*transceiver.firedDirection()))) || addListSize != addList.size()) {
        // https://w3c.github.io/webrtc-pc/#process-remote-track-addition
        trackEventList.append(RTCTrackEvent::create(eventNames().trackEvent, Event::CanBubble::No, Event::IsCancelable::No, &receiver, &receiver.track(), WTFMove(state.receiverStreams), &transceiver));
    }
    if (!(state.firedDirection && isDirectionReceiving(*state.firedDirection)) && transceiver.firedDirection() && isDirectionReceiving(*transceiver.firedDirection())) {
        // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
        muteTrackList.append(receiver.track());
    }
    transceiver.setFiredDirection(state.firedDirection);
}

void PeerConnectionBackend::setLocalDescriptionSucceeded(std::optional<DescriptionStates>&& descriptionStates, std::optional<TransceiverStates>&& transceiverStates, std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend, std::optional<double> maxMessageSize)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set local description succeeded");
    if (transceiverStates)
        DEBUG_LOG(LOGIDENTIFIER, "Transceiver states: ", *transceiverStates);
    ASSERT(m_setDescriptionCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), descriptionStates = WTFMove(descriptionStates), transceiverStates = WTFMove(transceiverStates), sctpBackend = WTFMove(sctpBackend), maxMessageSize](auto& peerConnection) mutable {
        if (peerConnection.isClosed())
            return;

        peerConnection.updateTransceiversAfterSuccessfulLocalDescription();
        peerConnection.updateSctpBackend(WTFMove(sctpBackend), maxMessageSize);

        if (descriptionStates) {
            peerConnection.updateDescriptions(WTFMove(*descriptionStates));
            if (peerConnection.isClosed())
                return;
        }

        peerConnection.processIceTransportChanges();
        if (peerConnection.isClosed())
            return;

        if (m_isProcessingLocalDescriptionAnswer && transceiverStates) {
            // Compute track related events.
            Vector<MediaStreamAndTrackItem> removeList;
            Vector<Ref<MediaStreamTrack>> muteTrackList;
            Vector<MediaStreamAndTrackItem> addListNoOp;
            for (auto& transceiverState : *transceiverStates) {
                RefPtr<RTCRtpTransceiver> transceiver;
                for (auto& item : peerConnection.currentTransceivers()) {
                    if (item->mid() == transceiverState.mid) {
                        transceiver = item;
                        break;
                    }
                }
                if (transceiver) {
                    if (!(transceiverState.firedDirection && isDirectionReceiving(*transceiverState.firedDirection)) && transceiver->firedDirection() && isDirectionReceiving(*transceiver->firedDirection())) {
                        setAssociatedRemoteStreams(transceiver->receiver(), transceiverState, addListNoOp, removeList);
                        muteTrackList.append(transceiver->receiver().track());
                    }
                }
                transceiver->setFiredDirection(transceiverState.firedDirection);
            }
            for (auto& track : muteTrackList) {
                track->setShouldFireMuteEventImmediately(true);
                track->source().setMuted(true);
                track->setShouldFireMuteEventImmediately(false);
                if (peerConnection.isClosed())
                    return;
            }

            for (auto& pair : removeList) {
                DEBUG_LOG(LOGIDENTIFIER, "Removing track "_s, pair.track->id(), " from MediaStream "_s, pair.stream->id());
                pair.stream->privateStream().removeTrack(pair.track->privateTrack());
                if (peerConnection.isClosed())
                    return;
            }
        }

        callback({ });
    });
}

void PeerConnectionBackend::setLocalDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set local description failed:", exception.message());

    ASSERT(m_setDescriptionCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_setDescriptionCallback), exception = WTFMove(exception)](auto& peerConnection) mutable {
        if (peerConnection.isClosed())
            return;

        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::setRemoteDescription(const RTCSessionDescription& sessionDescription, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection->isClosed());

    m_setDescriptionCallback = WTFMove(callback);
    doSetRemoteDescription(sessionDescription);
}

void PeerConnectionBackend::setRemoteDescriptionSucceeded(std::optional<DescriptionStates>&& descriptionStates, std::optional<TransceiverStates>&& transceiverStates, std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend, std::optional<double> maxMessageSize)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description succeeded");
    if (transceiverStates)
        DEBUG_LOG(LOGIDENTIFIER, "Transceiver states: ", *transceiverStates);
    ASSERT(m_setDescriptionCallback);

    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), descriptionStates = WTFMove(descriptionStates), transceiverStates = WTFMove(transceiverStates), sctpBackend = WTFMove(sctpBackend), maxMessageSize](auto& peerConnection) mutable {
        UNUSED_PARAM(this);

        if (peerConnection.isClosed())
            return;

        Vector<MediaStreamAndTrackItem> removeList;
        if (transceiverStates) {
            for (auto& transceiver : peerConnection.currentTransceivers()) {
                if (std::ranges::none_of(*transceiverStates, [&transceiver](auto& state) { return state.mid == transceiver->mid(); })) {
                    for (auto& stream : transceiver->receiver().associatedStreams()) {
                        if (stream)
                            removeList.append({ Ref { *stream }, Ref { transceiver->receiver().track() } });
                    }
                }
            }
        }

        peerConnection.updateTransceiversAfterSuccessfulRemoteDescription();
        peerConnection.updateSctpBackend(WTFMove(sctpBackend), maxMessageSize);

        if (descriptionStates) {
            peerConnection.updateDescriptions(WTFMove(*descriptionStates));
            if (peerConnection.isClosed()) {
                DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed after descriptions update");
                return;
            }
        }

        peerConnection.processIceTransportChanges();
        if (peerConnection.isClosed()) {
            DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed after ICE transport changes");
            return;
        }

        if (transceiverStates) {
            // Compute track related events.
            Vector<Ref<MediaStreamTrack>> muteTrackList;
            Vector<MediaStreamAndTrackItem> addList;
            Vector<Ref<RTCTrackEvent>> trackEventList;
            for (auto& transceiverState : *transceiverStates) {
                RefPtr<RTCRtpTransceiver> transceiver;
                for (auto& item : peerConnection.currentTransceivers()) {
                    if (item->mid() == transceiverState.mid) {
                        transceiver = item;
                        break;
                    }
                }
                if (transceiver)
                    processRemoteTracks(*transceiver, WTFMove(transceiverState), addList, removeList, trackEventList, muteTrackList);
            }

            DEBUG_LOG(LOGIDENTIFIER, "Processing ", muteTrackList.size(), " muted tracks");
            for (auto& track : muteTrackList) {
                track->setShouldFireMuteEventImmediately(true);
                track->source().setMuted(true);
                track->setShouldFireMuteEventImmediately(false);
                if (peerConnection.isClosed()) {
                    DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed while processing muted tracks");
                    return;
                }
            }

            DEBUG_LOG(LOGIDENTIFIER, "Removing ", removeList.size(), " tracks");
            for (auto& pair : removeList) {
                pair.stream->privateStream().removeTrack(pair.track->privateTrack());
                if (peerConnection.isClosed()) {
                    DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed while removing tracks");
                    return;
                }
            }

            DEBUG_LOG(LOGIDENTIFIER, "Adding ", addList.size(), " tracks");
            for (auto& pair : addList) {
                pair.stream->addTrackFromPlatform(pair.track.copyRef());
                if (peerConnection.isClosed()) {
                    DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed while adding tracks");
                    return;
                }
            }

            DEBUG_LOG(LOGIDENTIFIER, "Dispatching ", trackEventList.size(), " track events");
            for (auto& event : trackEventList) {
                RefPtr track = event->track();
                ALWAYS_LOG(LOGIDENTIFIER, "Dispatching track event for track ", track->id());
                peerConnection.dispatchEvent(event);
                if (peerConnection.isClosed()) {
                    DEBUG_LOG(LOGIDENTIFIER, "PeerConnection closed while dispatching track events");
                    return;
                }

                track->source().setMuted(false);
            }
        }

        callback({ });
    });
}

void PeerConnectionBackend::setRemoteDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description failed:", exception.message());

    ASSERT(m_setDescriptionCallback);
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(m_setDescriptionCallback), exception = WTFMove(exception)](auto& peerConnection) mutable {
        if (peerConnection.isClosed())
            return;

        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::iceGatheringStateChanged(RTCIceGatheringState state)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [this, state](auto& peerConnection) {
        if (state == RTCIceGatheringState::Complete) {
            doneGatheringCandidates();
            return;
        }
        peerConnection.updateIceGatheringState(state);
    });
}

Ref<RTCPeerConnection> PeerConnectionBackend::protectedPeerConnection() const
{
    return m_peerConnection.get();
}

static String extractIPAddress(StringView sdp)
{
    unsigned counter = 0;
    for (auto item : StringView { sdp }.split(' ')) {
        if (++counter == 5)
            return item.toString();
    }
    return { };
}

static inline bool shouldIgnoreIceCandidate(const RTCIceCandidate& iceCandidate)
{
    auto address = extractIPAddress(iceCandidate.candidate());
    if (!address.endsWithIgnoringASCIICase(".local"_s))
        return false;

    if (!WTF::isVersion4UUID(StringView { address }.left(address.length() - 6))) {
        RELEASE_LOG_ERROR(WebRTC, "mDNS candidate is not a Version 4 UUID");
        return true;
    }
    return false;
}

void PeerConnectionBackend::addIceCandidate(RTCIceCandidate* iceCandidate, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection->isClosed());

    if (!iceCandidate) {
        callback({ });
        return;
    }

    if (shouldIgnoreIceCandidate(*iceCandidate)) {
        callback({ });
        return;
    }

    doAddIceCandidate(*iceCandidate, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)]<typename Result> (Result&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        ActiveDOMObject::queueTaskKeepingObjectAlive(protectedThis->protectedPeerConnection().get(), TaskSource::Networking, [callback = WTFMove(callback), result = std::forward<Result>(result)](auto& peerConnection) mutable {
            if (peerConnection.isClosed())
                return;

            if (result.hasException()) {
                RELEASE_LOG_ERROR(WebRTC, "Adding ice candidate failed %hhu", enumToUnderlyingType(result.exception().code()));
                callback(result.releaseException());
                return;
            }

            if (auto descriptions = result.releaseReturnValue())
                peerConnection.updateDescriptions(WTFMove(*descriptions));
            callback({ });
        });
    });
}

void PeerConnectionBackend::enableICECandidateFiltering()
{
    m_shouldFilterICECandidates = true;
}

void PeerConnectionBackend::disableICECandidateFiltering()
{
    m_shouldFilterICECandidates = false;
}

void PeerConnectionBackend::validateSDP(const String& sdp) const
{
#if ASSERT_ENABLED
    if (!m_shouldFilterICECandidates)
        return;
    sdp.split('\n', [](auto line) {
        ASSERT(!line.startsWith("a=candidate"_s) || line.contains(".local"_s));
    });
#else
    UNUSED_PARAM(sdp);
#endif
}

void PeerConnectionBackend::newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex, String&& serverURL, std::optional<DescriptionStates>&& descriptions)
{
    ActiveDOMObject::queueTaskKeepingObjectAlive(protectedPeerConnection().get(), TaskSource::Networking, [logSiteIdentifier = LOGIDENTIFIER, this, sdp = WTFMove(sdp), mid = WTFMove(mid), sdpMLineIndex, serverURL = WTFMove(serverURL), descriptions = WTFMove(descriptions)](auto& peerConnection) mutable {
        if (peerConnection.isClosed())
            return;

        if (descriptions)
            peerConnection.updateDescriptions(WTFMove(*descriptions));

        if (peerConnection.isClosed())
            return;

        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "Gathered ice candidate:", sdp);
        m_finishedGatheringCandidates = false;

        ASSERT(!m_shouldFilterICECandidates || sdp.contains(".local"_s) || sdp.contains(" srflx "_s) || sdp.contains(" relay "_s));
        auto candidate = RTCIceCandidate::create(WTFMove(sdp), WTFMove(mid), sdpMLineIndex);
        ALWAYS_LOG(logSiteIdentifier, "Dispatching ICE event for SDP ", candidate->candidate());
        peerConnection.dispatchEvent(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, WTFMove(candidate), WTFMove(serverURL)));
    });
}

void PeerConnectionBackend::newDataChannel(UniqueRef<RTCDataChannelHandler>&& channelHandler, String&& label, RTCDataChannelInit&& channelInit)
{
    protectedPeerConnection()->dispatchDataChannelEvent(WTFMove(channelHandler), WTFMove(label), WTFMove(channelInit));
}

void PeerConnectionBackend::doneGatheringCandidates()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Finished ice candidate gathering");
    m_finishedGatheringCandidates = true;

    Ref peerConnection = m_peerConnection.get();
    peerConnection->scheduleEvent(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, nullptr, { }));
    peerConnection->updateIceGatheringState(RTCIceGatheringState::Complete);
}

void PeerConnectionBackend::stop()
{
    m_offerAnswerCallback = nullptr;
    m_setDescriptionCallback = nullptr;

    doStop();
}

void PeerConnectionBackend::markAsNeedingNegotiation(uint32_t eventId)
{
    protectedPeerConnection()->updateNegotiationNeededFlag(eventId);
}

ExceptionOr<Ref<RTCRtpSender>> PeerConnectionBackend::addTrack(MediaStreamTrack&, FixedVector<String>&&)
{
    return Exception { ExceptionCode::NotSupportedError, "Not implemented"_s };
}

ExceptionOr<Ref<RTCRtpTransceiver>> PeerConnectionBackend::addTransceiver(const String&, const RTCRtpTransceiverInit&, IgnoreNegotiationNeededFlag)
{
    return Exception { ExceptionCode::NotSupportedError, "Not implemented"_s };
}

ExceptionOr<Ref<RTCRtpTransceiver>> PeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&)
{
    return Exception { ExceptionCode::NotSupportedError, "Not implemented"_s };
}

void PeerConnectionBackend::generateCertificate(Document& document, const CertificateInformation& info, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
{
#if USE(LIBWEBRTC)
    auto* page = document.page();
    if (!page) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    auto& webRTCProvider = static_cast<LibWebRTCProvider&>(page->webRTCProvider());
    LibWebRTCCertificateGenerator::generateCertificate(document.securityOrigin(), webRTCProvider, info, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
#elif USE(GSTREAMER_WEBRTC)
    auto certificate = ::WebCore::generateCertificate(document.securityOrigin(), info);
    if (certificate.has_value())
        promise.resolve(*certificate);
    else
        promise.reject(ExceptionCode::NotSupportedError);
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(info);
    promise.reject(ExceptionCode::NotSupportedError);
#endif
}

ScriptExecutionContext* PeerConnectionBackend::context() const
{
    return protectedPeerConnection()->scriptExecutionContext();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PeerConnectionBackend::logChannel() const
{
    return LogWebRTC;
}
#endif

static Ref<JSON::Object> toJSONObject(const PeerConnectionBackend::TransceiverState& transceiverState)
{
    auto object = JSON::Object::create();
    object->setString("mid"_s, transceiverState.mid);

    auto receiverStreams = JSON::Array::create();
    for (auto receiverStream : transceiverState.receiverStreams)
        receiverStreams->pushString(receiverStream->id());
    object->setArray("receiverStreams"_s, WTFMove(receiverStreams));

    if (auto firedDirection = transceiverState.firedDirection)
        object->setString("firedDirection"_s, convertEnumerationToString(*firedDirection));

    return object;
}

static Ref<JSON::Array> toJSONArray(const PeerConnectionBackend::TransceiverStates& transceiverStates)
{
    auto array = JSON::Array::create();
    for (auto transceiverState : transceiverStates)
        array->pushObject(toJSONObject(transceiverState));

    return array;
}

static String toJSONString(const PeerConnectionBackend::TransceiverState& transceiverState)
{
    return toJSONObject(transceiverState)->toJSONString();
}

static String toJSONString(const PeerConnectionBackend::TransceiverStates& transceiverStates)
{
    return toJSONArray(transceiverStates)->toJSONString();
}

void PeerConnectionBackend::ref() const
{
    m_peerConnection->ref();
}

void PeerConnectionBackend::deref() const
{
    m_peerConnection->deref();
}

String PeerConnectionBackend::generateJSONLogEvent(LogEvent&& logEvent, bool isForGatherLogs)
{
    ASCIILiteral type;
    String event;
    WTF::switchOn(WTFMove(logEvent), [&](MessageLogEvent&& logEvent) {
        type = "event"_s;
        StringBuilder builder;
        auto strippedMessage = logEvent.message.removeCharacters([](auto character) {
            return character == '\n';
        });
        builder.append("{\"message\":\""_s, strippedMessage, "\",\"payload\":\""_s);
        if (logEvent.payload)
            builder.append(WTF::base64EncodeToString(*logEvent.payload));
        builder.append("\"}"_s);
        event = builder.toString();
    }, [&](StatsLogEvent&& logEvent) {
        type = "stats"_s;
        event = WTFMove(logEvent);
    });

    if (isForGatherLogs) {
        UNUSED_VARIABLE(type);
        return event;
    }

    auto timestamp = WTF::WallTime::now().secondsSinceEpoch().microseconds();
    return makeString("{\"peer-connection\":\""_s, m_logIdentifierString, "\",\"timestamp\":"_s, timestamp, ",\"type\":\""_s, type, "\",\"event\":"_s, event, '}');
}

void PeerConnectionBackend::emitJSONLogEvent(String&& event)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    if (!isJSONLogStreamingEnabled())
        return;

    auto& handler = jsonFileHandler();
    handler.log(WTFMove(event));
#else
    UNUSED_PARAM(event);
#endif
}

} // namespace WebCore

namespace WTF {

String LogArgument<WebCore::PeerConnectionBackend::TransceiverState>::toString(const WebCore::PeerConnectionBackend::TransceiverState& transceiverState)
{
    return toJSONString(transceiverState);
}

String LogArgument<WebCore::PeerConnectionBackend::TransceiverStates>::toString(const WebCore::PeerConnectionBackend::TransceiverStates& transceiverStates)
{
    return toJSONString(transceiverStates);
}

}

#endif // ENABLE(WEB_RTC)
