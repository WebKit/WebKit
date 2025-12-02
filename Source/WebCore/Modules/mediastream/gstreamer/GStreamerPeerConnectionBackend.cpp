/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GStreamerPeerConnectionBackend.h"

#if USE(GSTREAMER_WEBRTC)

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaEndpoint.h"
#include "GStreamerRtpReceiverBackend.h"
#include "GStreamerRtpSenderBackend.h"
#include "GStreamerRtpTransceiverBackend.h"
#include "IceCandidate.h"
#include "JSRTCStatsReport.h"
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "NotImplemented.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCRtpCapabilities.h"
#include "RTCRtpReceiver.h"
#include "RTCSessionDescription.h"
#include "RealtimeIncomingAudioSourceGStreamer.h"
#include "RealtimeIncomingVideoSourceGStreamer.h"
#include "RealtimeOutgoingAudioSourceGStreamer.h"
#include "RealtimeOutgoingVideoSourceGStreamer.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_pc_backend_debug);
#define GST_CAT_DEFAULT webkit_webrtc_pc_backend_debug

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebRTCLogObserver);

#ifndef GST_DISABLE_GST_DEBUG
class WebRTCLogObserver : public WebCoreLogObserver {
public:
    GstDebugCategory* debugCategory() const final
    {
        return webkit_webrtc_pc_backend_debug;
    }
    bool shouldEmitLogMessage(const WTFLogChannel& channel) const final
    {
        return StringView::fromLatin1(channel.name).startsWith("WebRTC"_s);
    }
};

WebRTCLogObserver& webrtcLogObserverSingleton()
{
    static NeverDestroyed<WebRTCLogObserver> sharedInstance;
    return sharedInstance;
}
#endif // GST_DISABLE_GST_DEBUG

static const std::unique_ptr<PeerConnectionBackend> createGStreamerPeerConnectionBackend(RTCPeerConnection& peerConnection, MediaEndpointConfiguration&& configuration)
{
    ensureGStreamerInitialized();
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_pc_backend_debug, "webkitwebrtcpeerconnection", 0, "WebKit WebRTC PeerConnection");
    });
    if (!isGStreamerPluginAvailable("webrtc"_s)) {
        WTFLogAlways("GstWebRTC plugin not found. Make sure to install gst-plugins-bad >= 1.20 with the webrtc plugin enabled.");
        return nullptr;
    }
    auto backend = WTF::makeUniqueWithoutRefCountedCheck<GStreamerPeerConnectionBackend, PeerConnectionBackend>(peerConnection);
    bool status = backend->setConfiguration(WTFMove(configuration));
    ASSERT_UNUSED(status, status);
    return backend;
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createGStreamerPeerConnectionBackend;

GStreamerPeerConnectionBackend::GStreamerPeerConnectionBackend(RTCPeerConnection& peerConnection)
    : PeerConnectionBackend(peerConnection)
    , m_endpoint(GStreamerMediaEndpoint::create(*this))
{
    disableICECandidateFiltering();

#if !RELEASE_LOG_DISABLED && !defined(GST_DISABLE_GST_DEBUG)
    // PeerConnectionBackend relies on the Document logger, so to prevent duplicate messages in case
    // more than one PeerConnection is created, we register a single observer.
    auto& logObserver = webrtcLogObserverSingleton();
    logObserver.addWatch(logger());

    auto identifier = makeString(LOGIDENTIFIER.objectIdentifier);
    GST_INFO_OBJECT(m_endpoint->pipeline(), "WebCore logs identifier for this pipeline is: %s", identifier.convertToASCIIUppercase().ascii().data());
#endif
}

GStreamerPeerConnectionBackend::~GStreamerPeerConnectionBackend()
{
#if !RELEASE_LOG_DISABLED && !defined(GST_DISABLE_GST_DEBUG)
    auto& logObserver = webrtcLogObserverSingleton();
    logObserver.removeWatch(logger());
#endif
}

void GStreamerPeerConnectionBackend::suspend()
{
    m_endpoint->suspend();
}

void GStreamerPeerConnectionBackend::resume()
{
    m_endpoint->resume();
}

void GStreamerPeerConnectionBackend::restartIce()
{
    m_endpoint->restartIce();
}

bool GStreamerPeerConnectionBackend::setConfiguration(MediaEndpointConfiguration&& configuration)
{
    return m_endpoint->setConfiguration(configuration);
}

GStreamerRtpSenderBackend& GStreamerPeerConnectionBackend::backendFromRTPSender(RTCRtpSender& sender)
{
    ASSERT(!sender.isStopped());
    return static_cast<GStreamerRtpSenderBackend&>(*sender.backend());
}

void GStreamerPeerConnectionBackend::dispatchSenderBitrateRequest(const GRefPtr<GstWebRTCDTLSTransport>& transport, uint32_t bitrate)
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        auto& senderBackend = backendFromRTPSender(transceiver->sender());
        GRefPtr<GstWebRTCDTLSTransport> candidate;
        g_object_get(senderBackend.rtcSender(), "transport", &candidate.outPtr(), nullptr);
        if (!candidate)
            continue;

        if (candidate == transport) {
            senderBackend.dispatchBitrateRequest(bitrate);
            return;
        }
    }
}

void GStreamerPeerConnectionBackend::getStats(Ref<DeferredPromise>&& promise)
{
    m_endpoint->getStats(nullptr, WTFMove(promise));
}

void GStreamerPeerConnectionBackend::getStats(RTCRtpSender& sender, Ref<DeferredPromise>&& promise)
{
    if (!sender.backend()) {
        m_endpoint->getStats(nullptr, WTFMove(promise));
        return;
    }

    auto& backend = backendFromRTPSender(sender);
    GRefPtr<GstPad> pad;
    if (RealtimeOutgoingAudioSourceGStreamer* source = backend.audioSource())
        pad = source->pad();
    else if (RealtimeOutgoingVideoSourceGStreamer* source = backend.videoSource())
        pad = source->pad();

    m_endpoint->getStats(pad.get(), WTFMove(promise));
}

void GStreamerPeerConnectionBackend::getStats(RTCRtpReceiver& receiver, Ref<DeferredPromise>&& promise)
{
    m_endpoint->getStats(receiver, WTFMove(promise));
}

void GStreamerPeerConnectionBackend::doSetLocalDescription(const RTCSessionDescription* description)
{
    m_endpoint->doSetLocalDescription(description);
    m_isLocalDescriptionSet = true;
}

void GStreamerPeerConnectionBackend::doSetRemoteDescription(const RTCSessionDescription& description)
{
    m_endpoint->doSetRemoteDescription(description);
    m_isRemoteDescriptionSet = true;
}

void GStreamerPeerConnectionBackend::doCreateOffer(RTCOfferOptions&& options)
{
    m_endpoint->doCreateOffer(options);
}

void GStreamerPeerConnectionBackend::doCreateAnswer(RTCAnswerOptions&&)
{
    if (!m_isRemoteDescriptionSet) {
        createAnswerFailed(Exception { ExceptionCode::InvalidStateError, "No remote description set"_s });
        return;
    }
    m_endpoint->doCreateAnswer();
}

void GStreamerPeerConnectionBackend::prepareForClose()
{
    m_endpoint->prepareForClose();
}

void GStreamerPeerConnectionBackend::close()
{
    m_endpoint->close();
}

void GStreamerPeerConnectionBackend::doStop()
{
    m_endpoint->stop();
}

void GStreamerPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate, AddIceCandidateCallback&& callback)
{
    unsigned sdpMLineIndex = candidate.sdpMLineIndex() ? candidate.sdpMLineIndex().value() : 0;
    auto rtcCandidate = WTF::makeUnique<GStreamerIceCandidate>(*new GStreamerIceCandidate { sdpMLineIndex, candidate.candidate() });
    m_endpoint->addIceCandidate(*rtcCandidate, WTFMove(callback));
}

Ref<RTCRtpReceiver> GStreamerPeerConnectionBackend::createReceiver(std::unique_ptr<GStreamerRtpReceiverBackend>&& backend, const String& trackKind, const String& trackId)
{
    auto& document = downcast<Document>(*protectedPeerConnection()->scriptExecutionContext());

    auto source = backend->createSource(trackKind, trackId);
    // Remote source is initially muted and will be unmuted when receiving the first packet.
    source->setMuted(true);
    auto trackID = source->persistentID();
    auto remoteTrackPrivate = MediaStreamTrackPrivate::create(document.logger(), WTFMove(source), WTFMove(trackID));
    auto remoteTrack = MediaStreamTrack::create(document, WTFMove(remoteTrackPrivate));

    return RTCRtpReceiver::create(*this, WTFMove(remoteTrack), WTFMove(backend));
}

std::unique_ptr<RTCDataChannelHandler> GStreamerPeerConnectionBackend::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return m_endpoint->createDataChannel(label, options);
}

ExceptionOr<Ref<RTCRtpSender>> GStreamerPeerConnectionBackend::addTrack(MediaStreamTrack& track, FixedVector<String>&& mediaStreamIds)
{
    // https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-addtrack
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Adding new track.");

    // 6. Let senders be the result of executing the CollectSenders algorithm.
    // This is already done in RTCPeerConnection so no need to repeat:
    // If an RTCRtpSender for track already exists in senders, throw an InvalidAccessError.
    Vector<RefPtr<RTCRtpSender>> senders;
    for (const auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (transceiver->stopped())
            continue;
        senders.append(&transceiver->sender());
    }

    // 7. The steps below describe how to determine if an existing sender can be reused. If any
    // RTCRtpSender object in senders matches all the following criteria, let sender be that object,
    // or null otherwise:
    RefPtr<RTCRtpSender> sender;
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Looking for a re-usable sender in %zu existing senders", senders.size());
    for (const auto& currentSender : senders) {
        bool noTrack = false;
        bool trackKindMatches = false;
        bool isNotStopped = false;
        bool isNotActivelySending = false;

        // The sender's track is null.
        if (!currentSender->track()) {
            GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Sender %p has no track, potentially reusing", currentSender.get());
            noTrack = true;
        }

        // The transceiver kind of the RTCRtpTransceiver, associated with the sender, matches kind.
        if (currentSender->trackKind() == track.kind()) {
            GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Sender %p kind matches, potentially reusing", currentSender.get());
            trackKindMatches = true;
        }

        // The [[Stopping]] slot of the RTCRtpTransceiver associated with the sender is false.
        if (!currentSender->isStopped()) {
            GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Sender %p is not stopped, potentially reusing", currentSender.get());
            isNotStopped = true;
        }

        // The sender has never been used to send. More precisely, the [[CurrentDirection]] slot of
        // the RTCRtpTransceiver associated with the sender has never had a value of "sendrecv" or
        // "sendonly".
        auto direction = currentSender->currentTransceiverDirection();
        if (direction != RTCRtpTransceiverDirection::Sendonly && direction != RTCRtpTransceiverDirection::Sendrecv) {
            GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Sender %p is not actively sending, potentially reusing", currentSender.get());
            isNotActivelySending = true;
        }

        if (noTrack && trackKindMatches && isNotStopped && isNotActivelySending) {
            sender = currentSender;
            break;
        }
    }

    // 8. If sender is not null, run the following steps to use that sender:
    if (sender) {
        GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Re-using sender %p", sender.get());

        // 1. Set sender.[[SenderTrack]] to track.
        sender->setTrack(track);

        // 2. Set sender.[[AssociatedMediaStreamIds]] to an empty set.
        // 3. For each stream in streams, add stream.id to [[AssociatedMediaStreamIds]] if it's not already there.
        sender->setMediaStreamIds(mediaStreamIds);

        // 4. Let transceiver be the RTCRtpTransceiver associated with sender.
        RefPtr<RTCRtpTransceiver> transceiver;
        for (const auto& currentTransceiver : protectedPeerConnection()->currentTransceivers()) {
            if (&currentTransceiver->sender() == sender.get()) {
                transceiver = currentTransceiver;
                break;
            }
        }
        if (!transceiver)
            return Exception { ExceptionCode::TypeError, "Unable to add track"_s };

        m_endpoint->recycleTransceiverForSenderTrack(reinterpret_cast<GStreamerRtpTransceiverBackend*>(transceiver->backend()), track, mediaStreamIds);

        // 5. If transceiver.[[Direction]] is "recvonly", set transceiver.[[Direction]] to "sendrecv".
        // 6. If transceiver.[[Direction]] is "inactive", set transceiver.[[Direction]] to "sendonly".
        auto direction = transceiver->direction();
        if (direction == RTCRtpTransceiverDirection::Recvonly)
            transceiver->setDirection(RTCRtpTransceiverDirection::Sendrecv);
        else if (direction == RTCRtpTransceiverDirection::Inactive)
            transceiver->setDirection(RTCRtpTransceiverDirection::Sendonly);

        // 11. Update the negotiation-needed flag for connection.
        m_endpoint->onNegotiationNeeded();

        // 12. Return sender.
        return sender.releaseNonNull();
    }

    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Creating new transceiver.");
    auto addTrackResult = m_endpoint->addTrack(track, mediaStreamIds);
    if (addTrackResult.hasException())
        return addTrackResult.releaseException();

    auto senderBackend = addTrackResult.releaseReturnValue();

    auto transceiverBackend = m_endpoint->transceiverBackendFromSender(*senderBackend);

    Ref peerConnection = m_peerConnection.get();
    auto newSender = RTCRtpSender::create(peerConnection, track, WTFMove(senderBackend));
    newSender->setMediaStreamIds(mediaStreamIds);
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), track.kind(), track.id());
    auto transceiver = RTCRtpTransceiver::create(newSender.copyRef(), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(WTFMove(transceiver));
    return newSender;
}

template<typename T>
ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiverFromTrackOrKind(T&& trackOrKind, const RTCRtpTransceiverInit& init, IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Adding new transceiver.");
    auto result = m_endpoint->addTransceiver(trackOrKind, init, ignoreNegotiationNeededFlag);
    if (result.hasException())
        return result.releaseException();

    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Creating new transceiver.");
    auto backends = result.releaseReturnValue();
    Ref peerConnection = m_peerConnection.get();
    auto sender = RTCRtpSender::create(peerConnection, WTFMove(trackOrKind), WTFMove(backends.senderBackend));
    auto receiver = createReceiver(WTFMove(backends.receiverBackend), sender->trackKind(), sender->trackId());
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(backends.transceiverBackend));
    peerConnection->addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init, IgnoreNegotiationNeededFlag ignoreNegotiationNeededFlag)
{
    return addTransceiverFromTrackOrKind(String { trackKind }, init, ignoreNegotiationNeededFlag);
}

ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&& track, const RTCRtpTransceiverInit& init)
{
    return addTransceiverFromTrackOrKind(WTFMove(track), init, IgnoreNegotiationNeededFlag::No);
}

GStreamerRtpSenderBackend::Source GStreamerPeerConnectionBackend::createSourceForTrack(MediaStreamTrack& track)
{
    return m_endpoint->createSourceForTrack(track);
}

static inline GStreamerRtpTransceiverBackend& backendFromRTPTransceiver(RTCRtpTransceiver& transceiver)
{
    return static_cast<GStreamerRtpTransceiverBackend&>(*transceiver.backend());
}

RefPtr<RTCRtpTransceiver> GStreamerPeerConnectionBackend::existingTransceiver(WTF::Function<bool(GStreamerRtpTransceiverBackend&)>&& matchingFunction)
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (matchingFunction(backendFromRTPTransceiver(*transceiver)))
            return transceiver;
    }
    return nullptr;
}

Ref<RTCRtpTransceiver> GStreamerPeerConnectionBackend::newRemoteTransceiver(std::unique_ptr<GStreamerRtpTransceiverBackend>&& transceiverBackend, RealtimeMediaSource::Type type, String&& receiverTrackId)
{
    auto trackKind = type == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s;
    Ref peerConnection = m_peerConnection.get();
    auto sender = RTCRtpSender::create(peerConnection, trackKind, transceiverBackend->createSenderBackend(*this, nullptr, nullptr));
    auto trackId = receiverTrackId.isEmpty() ? sender->trackId() : WTFMove(receiverTrackId);
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "New remote transceiver with receiver track ID: %s", trackId.utf8().data());
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), trackKind, trackId);
    Ref transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

void GStreamerPeerConnectionBackend::collectTransceivers()
{
    m_endpoint->collectTransceivers();
}

void GStreamerPeerConnectionBackend::removeTrack(RTCRtpSender& sender)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Removing "_s, sender.trackKind(), " track with ID "_s, sender.trackId());
    m_endpoint->removeTrack(backendFromRTPSender(sender));
}

void GStreamerPeerConnectionBackend::applyRotationForOutgoingVideoSources()
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (!transceiver->sender().isStopped()) {
            if (auto* videoSource = backendFromRTPSender(transceiver->sender()).videoSource())
                videoSource->setApplyRotation(true);
        }
    }
}

void GStreamerPeerConnectionBackend::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    m_endpoint->gatherDecoderImplementationName(WTFMove(callback));
}

bool GStreamerPeerConnectionBackend::isNegotiationNeeded(uint32_t eventId) const
{
    return m_endpoint->isNegotiationNeeded(eventId);
}

std::optional<bool> GStreamerPeerConnectionBackend::canTrickleIceCandidates() const
{
    return m_endpoint->canTrickleIceCandidates();
}

RTCPeerConnection& GStreamerPeerConnectionBackend::connection()
{
    return m_peerConnection.get();
}

void GStreamerPeerConnectionBackend::tearDown()
{
    for (auto& transceiver : connection().currentTransceivers()) {
        auto& sender = transceiver->sender();
        sender.setTransport(nullptr);

        if (auto senderBackend = sender.backend())
            static_cast<GStreamerRtpSenderBackend*>(senderBackend)->tearDown();

        auto& receiver = transceiver->receiver();
        receiver.setTransport(nullptr);

        auto& incomingSource = static_cast<RealtimeIncomingSourceGStreamer&>(receiver.track().privateTrack().source());
        incomingSource.tearDown();

        if (auto receiverBackend = receiver.backend())
            static_cast<GStreamerRtpReceiverBackend*>(receiverBackend)->tearDown();

        auto& backend = backendFromRTPTransceiver(*transceiver);
        backend.tearDown();
    }
    connection().clearTransports();
}

void GStreamerPeerConnectionBackend::startGatheringStatLogs(Function<void(String&&)>&& callback)
{
    if (!m_rtcStatsLogCallback)
        m_endpoint->startRTCLogs();
    m_rtcStatsLogCallback = WTFMove(callback);
}

void GStreamerPeerConnectionBackend::stopGatheringStatLogs()
{
    if (m_rtcStatsLogCallback) {
        m_endpoint->stopRTCLogs();
        m_rtcStatsLogCallback = { };
    }
}

void GStreamerPeerConnectionBackend::provideStatLogs(String&& stats)
{
    if (m_rtcStatsLogCallback)
        m_rtcStatsLogCallback(WTFMove(stats));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
