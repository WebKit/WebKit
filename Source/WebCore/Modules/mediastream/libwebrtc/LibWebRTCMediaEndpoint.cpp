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
#include "RTCOfferOptions.h"
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
    , m_peerConnectionFactory(*client.factory())
    , m_backend(client.createPeerConnection(*this))
    , m_createSessionDescriptionObserver(*this)
    , m_setLocalSessionDescriptionObserver(*this)
    , m_setRemoteSessionDescriptionObserver(*this)
{
    ASSERT(m_backend);
    ASSERT(client.factory());
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

void LibWebRTCMediaEndpoint::addTrack(RTCRtpSender& sender, MediaStreamTrack& track, const Vector<String>& mediaStreamIds)
{
    std::vector<webrtc::MediaStreamInterface*> mediaStreams;
    rtc::scoped_refptr<webrtc::MediaStreamInterface> mediaStream = nullptr;
    if (mediaStreamIds.size()) {
        // libwebrtc is only using the first one if any.
        mediaStream = m_peerConnectionFactory.CreateLocalMediaStream(mediaStreamIds[0].utf8().data());
        mediaStreams.push_back(mediaStream.get());
    }
    
    auto& source = track.source();
    switch (source.type()) {
    case RealtimeMediaSource::Type::Audio: {
        auto trackSource = RealtimeOutgoingAudioSource::create(source);
        auto audioTrack = m_peerConnectionFactory.CreateAudioTrack(track.id().utf8().data(), trackSource.ptr());
        m_peerConnectionBackend.addAudioSource(WTFMove(trackSource));
        m_senders.add(&sender, m_backend->AddTrack(audioTrack.get(), WTFMove(mediaStreams)));
        return;
    }
    case RealtimeMediaSource::Type::Video: {
        auto videoSource = RealtimeOutgoingVideoSource::create(source);
        auto videoTrack = m_peerConnectionFactory.CreateVideoTrack(track.id().utf8().data(), videoSource.ptr());
        m_peerConnectionBackend.addVideoSource(WTFMove(videoSource));
        m_senders.add(&sender, m_backend->AddTrack(videoTrack.get(), WTFMove(mediaStreams)));
        return;
    }
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
}

void LibWebRTCMediaEndpoint::removeTrack(RTCRtpSender& sender)
{
    auto rtcSender = m_senders.get(&sender);
    if (!rtcSender)
        return;
    m_backend->RemoveTrack(rtcSender.get());
}

void LibWebRTCMediaEndpoint::doCreateOffer(const RTCOfferOptions& options)
{
    m_isInitiator = true;
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions rtcOptions;
    rtcOptions.ice_restart = options.iceRestart;
    rtcOptions.voice_activity_detection = options.voiceActivityDetection;
    m_backend->CreateOffer(&m_createSessionDescriptionObserver, rtcOptions);
}

void LibWebRTCMediaEndpoint::doCreateAnswer()
{
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
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
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
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
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
    if (rtcStats.frames_encoded.is_defined())
        stats.framesEncoded = *rtcStats.frames_encoded;
}

static inline void fillRTCMediaStreamTrackStats(RTCStatsReport::MediaStreamTrackStats& stats, const webrtc::RTCMediaStreamTrackStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.track_identifier.is_defined())
        stats.trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.remote_source.is_defined())
        stats.remoteSource = *rtcStats.remote_source;
    if (rtcStats.ended.is_defined())
        stats.ended = *rtcStats.ended;
    if (rtcStats.detached.is_defined())
        stats.detached = *rtcStats.detached;
    if (rtcStats.frame_width.is_defined())
        stats.frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        stats.frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        stats.framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.frames_sent.is_defined())
        stats.framesSent = *rtcStats.frames_sent;
    if (rtcStats.frames_received.is_defined())
        stats.framesReceived = *rtcStats.frames_received;
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.frames_dropped.is_defined())
        stats.framesDropped = *rtcStats.frames_dropped;
    if (rtcStats.partial_frames_lost.is_defined())
        stats.partialFramesLost = *rtcStats.partial_frames_lost;
    if (rtcStats.full_frames_lost.is_defined())
        stats.fullFramesLost = *rtcStats.full_frames_lost;
    if (rtcStats.audio_level.is_defined())
        stats.audioLevel = *rtcStats.audio_level;
    if (rtcStats.echo_return_loss.is_defined())
        stats.echoReturnLoss = *rtcStats.echo_return_loss;
    if (rtcStats.echo_return_loss_enhancement.is_defined())
        stats.echoReturnLossEnhancement = *rtcStats.echo_return_loss_enhancement;
}

static inline void fillRTCDataChannelStats(RTCStatsReport::DataChannelStats& stats, const webrtc::RTCDataChannelStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.label.is_defined())
        stats.label = fromStdString(*rtcStats.label);
    if (rtcStats.protocol.is_defined())
        stats.protocol = fromStdString(*rtcStats.protocol);
    if (rtcStats.datachannelid.is_defined())
        stats.datachannelid = *rtcStats.datachannelid;
    if (rtcStats.state.is_defined())
        stats.state = fromStdString(*rtcStats.state);
    if (rtcStats.messages_sent.is_defined())
        stats.messagesSent = *rtcStats.messages_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.messages_received.is_defined())
        stats.messagesReceived = *rtcStats.messages_received;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
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
            } else if (rtcStats.type() == webrtc::RTCOutboundRTPStreamStats::kType) {
                RTCStatsReport::OutboundRTPStreamStats stats;
                fillOutboundRTPStreamStats(stats, static_cast<const webrtc::RTCOutboundRTPStreamStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::OutboundRTPStreamStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCMediaStreamTrackStats::kType) {
                RTCStatsReport::MediaStreamTrackStats stats;
                fillRTCMediaStreamTrackStats(stats, static_cast<const webrtc::RTCMediaStreamTrackStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::MediaStreamTrackStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCDataChannelStats::kType) {
                RTCStatsReport::DataChannelStats stats;
                fillRTCDataChannelStats(stats, static_cast<const webrtc::RTCDataChannelStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::DataChannelStats>>(WTFMove(stats));
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
        return RTCSignalingState::Stable;
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

MediaStream& LibWebRTCMediaEndpoint::mediaStreamFromRTCStream(webrtc::MediaStreamInterface& rtcStream)
{
    auto mediaStream = m_streams.ensure(&rtcStream, [&rtcStream, this] {
        auto label = rtcStream.label();
        auto stream = MediaStream::create(*m_peerConnectionBackend.connection().scriptExecutionContext(), MediaStreamPrivate::create({ }, String(label.data(), label.size())));
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

    auto& mediaStream = mediaStreamFromRTCStream(rtcStream);
    m_peerConnectionBackend.connection().fireEvent(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, &mediaStream));
}

class RTCRtpReceiverBackend final : public RTCRtpReceiver::Backend {
public:
    explicit RTCRtpReceiverBackend(rtc::scoped_refptr<webrtc::RtpReceiverInterface>&& rtcReceiver) : m_rtcReceiver(WTFMove(rtcReceiver)) { }
private:
    RTCRtpParameters getParameters() final;

    rtc::scoped_refptr<webrtc::RtpReceiverInterface> m_rtcReceiver;
};


void LibWebRTCMediaEndpoint::addRemoteTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>&& rtcReceiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& rtcStreams)
{
    ASSERT(rtcReceiver);
    RefPtr<RTCRtpReceiver> receiver;
    RefPtr<RealtimeMediaSource> remoteSource;

    auto* rtcTrack = rtcReceiver->track().get();

    switch (rtcReceiver->media_type()) {
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

    receiver->setBackend(std::make_unique<RTCRtpReceiverBackend>(WTFMove(rtcReceiver)));
    
    auto* track = receiver->track();
    ASSERT(track);

    Vector<RefPtr<MediaStream>> streams;
    for (auto& rtcStream : rtcStreams) {
        auto& mediaStream = mediaStreamFromRTCStream(*rtcStream.get());
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
    callOnMainThread([protectedThis = makeRef(*this), receiver = WTFMove(receiver), streams]() mutable {
        if (protectedThis->isStopped())
            return;
        protectedThis->addRemoteTrack(WTFMove(receiver), streams);
    });
}

std::unique_ptr<RTCDataChannelHandler> LibWebRTCMediaEndpoint::createDataChannel(const String& label, const RTCDataChannelInit& options)
{
    webrtc::DataChannelInit init;
    if (options.ordered)
        init.ordered = *options.ordered;
    if (options.maxPacketLifeTime)
        init.maxRetransmitTime = *options.maxPacketLifeTime;
    if (options.maxRetransmits)
        init.maxRetransmits = *options.maxRetransmits;
    init.protocol = options.protocol.utf8().data();
    if (options.negotiated)
        init.negotiated = *options.negotiated;
    if (options.id)
        init.id = *options.id;

    auto channel = m_backend->CreateDataChannel(label.utf8().data(), &init);
    if (!channel)
        return nullptr;

    return std::make_unique<LibWebRTCDataChannelHandler>(WTFMove(channel));
}

void LibWebRTCMediaEndpoint::addDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>&& dataChannel)
{
    auto protocol = dataChannel->protocol();
    auto label = dataChannel->label();

    RTCDataChannelInit init;
    init.ordered = dataChannel->ordered();
    init.maxPacketLifeTime = dataChannel->maxRetransmitTime();
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
            client.didChangeReadyState(RTCDataChannelState::Open);
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
    m_senders.clear();
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

static inline RTCRtpParameters::EncodingParameters fillEncodingParameters(const webrtc::RtpEncodingParameters& rtcParameters)
{
    RTCRtpParameters::EncodingParameters parameters;

    if (rtcParameters.ssrc)
        parameters.ssrc = *rtcParameters.ssrc;
    if (rtcParameters.rtx && rtcParameters.rtx->ssrc)
        parameters.rtx.ssrc = *rtcParameters.rtx->ssrc;
    if (rtcParameters.fec && rtcParameters.fec->ssrc)
        parameters.fec.ssrc = *rtcParameters.fec->ssrc;
    if (rtcParameters.dtx) {
        switch (*rtcParameters.dtx) {
        case webrtc::DtxStatus::DISABLED:
            parameters.dtx = RTCRtpParameters::DtxStatus::Disabled;
            break;
        case webrtc::DtxStatus::ENABLED:
            parameters.dtx = RTCRtpParameters::DtxStatus::Enabled;
        }
    }
    parameters.active = rtcParameters.active;
    if (rtcParameters.priority) {
        switch (*rtcParameters.priority) {
        case webrtc::PriorityType::VERY_LOW:
            parameters.priority = RTCRtpParameters::PriorityType::VeryLow;
            break;
        case webrtc::PriorityType::LOW:
            parameters.priority = RTCRtpParameters::PriorityType::Low;
            break;
        case webrtc::PriorityType::MEDIUM:
            parameters.priority = RTCRtpParameters::PriorityType::Medium;
            break;
        case webrtc::PriorityType::HIGH:
            parameters.priority = RTCRtpParameters::PriorityType::High;
            break;
        }
    }
    if (rtcParameters.max_bitrate_bps)
        parameters.maxBitrate = *rtcParameters.max_bitrate_bps;
    if (rtcParameters.max_framerate)
        parameters.maxFramerate = *rtcParameters.max_framerate;
    parameters.rid = fromStdString(rtcParameters.rid);
    parameters.scaleResolutionDownBy = rtcParameters.scale_resolution_down_by;

    return parameters;
}

static inline RTCRtpParameters::HeaderExtensionParameters fillHeaderExtensionParameters(const webrtc::RtpHeaderExtensionParameters& rtcParameters)
{
    RTCRtpParameters::HeaderExtensionParameters parameters;

    parameters.uri = fromStdString(rtcParameters.uri);
    parameters.id = rtcParameters.id;

    return parameters;
}

static inline RTCRtpParameters::CodecParameters fillCodecParameters(const webrtc::RtpCodecParameters& rtcParameters)
{
    RTCRtpParameters::CodecParameters parameters;

    parameters.payloadType = rtcParameters.payload_type;
    parameters.mimeType = fromStdString(rtcParameters.mime_type());
    if (rtcParameters.clock_rate)
        parameters.clockRate = *rtcParameters.clock_rate;
    if (rtcParameters.num_channels)
        parameters.channels = *rtcParameters.num_channels;

    return parameters;
}

static RTCRtpParameters fillRtpParameters(const webrtc::RtpParameters rtcParameters)
{
    RTCRtpParameters parameters;

    parameters.transactionId = fromStdString(rtcParameters.transaction_id);
    for (auto& rtcEncoding : rtcParameters.encodings)
        parameters.encodings.append(fillEncodingParameters(rtcEncoding));
    for (auto& extension : rtcParameters.header_extensions)
        parameters.headerExtensions.append(fillHeaderExtensionParameters(extension));
    for (auto& codec : rtcParameters.codecs)
        parameters.codecs.append(fillCodecParameters(codec));

    switch (rtcParameters.degradation_preference) {
    case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::MaintainFramerate;
        break;
    case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::MaintainResolution;
        break;
    case webrtc::DegradationPreference::BALANCED:
        parameters.degradationPreference = RTCRtpParameters::DegradationPreference::Balanced;
        break;
    };
    return parameters;
}

RTCRtpParameters RTCRtpReceiverBackend::getParameters()
{
    return fillRtpParameters(m_rtcReceiver->GetParameters());
}

RTCRtpParameters LibWebRTCMediaEndpoint::getRTCRtpSenderParameters(RTCRtpSender& sender)
{
    auto rtcSender = m_senders.get(&sender);
    if (!rtcSender)
        return { };
    return fillRtpParameters(rtcSender->GetParameters());
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
