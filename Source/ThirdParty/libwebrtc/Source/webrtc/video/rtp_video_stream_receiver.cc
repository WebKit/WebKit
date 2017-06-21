/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/rtp_video_stream_receiver.h"

#include <utility>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_types.h"
#include "webrtc/config.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/ulpfec_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/h264_sprop_parameter_sets.h"
#include "webrtc/modules/video_coding/h264_sps_pps_tracker.h"
#include "webrtc/modules/video_coding/packet_buffer.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/timestamp_extrapolator.h"
#include "webrtc/video/receive_statistics_proxy.h"

namespace webrtc {

namespace {
constexpr int kPacketBufferStartSize = 32;
constexpr int kPacketBufferMaxSixe = 2048;
}

std::unique_ptr<RtpRtcp> CreateRtpRtcpModule(
    ReceiveStatistics* receive_statistics,
    Transport* outgoing_transport,
    RtcpRttStats* rtt_stats,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    TransportSequenceNumberAllocator* transport_sequence_number_allocator) {
  RtpRtcp::Configuration configuration;
  configuration.audio = false;
  configuration.receiver_only = true;
  configuration.receive_statistics = receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.intra_frame_callback = nullptr;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer =
      rtcp_packet_type_counter_observer;
  configuration.transport_sequence_number_allocator =
      transport_sequence_number_allocator;
  configuration.send_bitrate_observer = nullptr;
  configuration.send_frame_count_observer = nullptr;
  configuration.send_side_delay_observer = nullptr;
  configuration.send_packet_observer = nullptr;
  configuration.bandwidth_callback = nullptr;
  configuration.transport_feedback_callback = nullptr;

  std::unique_ptr<RtpRtcp> rtp_rtcp(RtpRtcp::CreateRtpRtcp(configuration));
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);

  return rtp_rtcp;
}

static const int kPacketLogIntervalMs = 10000;

RtpVideoStreamReceiver::RtpVideoStreamReceiver(
    Transport* transport,
    RtcpRttStats* rtt_stats,
    PacketRouter* packet_router,
    const VideoReceiveStream::Config* config,
    ReceiveStatisticsProxy* receive_stats_proxy,
    ProcessThread* process_thread,
    NackSender* nack_sender,
    KeyFrameRequestSender* keyframe_request_sender,
    video_coding::OnCompleteFrameCallback* complete_frame_callback,
    VCMTiming* timing)
    : clock_(Clock::GetRealTimeClock()),
      config_(*config),
      packet_router_(packet_router),
      process_thread_(process_thread),
      ntp_estimator_(clock_),
      rtp_header_parser_(RtpHeaderParser::Create()),
      rtp_receiver_(RtpReceiver::CreateVideoReceiver(clock_,
                                                     this,
                                                     this,
                                                     &rtp_payload_registry_)),
      rtp_receive_statistics_(ReceiveStatistics::Create(clock_)),
      ulpfec_receiver_(UlpfecReceiver::Create(this)),
      receiving_(false),
      restored_packet_in_use_(false),
      last_packet_log_ms_(-1),
      rtp_rtcp_(CreateRtpRtcpModule(rtp_receive_statistics_.get(),
                                    transport,
                                    rtt_stats,
                                    receive_stats_proxy,
                                    packet_router)),
      complete_frame_callback_(complete_frame_callback),
      keyframe_request_sender_(keyframe_request_sender),
      timing_(timing),
      has_received_frame_(false) {
  packet_router_->AddReceiveRtpModule(rtp_rtcp_.get());
  rtp_receive_statistics_->RegisterRtpStatisticsCallback(receive_stats_proxy);
  rtp_receive_statistics_->RegisterRtcpStatisticsCallback(receive_stats_proxy);

  RTC_DCHECK(config_.rtp.rtcp_mode != RtcpMode::kOff)
      << "A stream should not be configured with RTCP disabled. This value is "
         "reserved for internal usage.";
  RTC_DCHECK(config_.rtp.remote_ssrc != 0);
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  RTC_DCHECK(config_.rtp.local_ssrc != 0);
  RTC_DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  rtp_rtcp_->SetRTCPStatus(config_.rtp.rtcp_mode);
  rtp_rtcp_->SetSSRC(config_.rtp.local_ssrc);
  rtp_rtcp_->SetRemoteSSRC(config_.rtp.remote_ssrc);
  rtp_rtcp_->SetKeyFrameRequestMethod(kKeyFrameReqPliRtcp);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    EnableReceiveRtpHeaderExtension(config_.rtp.extensions[i].uri,
                                    config_.rtp.extensions[i].id);
  }

  static const int kMaxPacketAgeToNack = 450;
  const int max_reordering_threshold = (config_.rtp.nack.rtp_history_ms > 0)
                                           ? kMaxPacketAgeToNack
                                           : kDefaultMaxReorderingThreshold;
  rtp_receive_statistics_->SetMaxReorderingThreshold(max_reordering_threshold);

  if (config_.rtp.rtx_ssrc) {
    rtp_payload_registry_.SetRtxSsrc(config_.rtp.rtx_ssrc);

    for (const auto& kv : config_.rtp.rtx_payload_types) {
      RTC_DCHECK(kv.second != 0);
      rtp_payload_registry_.SetRtxPayloadType(kv.second, kv.first);
    }
  }

  if (IsUlpfecEnabled()) {
    VideoCodec ulpfec_codec = {};
    ulpfec_codec.codecType = kVideoCodecULPFEC;
    strncpy(ulpfec_codec.plName, "ulpfec", sizeof(ulpfec_codec.plName));
    ulpfec_codec.plType = config_.rtp.ulpfec.ulpfec_payload_type;
    RTC_CHECK(AddReceiveCodec(ulpfec_codec));
  }

  if (IsRedEnabled()) {
    VideoCodec red_codec = {};
    red_codec.codecType = kVideoCodecRED;
    strncpy(red_codec.plName, "red", sizeof(red_codec.plName));
    red_codec.plType = config_.rtp.ulpfec.red_payload_type;
    RTC_CHECK(AddReceiveCodec(red_codec));
    if (config_.rtp.ulpfec.red_rtx_payload_type != -1) {
      rtp_payload_registry_.SetRtxPayloadType(
          config_.rtp.ulpfec.red_rtx_payload_type,
          config_.rtp.ulpfec.red_payload_type);
    }
  }

  if (config_.rtp.rtcp_xr.receiver_reference_time_report)
    rtp_rtcp_->SetRtcpXrRrtrStatus(true);

  // Stats callback for CNAME changes.
  rtp_rtcp_->RegisterRtcpStatisticsCallback(receive_stats_proxy);

  process_thread_->RegisterModule(rtp_rtcp_.get(), RTC_FROM_HERE);

  if (config_.rtp.nack.rtp_history_ms != 0) {
    nack_module_.reset(
        new NackModule(clock_, nack_sender, keyframe_request_sender));
    process_thread_->RegisterModule(nack_module_.get(), RTC_FROM_HERE);
  }

  packet_buffer_ = video_coding::PacketBuffer::Create(
      clock_, kPacketBufferStartSize, kPacketBufferMaxSixe, this);
  reference_finder_.reset(new video_coding::RtpFrameReferenceFinder(this));
}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {
  if (nack_module_) {
    process_thread_->DeRegisterModule(nack_module_.get());
  }

  process_thread_->DeRegisterModule(rtp_rtcp_.get());

  packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  UpdateHistograms();
}

bool RtpVideoStreamReceiver::AddReceiveCodec(
    const VideoCodec& video_codec,
    const std::map<std::string, std::string>& codec_params) {
  pt_codec_params_.insert(make_pair(video_codec.plType, codec_params));
  return AddReceiveCodec(video_codec);
}

bool RtpVideoStreamReceiver::AddReceiveCodec(const VideoCodec& video_codec) {
  int8_t old_pltype = -1;
  if (rtp_payload_registry_.ReceivePayloadType(video_codec, &old_pltype) !=
      -1) {
    rtp_payload_registry_.DeRegisterReceivePayload(old_pltype);
  }
  return rtp_payload_registry_.RegisterReceivePayload(video_codec) == 0;
}

uint32_t RtpVideoStreamReceiver::GetRemoteSsrc() const {
  return config_.rtp.remote_ssrc;
}

int RtpVideoStreamReceiver::GetCsrcs(uint32_t* csrcs) const {
  return rtp_receiver_->CSRCs(csrcs);
}

RtpReceiver* RtpVideoStreamReceiver::GetRtpReceiver() const {
  return rtp_receiver_.get();
}

int32_t RtpVideoStreamReceiver::OnReceivedPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const WebRtcRTPHeader* rtp_header) {
  WebRtcRTPHeader rtp_header_with_ntp = *rtp_header;
  rtp_header_with_ntp.ntp_time_ms =
      ntp_estimator_.Estimate(rtp_header->header.timestamp);
  VCMPacket packet(payload_data, payload_size, rtp_header_with_ntp);
  packet.timesNacked =
      nack_module_ ? nack_module_->OnReceivedPacket(packet) : -1;
  packet.receive_time_ms = clock_->TimeInMilliseconds();

  // In the case of a video stream without picture ids and no rtx the
  // RtpFrameReferenceFinder will need to know about padding to
  // correctly calculate frame references.
  if (packet.sizeBytes == 0) {
    reference_finder_->PaddingReceived(packet.seqNum);
    packet_buffer_->PaddingReceived(packet.seqNum);
    return 0;
  }

  if (packet.codec == kVideoCodecH264) {
    // Only when we start to receive packets will we know what payload type
    // that will be used. When we know the payload type insert the correct
    // sps/pps into the tracker.
    if (packet.payloadType != last_payload_type_) {
      last_payload_type_ = packet.payloadType;
      InsertSpsPpsIntoTracker(packet.payloadType);
    }

    switch (tracker_.CopyAndFixBitstream(&packet)) {
      case video_coding::H264SpsPpsTracker::kRequestKeyframe:
        keyframe_request_sender_->RequestKeyFrame();
        FALLTHROUGH();
      case video_coding::H264SpsPpsTracker::kDrop:
        return 0;
      case video_coding::H264SpsPpsTracker::kInsert:
        break;
    }

  } else {
    uint8_t* data = new uint8_t[packet.sizeBytes];
    memcpy(data, packet.dataPtr, packet.sizeBytes);
    packet.dataPtr = data;
  }

  packet_buffer_->InsertPacket(&packet);
  return 0;
}

// TODO(nisse): Try to delete this method. Obstacles: It is used by
// ParseAndHandleEncapsulatingHeader, for handling Rtx packets, and
// for callbacks from |ulpfec_receiver_|.
void RtpVideoStreamReceiver::OnRecoveredPacket(const uint8_t* rtp_packet,
                                               size_t rtp_packet_length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(rtp_packet, rtp_packet_length, &header)) {
    return;
  }
  header.payload_type_frequency = kVideoPayloadTypeFrequency;
  bool in_order = IsPacketInOrder(header);
  ReceivePacket(rtp_packet, rtp_packet_length, header, in_order);
}

// TODO(pbos): Remove as soon as audio can handle a changing payload type
// without this callback.
int32_t RtpVideoStreamReceiver::OnInitializeDecoder(
    const int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const size_t channels,
    const uint32_t rate) {
  RTC_NOTREACHED();
  return 0;
}

// This method handles both regular RTP packets and packets recovered
// via FlexFEC.
void RtpVideoStreamReceiver::OnRtpPacket(const RtpPacketReceived& packet) {
  {
    rtc::CritScope lock(&receive_cs_);
    if (!receiving_) {
      return;
    }

    if (!packet.recovered()) {
      int64_t now_ms = clock_->TimeInMilliseconds();

      // Periodically log the RTP header of incoming packets.
      if (now_ms - last_packet_log_ms_ > kPacketLogIntervalMs) {
        std::stringstream ss;
        ss << "Packet received on SSRC: " << packet.Ssrc()
           << " with payload type: " << static_cast<int>(packet.PayloadType())
           << ", timestamp: " << packet.Timestamp()
           << ", sequence number: " << packet.SequenceNumber()
           << ", arrival time: " << packet.arrival_time_ms();
        int32_t time_offset;
        if (packet.GetExtension<TransmissionOffset>(&time_offset)) {
          ss << ", toffset: " << time_offset;
        }
        uint32_t send_time;
        if (packet.GetExtension<AbsoluteSendTime>(&send_time)) {
          ss << ", abs send time: " << send_time;
        }
        LOG(LS_INFO) << ss.str();
        last_packet_log_ms_ = now_ms;
      }
    }
  }

  // TODO(nisse): Delete use of GetHeader, but needs refactoring of
  // ReceivePacket and IncomingPacket methods below.
  RTPHeader header;
  packet.GetHeader(&header);

  header.payload_type_frequency = kVideoPayloadTypeFrequency;

  bool in_order = IsPacketInOrder(header);
  if (!packet.recovered()) {
    // TODO(nisse): Why isn't this done for recovered packets?
    rtp_payload_registry_.SetIncomingPayloadType(header);
  }
  ReceivePacket(packet.data(), packet.size(), header, in_order);
  // Update receive statistics after ReceivePacket.
  // Receive statistics will be reset if the payload type changes (make sure
  // that the first packet is included in the stats).
  if (!packet.recovered()) {
    // TODO(nisse): We should pass a recovered flag to stats, to aid
    // fixing bug bugs.webrtc.org/6339.
    rtp_receive_statistics_->IncomingPacket(
        header, packet.size(), IsPacketRetransmitted(header, in_order));
  }
}

int32_t RtpVideoStreamReceiver::RequestKeyFrame() {
  return rtp_rtcp_->RequestKeyFrame();
}

bool RtpVideoStreamReceiver::IsUlpfecEnabled() const {
  return config_.rtp.ulpfec.ulpfec_payload_type != -1;
}

bool RtpVideoStreamReceiver::IsRedEnabled() const {
  return config_.rtp.ulpfec.red_payload_type != -1;
}

bool RtpVideoStreamReceiver::IsRetransmissionsEnabled() const {
  return config_.rtp.nack.rtp_history_ms > 0;
}

void RtpVideoStreamReceiver::RequestPacketRetransmit(
    const std::vector<uint16_t>& sequence_numbers) {
  rtp_rtcp_->SendNack(sequence_numbers);
}

int32_t RtpVideoStreamReceiver::ResendPackets(const uint16_t* sequence_numbers,
                                              uint16_t length) {
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}

void RtpVideoStreamReceiver::OnReceivedFrame(
    std::unique_ptr<video_coding::RtpFrameObject> frame) {
  if (!has_received_frame_) {
    has_received_frame_ = true;
    if (frame->FrameType() != kVideoFrameKey)
      keyframe_request_sender_->RequestKeyFrame();
  }

  if (!frame->delayed_by_retransmission())
    timing_->IncomingTimestamp(frame->timestamp, clock_->TimeInMilliseconds());
  reference_finder_->ManageFrame(std::move(frame));
}

void RtpVideoStreamReceiver::OnCompleteFrame(
    std::unique_ptr<video_coding::FrameObject> frame) {
  {
    rtc::CritScope lock(&last_seq_num_cs_);
    video_coding::RtpFrameObject* rtp_frame =
        static_cast<video_coding::RtpFrameObject*>(frame.get());
    last_seq_num_for_pic_id_[rtp_frame->picture_id] = rtp_frame->last_seq_num();
  }
  complete_frame_callback_->OnCompleteFrame(std::move(frame));
}

void RtpVideoStreamReceiver::OnRttUpdate(int64_t avg_rtt_ms,
                                         int64_t max_rtt_ms) {
  if (nack_module_)
    nack_module_->UpdateRtt(max_rtt_ms);
}

rtc::Optional<int64_t> RtpVideoStreamReceiver::LastReceivedPacketMs() const {
  return packet_buffer_->LastReceivedPacketMs();
}

rtc::Optional<int64_t> RtpVideoStreamReceiver::LastReceivedKeyframePacketMs()
    const {
  return packet_buffer_->LastReceivedKeyframePacketMs();
}

void RtpVideoStreamReceiver::ReceivePacket(const uint8_t* packet,
                                           size_t packet_length,
                                           const RTPHeader& header,
                                           bool in_order) {
  if (rtp_payload_registry_.IsEncapsulated(header)) {
    ParseAndHandleEncapsulatingHeader(packet, packet_length, header);
    return;
  }
  const uint8_t* payload = packet + header.headerLength;
  assert(packet_length >= header.headerLength);
  size_t payload_length = packet_length - header.headerLength;
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_.GetPayloadSpecifics(header.payloadType,
                                                 &payload_specific)) {
    return;
  }
  rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                   payload_specific, in_order);
}

void RtpVideoStreamReceiver::ParseAndHandleEncapsulatingHeader(
    const uint8_t* packet, size_t packet_length, const RTPHeader& header) {
  if (rtp_payload_registry_.IsRed(header)) {
    int8_t ulpfec_pt = rtp_payload_registry_.ulpfec_payload_type();
    if (packet[header.headerLength] == ulpfec_pt) {
      rtp_receive_statistics_->FecPacketReceived(header, packet_length);
      // Notify video_receiver about received FEC packets to avoid NACKing these
      // packets.
      NotifyReceiverOfFecPacket(header);
    }
    if (ulpfec_receiver_->AddReceivedRedPacket(header, packet, packet_length,
                                               ulpfec_pt) != 0) {
      return;
    }
    ulpfec_receiver_->ProcessReceivedFec();
  } else if (rtp_payload_registry_.IsRtx(header)) {
    if (header.headerLength + header.paddingLength == packet_length) {
      // This is an empty packet and should be silently dropped before trying to
      // parse the RTX header.
      return;
    }
    // Remove the RTX header and parse the original RTP header.
    if (packet_length < header.headerLength)
      return;
    if (packet_length > sizeof(restored_packet_))
      return;
    rtc::CritScope lock(&receive_cs_);
    if (restored_packet_in_use_) {
      LOG(LS_WARNING) << "Multiple RTX headers detected, dropping packet.";
      return;
    }
    if (!rtp_payload_registry_.RestoreOriginalPacket(
            restored_packet_, packet, &packet_length, config_.rtp.remote_ssrc,
            header)) {
      LOG(LS_WARNING) << "Incoming RTX packet: Invalid RTP header ssrc: "
                      << header.ssrc << " payload type: "
                      << static_cast<int>(header.payloadType);
      return;
    }
    restored_packet_in_use_ = true;
    OnRecoveredPacket(restored_packet_, packet_length);
    restored_packet_in_use_ = false;
  }
}

void RtpVideoStreamReceiver::NotifyReceiverOfFecPacket(
    const RTPHeader& header) {
  int8_t last_media_payload_type =
      rtp_payload_registry_.last_received_media_payload_type();
  if (last_media_payload_type < 0) {
    LOG(LS_WARNING) << "Failed to get last media payload type.";
    return;
  }
  // Fake an empty media packet.
  WebRtcRTPHeader rtp_header = {};
  rtp_header.header = header;
  rtp_header.header.payloadType = last_media_payload_type;
  rtp_header.header.paddingLength = 0;
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_.GetPayloadSpecifics(last_media_payload_type,
                                                 &payload_specific)) {
    LOG(LS_WARNING) << "Failed to get payload specifics.";
    return;
  }
  rtp_header.type.Video.codec = payload_specific.Video.videoCodecType;
  rtp_header.type.Video.rotation = kVideoRotation_0;
  if (header.extension.hasVideoRotation) {
    rtp_header.type.Video.rotation = header.extension.videoRotation;
  }
  rtp_header.type.Video.content_type = VideoContentType::UNSPECIFIED;
  if (header.extension.hasVideoContentType) {
    rtp_header.type.Video.content_type = header.extension.videoContentType;
  }
  rtp_header.type.Video.video_timing = {0u, 0u, 0u, 0u, 0u, 0u, false};
  if (header.extension.has_video_timing) {
    rtp_header.type.Video.video_timing = header.extension.video_timing;
    rtp_header.type.Video.video_timing.is_timing_frame = true;
  }
  rtp_header.type.Video.playout_delay = header.extension.playout_delay;

  OnReceivedPayloadData(nullptr, 0, &rtp_header);
}

bool RtpVideoStreamReceiver::DeliverRtcp(const uint8_t* rtcp_packet,
                                         size_t rtcp_packet_length) {
  {
    rtc::CritScope lock(&receive_cs_);
    if (!receiving_) {
      return false;
    }
  }

  rtp_rtcp_->IncomingRtcpPacket(rtcp_packet, rtcp_packet_length);

  int64_t rtt = 0;
  rtp_rtcp_->RTT(rtp_receiver_->SSRC(), &rtt, nullptr, nullptr, nullptr);
  if (rtt == 0) {
    // Waiting for valid rtt.
    return true;
  }
  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (rtp_rtcp_->RemoteNTP(&ntp_secs, &ntp_frac, nullptr, nullptr,
                           &rtp_timestamp) != 0) {
    // Waiting for RTCP.
    return true;
  }
  ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);

  return true;
}

void RtpVideoStreamReceiver::FrameContinuous(uint16_t picture_id) {
  if (!nack_module_)
    return;

  int seq_num = -1;
  {
    rtc::CritScope lock(&last_seq_num_cs_);
    auto seq_num_it = last_seq_num_for_pic_id_.find(picture_id);
    if (seq_num_it != last_seq_num_for_pic_id_.end())
      seq_num = seq_num_it->second;
  }
  if (seq_num != -1)
    nack_module_->ClearUpTo(seq_num);
}

void RtpVideoStreamReceiver::FrameDecoded(uint16_t picture_id) {
  int seq_num = -1;
  {
    rtc::CritScope lock(&last_seq_num_cs_);
    auto seq_num_it = last_seq_num_for_pic_id_.find(picture_id);
    if (seq_num_it != last_seq_num_for_pic_id_.end()) {
      seq_num = seq_num_it->second;
      last_seq_num_for_pic_id_.erase(last_seq_num_for_pic_id_.begin(),
                                     ++seq_num_it);
    }
  }
  if (seq_num != -1) {
    packet_buffer_->ClearTo(seq_num);
    reference_finder_->ClearTo(seq_num);
  }
}

void RtpVideoStreamReceiver::SignalNetworkState(NetworkState state) {
  rtp_rtcp_->SetRTCPStatus(state == kNetworkUp ? config_.rtp.rtcp_mode
                                               : RtcpMode::kOff);
}

void RtpVideoStreamReceiver::StartReceive() {
  rtc::CritScope lock(&receive_cs_);
  receiving_ = true;
}

void RtpVideoStreamReceiver::StopReceive() {
  rtc::CritScope lock(&receive_cs_);
  receiving_ = false;
}

bool RtpVideoStreamReceiver::IsPacketInOrder(const RTPHeader& header) const {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  return statistician->IsPacketInOrder(header.sequenceNumber);
}

bool RtpVideoStreamReceiver::IsPacketRetransmitted(const RTPHeader& header,
                                                   bool in_order) const {
  // Retransmissions are handled separately if RTX is enabled.
  if (rtp_payload_registry_.RtxEnabled())
    return false;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  // Check if this is a retransmission.
  int64_t min_rtt = 0;
  rtp_rtcp_->RTT(config_.rtp.remote_ssrc, nullptr, nullptr, &min_rtt, nullptr);
  return !in_order &&
      statistician->IsRetransmitOfOldPacket(header, min_rtt);
}

void RtpVideoStreamReceiver::UpdateHistograms() {
  FecPacketCounter counter = ulpfec_receiver_->GetPacketCounter();
  if (counter.first_packet_time_ms == -1)
    return;

  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - counter.first_packet_time_ms) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds)
    return;

  if (counter.num_packets > 0) {
    RTC_HISTOGRAM_PERCENTAGE(
        "WebRTC.Video.ReceivedFecPacketsInPercent",
        static_cast<int>(counter.num_fec_packets * 100 / counter.num_packets));
  }
  if (counter.num_fec_packets > 0) {
    RTC_HISTOGRAM_PERCENTAGE("WebRTC.Video.RecoveredMediaPacketsInPercentOfFec",
                             static_cast<int>(counter.num_recovered_packets *
                                              100 / counter.num_fec_packets));
  }
}

void RtpVideoStreamReceiver::EnableReceiveRtpHeaderExtension(
    const std::string& extension, int id) {
  // One-byte-extension local identifiers are in the range 1-14 inclusive.
  RTC_DCHECK_GE(id, 1);
  RTC_DCHECK_LE(id, 14);
  RTC_DCHECK(RtpExtension::IsSupportedForVideo(extension));
  RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
      StringToRtpExtensionType(extension), id));
}

void RtpVideoStreamReceiver::InsertSpsPpsIntoTracker(uint8_t payload_type) {
  auto codec_params_it = pt_codec_params_.find(payload_type);
  if (codec_params_it == pt_codec_params_.end())
    return;

  LOG(LS_INFO) << "Found out of band supplied codec parameters for"
               << " payload type: " << static_cast<int>(payload_type);

  H264SpropParameterSets sprop_decoder;
  auto sprop_base64_it =
      codec_params_it->second.find(cricket::kH264FmtpSpropParameterSets);

  if (sprop_base64_it == codec_params_it->second.end())
    return;

  if (!sprop_decoder.DecodeSprop(sprop_base64_it->second.c_str()))
    return;

  tracker_.InsertSpsPpsNalus(sprop_decoder.sps_nalu(),
                             sprop_decoder.pps_nalu());
}

}  // namespace webrtc
