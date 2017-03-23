/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "JSDOMConvert.h"
#include "JSRTCStatsReport.h"
#include "LibWebRTCDataChannelHandler.h"
#include "LibWebRTCPeerConnectionBackend.h"
#include "LibWebRTCProvider.h"
#include "MediaStreamEvent.h"
#include "NotImplemented.h"
#include "PlatformStrategies.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include "RTCPeerConnection.h"
#include "RTCSessionDescription.h"
#include "RTCStatsReport.h"
#include "RTCTrackEvent.h"
#include "RealtimeIncomingAudioSource.h"
#include "RealtimeIncomingVideoSource.h"
#include "RuntimeEnabledFeatures.h"
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/p2p/base/basicpacketsocketfactory.h>
#include <webrtc/p2p/client/basicportallocator.h>
#include <webrtc/pc/peerconnectionfactory.h>
#include <wtf/MainThread.h>

#include "CoreMediaSoftLink.h"

namespace WebCore {

LibWebRTCMediaEndpoint::LibWebRTCMediaEndpoint(LibWebRTCPeerConnectionBackend& peerConnection, LibWebRTCProvider& client)
    : m_peerConnectionBackend(peerConnection)
    , m_backend(client.createPeerConnection(*this))
    , m_createSessionDescriptionObserver(*this)
    , m_setLocalSessionDescriptionObserver(*this)
    , m_setRemoteSessionDescriptionObserver(*this)
{
    ASSERT(m_backend);
}

// FIXME: unify with MediaEndpointSessionDescription::typeString()
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
    String sdpString(sdp.data(), sdp.size());

    return RTCSessionDescription::create(fromSessionDescriptionType(*description), WTFMove(sdpString));
}

// FIXME: We might want to create a new object only if the session actually changed for all description getters.
RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::currentLocalDescription() const
{
    return fromSessionDescription(m_backend->current_local_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::currentRemoteDescription() const
{
    return fromSessionDescription(m_backend->current_remote_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::pendingLocalDescription() const
{
    return fromSessionDescription(m_backend->pending_local_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::pendingRemoteDescription() const
{
    return fromSessionDescription(m_backend->pending_remote_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::localDescription() const
{
    return fromSessionDescription(m_backend->local_description());
}

RefPtr<RTCSessionDescription> LibWebRTCMediaEndpoint::remoteDescription() const
{
    // FIXME: We might want to create a new object only if the session actually changed.
    return fromSessionDescription(m_backend->remote_description());
}

void LibWebRTCMediaEndpoint::doSetLocalDescription(RTCSessionDescription& description)
{
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));

    if (!sessionDescription) {
        String errorMessage(error.description.data(), error.description.size());
        m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, WTFMove(errorMessage) });
        return;
    }
    m_backend->SetLocalDescription(&m_setLocalSessionDescriptionObserver, sessionDescription.release());
}

void LibWebRTCMediaEndpoint::doSetRemoteDescription(RTCSessionDescription& description)
{
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription(webrtc::CreateSessionDescription(sessionDescriptionType(description.type()), description.sdp().utf8().data(), &error));
    if (!sessionDescription) {
        String errorMessage(error.description.data(), error.description.size());
        m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { OperationError, WTFMove(errorMessage) });
        return;
    }
    m_backend->SetRemoteDescription(&m_setRemoteSessionDescriptionObserver, sessionDescription.release());
}

void LibWebRTCMediaEndpoint::addTrack(MediaStreamTrack& track, const Vector<String>& mediaStreamIds)
{
    if (!LibWebRTCProvider::factory())
        return;

    std::vector<webrtc::MediaStreamInterface*> mediaStreams;
    rtc::scoped_refptr<webrtc::MediaStreamInterface> mediaStream = nullptr;
    if (mediaStreamIds.size()) {
        // libwebrtc is only using the first one if any.
        mediaStream = LibWebRTCProvider::factory()->CreateLocalMediaStream(mediaStreamIds[0].utf8().data());
        mediaStreams.push_back(mediaStream.get());
    }
    
    auto& source = track.source();
    switch (source.type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto trackSource = RealtimeOutgoingAudioSource::create(source);
        auto audioTrack = LibWebRTCProvider::factory()->CreateAudioTrack(track.id().utf8().data(), trackSource.ptr());
        trackSource->setTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface>(audioTrack));
        m_peerConnectionBackend.addAudioSource(WTFMove(trackSource));
        m_backend->AddTrack(audioTrack.get(), WTFMove(mediaStreams));
        return;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(source);
        auto videoTrack = LibWebRTCProvider::factory()->CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        m_peerConnectionBackend.addVideoSource(WTFMove(videoSource));
        m_backend->AddTrack(videoTrack.get(), WTFMove(mediaStreams));
        return;
    }
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
}

void LibWebRTCMediaEndpoint::doCreateOffer()
{
    if (!LibWebRTCProvider::factory()) {
        m_peerConnectionBackend.createOfferFailed(Exception { NOT_SUPPORTED_ERR, ASCIILiteral("libwebrtc backend is missing.") });
        return;
    }
        
    m_isInitiator = true;
    m_backend->CreateOffer(&m_createSessionDescriptionObserver, nullptr);
}

void LibWebRTCMediaEndpoint::doCreateAnswer()
{
    if (!LibWebRTCProvider::factory()) {
        m_peerConnectionBackend.createAnswerFailed(Exception { NOT_SUPPORTED_ERR, ASCIILiteral("libwebrtc backend is missing.") });
        return;
    }

    m_isInitiator = false;
    m_backend->CreateAnswer(&m_createSessionDescriptionObserver, nullptr);
}

void LibWebRTCMediaEndpoint::getStats(MediaStreamTrack* track, const DeferredPromise& promise)
{
    UNUSED_PARAM(track);
    UNUSED_PARAM(promise);
    m_backend->GetStats(StatsCollector::create(*this, promise, track).get());
}

LibWebRTCMediaEndpoint::StatsCollector::StatsCollector(Ref<LibWebRTCMediaEndpoint>&& endpoint, const DeferredPromise& promise, MediaStreamTrack* track)
    : m_endpoint(WTFMove(endpoint))
    , m_promise(promise)
{
    if (track)
        m_id = track->id();
}

static inline String fromStdString(const std::string& value)
{
    return String(value.data(), value.length());
}

static inline void fillRTCStats(RTCStatsReport::Stats& stats, const webrtc::RTCStats& rtcStats)
{
    stats.timestamp = rtcStats.timestamp_us();
    stats.id = fromStdString(rtcStats.id());
}

static inline void fillRTCRTPStreamStats(RTCStatsReport::RTCRTPStreamStats& stats, const webrtc::RTCRTPStreamStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);
    if (rtcStats.ssrc.is_defined())
        stats.ssrc = *rtcStats.ssrc;
    if (rtcStats.associate_stats_id.is_defined())
        stats.associateStatsId = fromStdString(*rtcStats.associate_stats_id);
    if (rtcStats.is_remote.is_defined())
        stats.isRemote = *rtcStats.is_remote;
    if (rtcStats.media_type.is_defined())
        stats.mediaType = fromStdString(*rtcStats.media_type);
    if (rtcStats.track_id.is_defined())
        stats.mediaTrackId = fromStdString(*rtcStats.track_id);
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        stats.codecId = fromStdString(*rtcStats.codec_id);
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.sli_count.is_defined())
        stats.sliCount = *rtcStats.sli_count;
    // FIXME: Set qpSum
    stats.qpSum = 0;
}

static inline void fillInboundRTPStreamStats(RTCStatsReport::InboundRTPStreamStats& stats, const webrtc::RTCInboundRTPStreamStats& rtcStats)
{
    fillRTCRTPStreamStats(stats, rtcStats);
    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.packets_lost.is_defined())
        stats.packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter.is_defined())
        stats.jitter = *rtcStats.jitter;
    if (rtcStats.fraction_lost.is_defined())
        stats.fractionLost = *rtcStats.fraction_lost;
    if (rtcStats.packets_discarded.is_defined())
        stats.packetsDiscarded = *rtcStats.packets_discarded;
    if (rtcStats.packets_repaired.is_defined())
        stats.packetsRepaired = *rtcStats.packets_repaired;
    if (rtcStats.burst_packets_lost.is_defined())
        stats.burstPacketsLost = *rtcStats.burst_packets_lost;
    if (rtcStats.burst_packets_discarded.is_defined())
        stats.burstPacketsDiscarded = *rtcStats.burst_packets_discarded;
    if (rtcStats.burst_loss_count.is_defined())
        stats.burstLossCount = *rtcStats.burst_loss_count;
    if (rtcStats.burst_discard_count.is_defined())
        stats.burstDiscardCount = *rtcStats.burst_discard_count;
    if (rtcStats.burst_loss_rate.is_defined())
        stats.burstLossRate = *rtcStats.burst_loss_rate;
    if (rtcStats.burst_discard_rate.is_defined())
        stats.burstDiscardRate = *rtcStats.burst_discard_rate;
    if (rtcStats.gap_loss_rate.is_defined())
        stats.gapLossRate = *rtcStats.gap_loss_rate;
    if (rtcStats.gap_discard_rate.is_defined())
        stats.gapDiscardRate = *rtcStats.gap_discard_rate;
    // FIXME: Set framesDecoded
    stats.framesDecoded = 0;
}

static inline void fillOutboundRTPStreamStats(RTCStatsReport::OutboundRTPStreamStats& stats, const webrtc::RTCOutboundRTPStreamStats& rtcStats)
{
    fillRTCRTPStreamStats(stats, rtcStats);

    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.target_bitrate.is_defined())
        stats.targetBitrate = *rtcStats.target_bitrate;
    // FIXME: Set framesEncoded
    stats.framesEncoded = 0;
}

void LibWebRTCMediaEndpoint::StatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    callOnMainThread([protectedThis = rtc::scoped_refptr<LibWebRTCMediaEndpoint::StatsCollector>(this), rtcReport] {
        if (protectedThis->m_endpoint->isStopped())
            return;

        auto report = RTCStatsReport::create();
        protectedThis->m_endpoint->m_peerConnectionBackend.getStatsSucceeded(protectedThis->m_promise, report.copyRef());
        ASSERT(report->backingMap());

        for (const auto& rtcStats : *rtcReport) {
            if (rtcStats.type() == webrtc::RTCInboundRTPStreamStats::kType) {
                RTCStatsReport::InboundRTPStreamStats stats;
                fillInboundRTPStreamStats(stats, static_cast<const webrtc::RTCInboundRTPStreamStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::InboundRTPStreamStats>>(WTFMove(stats));
                return;
            }
            if (rtcStats.type() == webrtc::RTCOutboundRTPStreamStats::kType) {
                RTCStatsReport::OutboundRTPStreamStats stats;
                fillOutboundRTPStreamStats(stats, static_cast<const webrtc::RTCOutboundRTPStreamStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::OutboundRTPStreamStats>>(WTFMove(stats));
                return;
            }
        }
    });
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
        return RTCSignalingState::Closed;
    }
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

static inline String trackId(webrtc::MediaStreamTrackInterface& videoTrack)
{
    return String(videoTrack.id().data(), videoTrack.id().size());
}

MediaStream& LibWebRTCMediaEndpoint::mediaStreamFromRTCStream(webrtc::MediaStreamInterface* rtcStream)
{
    auto mediaStream = m_streams.ensure(rtcStream, [this] {
        auto stream = MediaStream::create(*m_peerConnectionBackend.connection().scriptExecutionContext());
        auto streamPointer = stream.ptr();
        m_peerConnectionBackend.addRemoteStream(WTFMove(stream));
        return streamPointer;
    });
    return *mediaStream.iterator->value;
}

void LibWebRTCMediaEndpoint::addRemoteStream(webrtc::MediaStreamInterface& rtcStream)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webRTCLegacyAPIEnabled())
        return;

    auto& mediaStream = mediaStreamFromRTCStream(&rtcStream);
    m_peerConnectionBackend.connection().fireEvent(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, &mediaStream));
}

void LibWebRTCMediaEndpoint::addRemoteTrack(const webrtc::RtpReceiverInterface& rtcReceiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& rtcStreams)
{
    RefPtr<RTCRtpReceiver> receiver;
    RefPtr<RealtimeMediaSource> remoteSource;

    auto* rtcTrack = rtcReceiver.track().get();

    switch (rtcReceiver.media_type()) {
    case cricket::MEDIA_TYPE_DATA:
        return;
    case cricket::MEDIA_TYPE_AUDIO: {
        rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack = static_cast<webrtc::AudioTrackInterface*>(rtcTrack);
        auto audioReceiver = m_peerConnectionBackend.audioReceiver(trackId(*rtcTrack));

        receiver = WTFMove(audioReceiver.receiver);
        audioReceiver.source->setSourceTrack(WTFMove(audioTrack));
        break;
    }
    case cricket::MEDIA_TYPE_VIDEO: {
        rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack = static_cast<webrtc::VideoTrackInterface*>(rtcTrack);
        auto videoReceiver = m_peerConnectionBackend.videoReceiver(trackId(*rtcTrack));

        receiver = WTFMove(videoReceiver.receiver);
        videoReceiver.source->setSourceTrack(WTFMove(videoTrack));
        break;
    }
    }

    auto* track = receiver->track();
    ASSERT(track);

    Vector<RefPtr<MediaStream>> streams;
    for (auto& rtcStream : rtcStreams) {
        auto& mediaStream = mediaStreamFromRTCStream(rtcStream.get());
        streams.append(&mediaStream);
        mediaStream.addTrackFromPlatform(*track);
    }
    m_peerConnectionBackend.connection().fireEvent(RTCTrackEvent::create(eventNames().trackEvent, false, false, WTFMove(receiver), track, WTFMove(streams), nullptr));
}

void LibWebRTCMediaEndpoint::removeRemoteStream(webrtc::MediaStreamInterface& rtcStream)
{
    auto* mediaStream = m_streams.take(&rtcStream);
    if (mediaStream)
        m_peerConnectionBackend.removeRemoteStream(mediaStream);
}

void LibWebRTCMediaEndpoint::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    callOnMainThread([protectedThis = makeRef(*this), stream = WTFMove(stream)] {
        if (protectedThis->isStopped())
            return;
        ASSERT(stream);
        protectedThis->addRemoteStream(*stream.get());
    });
}

void LibWebRTCMediaEndpoint::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
    callOnMainThread([protectedThis = makeRef(*this), stream = WTFMove(stream)] {
        if (protectedThis->isStopped())
            return;
        ASSERT(stream);
        protectedThis->removeRemoteStream(*stream.get());
    });
}

void LibWebRTCMediaEndpoint::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
    callOnMainThread([protectedThis = makeRef(*this), receiver = WTFMove(receiver), streams] {
        if (protectedThis->isStopped())
            return;
        ASSERT(receiver);
        protectedThis->addRemoteTrack(*receiver, streams);
    });
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    webrtc::DataChannelInit init;
    init.ordered = options.ordered;
    init.maxRetransmitTime = options.maxRetransmitTime;
    init.maxRetransmits = options.maxRetransmits;
    init.protocol = options.protocol.utf8().data();
    init.negotiated = options.negotiated;
    init.id = options.id;

    return std::make_unique<LibWebRTCDataChannelHandler>(m_backend->CreateDataChannel(label.utf8().data(), &init));
}

void LibWebRTCMediaEndpoint::addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>&& dataChannel)
{
    auto protocol = dataChannel->protocol();
    auto label = dataChannel->label();

    RTCDataChannelInit init;
    init.ordered = dataChannel->ordered();
    init.maxRetransmitTime = dataChannel->maxRetransmitTime();
    init.maxRetransmits = dataChannel->maxRetransmits();
    init.protocol = String(protocol.data(), protocol.size());
    init.negotiated = dataChannel->negotiated();
    init.id = dataChannel->id();

    bool isOpened = dataChannel->state() == webrtc::DataChannelInterface::kOpen;

    auto handler =  std::make_unique<LibWebRTCDataChannelHandler>(WTFMove(dataChannel));
    ASSERT(m_peerConnectionBackend.connection().scriptExecutionContext());
    auto channel = RTCDataChannel::create(*m_peerConnectionBackend.connection().scriptExecutionContext(), WTFMove(handler), String(label.data(), label.size()), WTFMove(init));

    if (isOpened) {
        callOnMainThread([channel = channel.copyRef()] {
            // FIXME: We should be able to write channel->didChangeReadyState(...)
            RTCDataChannelHandlerClient& client = channel.get();
            client.didChangeReadyState(RTCDataChannel::ReadyStateOpen);
        });
    }

    m_peerConnectionBackend.connection().fireEvent(RTCDataChannelEvent::create(eventNames().datachannelEvent, false, false, WTFMove(channel)));
}

void LibWebRTCMediaEndpoint::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{
    callOnMainThread([protectedThis = makeRef(*this), dataChannel = WTFMove(dataChannel)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>(dataChannel));
    });
}

void LibWebRTCMediaEndpoint::stop()
{
    ASSERT(m_backend);
    m_backend->Close();
    m_backend = nullptr;
    m_streams.clear();
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
        ASSERT_NOT_REACHED();
        return RTCIceConnectionState::New;
    }
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
    String candidateSDP(sdp.data(), sdp.size());

    auto mid = rtcCandidate->sdp_mid();
    String candidateMid(mid.data(), mid.size());

    callOnMainThread([protectedThis = makeRef(*this), mid = WTFMove(candidateMid), sdp = WTFMove(candidateSDP)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.newICECandidate(String(sdp), String(mid));
    });
}

void LibWebRTCMediaEndpoint::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&)
{
    ASSERT_NOT_REACHED();
}

void LibWebRTCMediaEndpoint::createSessionDescriptionSucceeded(webrtc::SessionDescriptionInterface* description)
{
    std::string sdp;
    description->ToString(&sdp);
    String sdpString(sdp.data(), sdp.size());

    callOnMainThread([protectedThis = makeRef(*this), sdp = WTFMove(sdpString)] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferSucceeded(String(sdp));
        else
            protectedThis->m_peerConnectionBackend.createAnswerSucceeded(String(sdp));
    });
}

void LibWebRTCMediaEndpoint::createSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        if (protectedThis->m_isInitiator)
            protectedThis->m_peerConnectionBackend.createOfferFailed(Exception { OperationError, String(error) });
        else
            protectedThis->m_peerConnectionBackend.createAnswerFailed(Exception { OperationError, String(error) });
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

void LibWebRTCMediaEndpoint::setLocalSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setLocalDescriptionFailed(Exception { OperationError, String(error) });
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

void LibWebRTCMediaEndpoint::setRemoteSessionDescriptionFailed(const std::string& errorMessage)
{
    String error(errorMessage.data(), errorMessage.size());
    callOnMainThread([protectedThis = makeRef(*this), error = WTFMove(error)] {
        if (protectedThis->isStopped())
            return;
        protectedThis->m_peerConnectionBackend.setRemoteDescriptionFailed(Exception { OperationError, String(error) });
    });
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
