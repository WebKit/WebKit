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

namespace WebCore {

static std::unique_ptr<PeerConnectionBackend> createGStreamerPeerConnectionBackend(RTCPeerConnection& peerConnection)
{
    ensureGStreamerInitialized();
    if (!isGStreamerPluginAvailable("webrtc")) {
        WTFLogAlways("GstWebRTC plugin not found. Make sure to install gst-plugins-bad >= 1.20 with the webrtc plugin enabled.");
        return nullptr;
    }
    return WTF::makeUnique<GStreamerPeerConnectionBackend>(peerConnection);
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createGStreamerPeerConnectionBackend;

static RTCRtpCapabilities gstreamerRtpCapatiblities(const String& kind)
{
    RTCRtpCapabilities capabilities;
    capabilities.headerExtensions.append({ "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"_s });
    if (kind == "audio"_s) {
        capabilities.codecs.reserveCapacity(4);
        capabilities.codecs.uncheckedAppend({ .mimeType = "audio/OPUS"_s,
            .clockRate = 48000,
            .channels = 2,
            .sdpFmtpLine = emptyString() });
        capabilities.codecs.uncheckedAppend({ .mimeType = "audio/G722"_s,
            .clockRate = 8000,
            .channels = 1,
            .sdpFmtpLine = emptyString() });
        capabilities.codecs.uncheckedAppend({ .mimeType = "audio/PCMA"_s,
            .clockRate = 8000,
            .channels = 1,
            .sdpFmtpLine = emptyString() });
        capabilities.codecs.uncheckedAppend({ .mimeType = "audio/PCMU"_s,
            .clockRate = 8000,
            .channels = 1,
            .sdpFmtpLine = emptyString() });
    } else {
        capabilities.headerExtensions.append({ "urn:3gpp:video-orientation"_s });
        capabilities.codecs.reserveCapacity(4);
        capabilities.codecs.uncheckedAppend({ .mimeType = "video/VP8"_s,
            .clockRate = 90000,
            .channels = std::nullopt,
            .sdpFmtpLine = emptyString() });
        capabilities.codecs.uncheckedAppend({ .mimeType = "video/VP9"_s,
            .clockRate = 90000,
            .channels = std::nullopt,
            .sdpFmtpLine = "profile-id=0"_s });
        capabilities.codecs.uncheckedAppend({ .mimeType = "video/VP9"_s,
            .clockRate = 90000,
            .channels = std::nullopt,
            .sdpFmtpLine = "profile-id=1"_s });
        capabilities.codecs.uncheckedAppend({ .mimeType = "video/H264"_s,
            .clockRate = 90000,
            .channels = std::nullopt,
            .sdpFmtpLine = "packetization-mode=1;profile-level-id=42e01f"_s });
    }
    return capabilities;
}

std::optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext&, const String& kind)
{
    return gstreamerRtpCapatiblities(kind);
}

std::optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext&, const String& kind)
{
    return gstreamerRtpCapatiblities(kind);
}

GStreamerPeerConnectionBackend::GStreamerPeerConnectionBackend(RTCPeerConnection& peerConnection)
    : PeerConnectionBackend(peerConnection)
    , m_endpoint(GStreamerMediaEndpoint::create(*this))
{
    disableICECandidateFiltering();
}

GStreamerPeerConnectionBackend::~GStreamerPeerConnectionBackend() = default;

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

void GStreamerPeerConnectionBackend::getStats(Ref<DeferredPromise>&& promise)
{
    m_endpoint->getStats(nullptr, WTFMove(promise));
}

static inline GStreamerRtpSenderBackend& backendFromRTPSender(RTCRtpSender& sender)
{
    ASSERT(!sender.isStopped());
    return static_cast<GStreamerRtpSenderBackend&>(*sender.backend());
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
    if (!receiver.backend()) {
        m_endpoint->getStats(nullptr, WTFMove(promise));
        return;
    }

    GstElement* bin = nullptr;
    auto& source = receiver.track().privateTrack().source();
    if (source.isIncomingAudioSource())
        bin = static_cast<RealtimeIncomingAudioSourceGStreamer&>(source).bin();
    else if (source.isIncomingVideoSource())
        bin = static_cast<RealtimeIncomingVideoSourceGStreamer&>(source).bin();
    else
        RELEASE_ASSERT_NOT_REACHED();

    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin, "sink"));
    auto srcPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
    m_endpoint->getStats(srcPad.get(), WTFMove(promise));
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
        createAnswerFailed(Exception { InvalidStateError, "No remote description set"_s });
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
    m_pendingReceivers.clear();
}

void GStreamerPeerConnectionBackend::doAddIceCandidate(RTCIceCandidate& candidate, AddIceCandidateCallback&& callback)
{
    unsigned sdpMLineIndex = candidate.sdpMLineIndex() ? candidate.sdpMLineIndex().value() : 0;
    auto rtcCandidate = WTF::makeUnique<GStreamerIceCandidate>(*new GStreamerIceCandidate { sdpMLineIndex, candidate.candidate() });
    m_endpoint->addIceCandidate(*rtcCandidate, WTFMove(callback));
}

Ref<RTCRtpReceiver> GStreamerPeerConnectionBackend::createReceiver(std::unique_ptr<GStreamerRtpReceiverBackend>&& backend, const String& trackKind, const String& trackId)
{
    auto& document = downcast<Document>(*m_peerConnection.scriptExecutionContext());

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

static inline RefPtr<RTCRtpSender> findExistingSender(const Vector<RefPtr<RTCRtpTransceiver>>& transceivers, GStreamerRtpSenderBackend& senderBackend)
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
    auto senderBackend = WTF::makeUnique<GStreamerRtpSenderBackend>(*this, nullptr, nullptr);
    if (!m_endpoint->addTrack(*senderBackend, track, mediaStreamIds))
        return Exception { TypeError, "Unable to add track"_s };

    if (auto sender = findExistingSender(m_peerConnection.currentTransceivers(), *senderBackend)) {
        backendFromRTPSender(*sender).takeSource(*senderBackend);
        sender->setTrack(Ref(track));
        sender->setMediaStreamIds(WTFMove(mediaStreamIds));
        return sender.releaseNonNull();
    }

    auto transceiverBackend = m_endpoint->transceiverBackendFromSender(*senderBackend);

    auto sender = RTCRtpSender::create(m_peerConnection, Ref(track), WTFMove(senderBackend));
    sender->setMediaStreamIds(WTFMove(mediaStreamIds));
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), track.kind(), track.id());
    auto transceiver = RTCRtpTransceiver::create(sender.copyRef(), WTFMove(receiver), WTFMove(transceiverBackend));
    m_peerConnection.addInternalTransceiver(WTFMove(transceiver));
    return sender;
}

template<typename T>
ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiverFromTrackOrKind(T&& trackOrKind, const RTCRtpTransceiverInit& init)
{
    auto backends = m_endpoint->addTransceiver(trackOrKind, init);
    if (!backends)
        return Exception { InvalidAccessError, "Unable to add transceiver"_s };

    auto sender = RTCRtpSender::create(m_peerConnection, WTFMove(trackOrKind), WTFMove(backends->senderBackend));
    auto receiver = createReceiver(WTFMove(backends->receiverBackend), sender->trackKind(), sender->trackId());
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(backends->transceiverBackend));
    m_peerConnection.addInternalTransceiver(transceiver.copyRef());
    return transceiver;
}

ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiver(const String& trackKind, const RTCRtpTransceiverInit& init)
{
    return addTransceiverFromTrackOrKind(String { trackKind }, init);
}

ExceptionOr<Ref<RTCRtpTransceiver>> GStreamerPeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&& track, const RTCRtpTransceiverInit& init)
{
    return addTransceiverFromTrackOrKind(WTFMove(track), init);
}

void GStreamerPeerConnectionBackend::setSenderSourceFromTrack(GStreamerRtpSenderBackend& sender, MediaStreamTrack& track)
{
    m_endpoint->setSenderSourceFromTrack(sender, track);
}

static inline GStreamerRtpTransceiverBackend& backendFromRTPTransceiver(RTCRtpTransceiver& transceiver)
{
    return static_cast<GStreamerRtpTransceiverBackend&>(*transceiver.backend());
}

RTCRtpTransceiver* GStreamerPeerConnectionBackend::existingTransceiver(WTF::Function<bool(GStreamerRtpTransceiverBackend&)>&& matchingFunction)
{
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
        if (matchingFunction(backendFromRTPTransceiver(*transceiver)))
            return transceiver.get();
    }
    return nullptr;
}

RTCRtpTransceiver& GStreamerPeerConnectionBackend::newRemoteTransceiver(std::unique_ptr<GStreamerRtpTransceiverBackend>&& transceiverBackend, RealtimeMediaSource::Type type)
{
    auto trackKind = type == RealtimeMediaSource::Type::Audio ? "audio"_s : "video"_s;
    auto sender = RTCRtpSender::create(m_peerConnection, trackKind, transceiverBackend->createSenderBackend(*this, nullptr, nullptr));
    auto receiver = createReceiver(transceiverBackend->createReceiverBackend(), trackKind, sender->trackId());
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver), WTFMove(transceiverBackend));
    m_peerConnection.addInternalTransceiver(transceiver.copyRef());
    return transceiver.get();
}

void GStreamerPeerConnectionBackend::collectTransceivers()
{
    m_endpoint->collectTransceivers();
}

void GStreamerPeerConnectionBackend::addPendingTrackEvent(PendingTrackEvent&& event)
{
    m_pendingTrackEvents.append(WTFMove(event));
}

void GStreamerPeerConnectionBackend::dispatchPendingTrackEvents()
{
    auto events = WTFMove(m_pendingTrackEvents);
    for (auto& event : events)
        dispatchTrackEvent(event);
}

void GStreamerPeerConnectionBackend::removeTrack(RTCRtpSender& sender)
{
    m_endpoint->removeTrack(backendFromRTPSender(sender));
}

void GStreamerPeerConnectionBackend::applyRotationForOutgoingVideoSources()
{
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
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

bool GStreamerPeerConnectionBackend::isNegotiationNeeded(uint32_t) const
{
    return m_endpoint->isNegotiationNeeded();
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
