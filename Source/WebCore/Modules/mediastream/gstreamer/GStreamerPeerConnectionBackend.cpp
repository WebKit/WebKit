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

#include "Document.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaEndpoint.h"
#include "GStreamerRtpReceiverBackend.h"
#include "GStreamerRtpSenderBackend.h"
#include "GStreamerRtpTransceiverBackend.h"
#include "IceCandidate.h"
#include "JSRTCStatsReport.h"
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
        return g_str_has_prefix(channel.name, "WebRTC");
    }
};

WebRTCLogObserver& webrtcLogObserverSingleton()
{
    static NeverDestroyed<WebRTCLogObserver> sharedInstance;
    return sharedInstance;
}
#endif // GST_DISABLE_GST_DEBUG

static std::unique_ptr<PeerConnectionBackend> createGStreamerPeerConnectionBackend(RTCPeerConnection& peerConnection)
{
    ensureGStreamerInitialized();
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_pc_backend_debug, "webkitwebrtcpeerconnection", 0, "WebKit WebRTC PeerConnection");
    });
    if (!isGStreamerPluginAvailable("webrtc")) {
        WTFLogAlways("GstWebRTC plugin not found. Make sure to install gst-plugins-bad >= 1.20 with the webrtc plugin enabled.");
        return nullptr;
    }
    return WTF::makeUniqueWithoutRefCountedCheck<GStreamerPeerConnectionBackend>(peerConnection);
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

    auto identifier = makeString(logIdentifier());
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

RefPtr<RTCRtpSender> GStreamerPeerConnectionBackend::findExistingSender(const Vector<RefPtr<RTCRtpTransceiver>>& transceivers, GStreamerRtpSenderBackend& senderBackend)
{
    ASSERT(senderBackend.rtcSender());
    for (auto& transceiver : transceivers) {
        auto& sender = transceiver->sender();
        if (!sender.isStopped() && senderBackend.rtcSender() == backendFromRTPSender(sender).rtcSender())
            return Ref(sender);
    }
    return nullptr;
}

ExceptionOr<Ref<RTCRtpSender>> GStreamerPeerConnectionBackend::addTrack(MediaStreamTrack& track, FixedVector<String>&& mediaStreamIds)
{
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Adding new track.");
    auto senderBackend = WTF::makeUnique<GStreamerRtpSenderBackend>(*this, nullptr);
    if (!m_endpoint->addTrack(*senderBackend, track, mediaStreamIds))
        return Exception { ExceptionCode::TypeError, "Unable to add track"_s };

    Ref peerConnection = m_peerConnection.get();
    if (auto sender = findExistingSender(peerConnection->currentTransceivers(), *senderBackend)) {
        GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Existing sender found, associating track to it.");
        backendFromRTPSender(*sender).takeSource(*senderBackend);
        sender->setTrack(track);
        sender->setMediaStreamIds(mediaStreamIds);
        return sender.releaseNonNull();
    }

    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "Creating new transceiver.");
    auto transceiverBackend = m_endpoint->transceiverBackendFromSender(*senderBackend);

    auto sender = RTCRtpSender::create(peerConnection, track, WTFMove(senderBackend));
    sender->setMediaStreamIds(mediaStreamIds);
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), track.kind(), track.id());
    auto transceiver = RTCRtpTransceiver::create(sender.copyRef(), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(WTFMove(transceiver));
    return sender;
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

GStreamerRtpSenderBackend::Source GStreamerPeerConnectionBackend::createLinkedSourceForTrack(MediaStreamTrack& track)
{
    return m_endpoint->createLinkedSourceForTrack(track);
}

static inline GStreamerRtpTransceiverBackend& backendFromRTPTransceiver(RTCRtpTransceiver& transceiver)
{
    return static_cast<GStreamerRtpTransceiverBackend&>(*transceiver.backend());
}

RTCRtpTransceiver* GStreamerPeerConnectionBackend::existingTransceiver(WTF::Function<bool(GStreamerRtpTransceiverBackend&)>&& matchingFunction)
{
    for (auto& transceiver : protectedPeerConnection()->currentTransceivers()) {
        if (matchingFunction(backendFromRTPTransceiver(*transceiver)))
            return transceiver.get();
    }
    return nullptr;
}

RTCRtpTransceiver& GStreamerPeerConnectionBackend::newRemoteTransceiver(std::unique_ptr<GStreamerRtpTransceiverBackend>&& transceiverBackend, RealtimeMediaSource::Type type, String&& receiverTrackId)
{
    auto trackKind = type == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s;
    Ref peerConnection = m_peerConnection.get();
    auto sender = RTCRtpSender::create(peerConnection, trackKind, transceiverBackend->createSenderBackend(*this, nullptr, nullptr));
    auto trackId = receiverTrackId.isEmpty() ? sender->trackId() : WTFMove(receiverTrackId);
    GST_DEBUG_OBJECT(m_endpoint->pipeline(), "New remote transceiver with receiver track ID: %s", trackId.utf8().data());
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), trackKind, trackId);
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(transceiverBackend));
    peerConnection->addInternalTransceiver(transceiver.copyRef());
    return transceiver.get();
}

void GStreamerPeerConnectionBackend::collectTransceivers()
{
    m_endpoint->collectTransceivers();
}

void GStreamerPeerConnectionBackend::removeTrack(RTCRtpSender& sender)
{
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
        auto& track = transceiver->receiver().track();
        auto& source = track.privateTrack().source();
        if (source.isIncomingAudioSource()) {
            auto& audioSource = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source);
            audioSource.tearDown();
        } else if (source.isIncomingVideoSource()) {
            auto& videoSource = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source);
            videoSource.tearDown();
        }

        if (auto senderBackend = transceiver->sender().backend())
            static_cast<GStreamerRtpSenderBackend*>(senderBackend)->tearDown();

        auto& backend = backendFromRTPTransceiver(*transceiver);
        backend.tearDown();
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
