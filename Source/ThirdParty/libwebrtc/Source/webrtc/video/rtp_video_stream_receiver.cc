/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"

#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#include "modules/pacing/packet_router.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/ulpfec_receiver.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/nack_module.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/fallthrough.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "video/receive_statistics_proxy.h"

namespace webrtc {

namespace {
// TODO(philipel): Change kPacketBufferStartSize back to 32 in M63 see:
//                 crbug.com/752886
constexpr int kPacketBufferStartSize = 512;
constexpr int kPacketBufferMaxSixe = 2048;
}  // namespace

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
    ReceiveStatistics* rtp_receive_statistics,
    ReceiveStatisticsProxy* receive_stats_proxy,
    ProcessThread* process_thread,
    NackSender* nack_sender,
    KeyFrameRequestSender* keyframe_request_sender,
    video_coding::OnCompleteFrameCallback* complete_frame_callback)
    : clock_(Clock::GetRealTimeClock()),
      config_(*config),
      packet_router_(packet_router),
      process_thread_(process_thread),
      ntp_estimator_(clock_),
      rtp_header_extensions_(config_.rtp.extensions),
      rtp_receive_statistics_(rtp_receive_statistics),
      ulpfec_receiver_(UlpfecReceiver::Create(config->rtp.remote_ssrc, this)),
      receiving_(false),
      last_packet_log_ms_(-1),
      rtp_rtcp_(CreateRtpRtcpModule(rtp_receive_statistics_,
                                    transport,
                                    rtt_stats,
                                    receive_stats_proxy,
                                    packet_router)),
      complete_frame_callback_(complete_frame_callback),
      keyframe_request_sender_(keyframe_request_sender),
      has_received_frame_(false) {
  constexpr bool remb_candidate = true;
  packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(), remb_candidate);
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

  static const int kMaxPacketAgeToNack = 450;
  const int max_reordering_threshold = (config_.rtp.nack.rtp_history_ms > 0)
                                           ? kMaxPacketAgeToNack
                                           : kDefaultMaxReorderingThreshold;
  rtp_receive_statistics_->SetMaxReorderingThreshold(max_reordering_threshold);

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
  RTC_DCHECK(secondary_sinks_.empty());

  if (nack_module_) {
    process_thread_->DeRegisterModule(nack_module_.get());
  }

  process_thread_->DeRegisterModule(rtp_rtcp_.get());

  packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  UpdateHistograms();
}

void RtpVideoStreamReceiver::AddReceiveCodec(
    const VideoCodec& video_codec,
    const std::map<std::string, std::string>& codec_params) {
  pt_codec_type_.emplace(video_codec.plType, video_codec.codecType);
  pt_codec_params_.emplace(video_codec.plType, codec_params);
}

absl::optional<Syncable::Info> RtpVideoStreamReceiver::GetSyncInfo() const {
  Syncable::Info info;
  if (rtp_rtcp_->RemoteNTP(&info.capture_time_ntp_secs,
                           &info.capture_time_ntp_frac, nullptr, nullptr,
                           &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }
  {
    rtc::CritScope lock(&rtp_sources_lock_);
    if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
      return absl::nullopt;
    }
    info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
    info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;
  }

  // Leaves info.current_delay_ms uninitialized.
  return info;
}

int32_t RtpVideoStreamReceiver::OnReceivedPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const WebRtcRTPHeader* rtp_header) {
  return OnReceivedPayloadData(payload_data, payload_size, rtp_header,
                               absl::nullopt);
}

int32_t RtpVideoStreamReceiver::OnReceivedPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const WebRtcRTPHeader* rtp_header,
    const absl::optional<RtpGenericFrameDescriptor>& generic_descriptor) {
  WebRtcRTPHeader rtp_header_with_ntp = *rtp_header;
  rtp_header_with_ntp.ntp_time_ms =
      ntp_estimator_.Estimate(rtp_header->header.timestamp);

  VCMPacket packet(payload_data, payload_size, rtp_header_with_ntp);
  if (nack_module_) {
    const bool is_keyframe =
        rtp_header->video_header().is_first_packet_in_frame &&
        rtp_header->frameType == kVideoFrameKey;

    packet.timesNacked = nack_module_->OnReceivedPacket(
        rtp_header->header.sequenceNumber, is_keyframe);
  } else {
    packet.timesNacked = -1;
  }
  packet.receive_time_ms = clock_->TimeInMilliseconds();

  if (packet.sizeBytes == 0) {
    NotifyReceiverOfEmptyPacket(packet.seqNum);
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
        RTC_FALLTHROUGH();
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

  packet.generic_descriptor = generic_descriptor;

  packet_buffer_->InsertPacket(&packet);
  return 0;
}

void RtpVideoStreamReceiver::OnRecoveredPacket(const uint8_t* rtp_packet,
                                               size_t rtp_packet_length) {
  RtpPacketReceived packet;
  if (!packet.Parse(rtp_packet, rtp_packet_length))
    return;
  if (packet.PayloadType() == config_.rtp.red_payload_type) {
    RTC_LOG(LS_WARNING) << "Discarding recovered packet with RED encapsulation";
    return;
  }

  packet.IdentifyExtensions(rtp_header_extensions_);
  packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);
  // TODO(nisse): UlpfecReceiverImpl::ProcessReceivedFec passes both
  // original (decapsulated) media packets and recovered packets to
  // this callback. We need a way to distinguish, for setting
  // packet.recovered() correctly. Ideally, move RED decapsulation out
  // of the Ulpfec implementation.

  ReceivePacket(packet);
}

// This method handles both regular RTP packets and packets recovered
// via FlexFEC.
void RtpVideoStreamReceiver::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);

  if (!receiving_) {
    return;
  }

  if (!packet.recovered()) {
    // TODO(nisse): Exclude out-of-order packets?
    int64_t now_ms = clock_->TimeInMilliseconds();
    {
      rtc::CritScope cs(&rtp_sources_lock_);
      last_received_rtp_timestamp_ = packet.Timestamp();
      last_received_rtp_system_time_ms_ = now_ms;

      std::vector<uint32_t> csrcs = packet.Csrcs();
      contributing_sources_.Update(now_ms, csrcs);
    }
    // Periodically log the RTP header of incoming packets.
    if (now_ms - last_packet_log_ms_ > kPacketLogIntervalMs) {
      rtc::StringBuilder ss;
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
      RTC_LOG(LS_INFO) << ss.str();
      last_packet_log_ms_ = now_ms;
    }
  }

  ReceivePacket(packet);

  // Update receive statistics after ReceivePacket.
  // Receive statistics will be reset if the payload type changes (make sure
  // that the first packet is included in the stats).
  if (!packet.recovered()) {
    rtp_receive_statistics_->OnRtpPacket(packet);
  }

  for (RtpPacketSinkInterface* secondary_sink : secondary_sinks_) {
    secondary_sink->OnRtpPacket(packet);
  }
}

int32_t RtpVideoStreamReceiver::RequestKeyFrame() {
  return rtp_rtcp_->RequestKeyFrame();
}

bool RtpVideoStreamReceiver::IsUlpfecEnabled() const {
  return config_.rtp.ulpfec_payload_type != -1;
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

  reference_finder_->ManageFrame(std::move(frame));
}

void RtpVideoStreamReceiver::OnCompleteFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  {
    rtc::CritScope lock(&last_seq_num_cs_);
    video_coding::RtpFrameObject* rtp_frame =
        static_cast<video_coding::RtpFrameObject*>(frame.get());
    last_seq_num_for_pic_id_[rtp_frame->id.picture_id] =
        rtp_frame->last_seq_num();
  }
  complete_frame_callback_->OnCompleteFrame(std::move(frame));
}

void RtpVideoStreamReceiver::UpdateRtt(int64_t max_rtt_ms) {
  if (nack_module_)
    nack_module_->UpdateRtt(max_rtt_ms);
}

absl::optional<int64_t> RtpVideoStreamReceiver::LastReceivedPacketMs() const {
  return packet_buffer_->LastReceivedPacketMs();
}

absl::optional<int64_t> RtpVideoStreamReceiver::LastReceivedKeyframePacketMs()
    const {
  return packet_buffer_->LastReceivedKeyframePacketMs();
}

void RtpVideoStreamReceiver::AddSecondarySink(RtpPacketSinkInterface* sink) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);
  RTC_DCHECK(std::find(secondary_sinks_.cbegin(), secondary_sinks_.cend(),
                       sink) == secondary_sinks_.cend());
  secondary_sinks_.push_back(sink);
}

void RtpVideoStreamReceiver::RemoveSecondarySink(
    const RtpPacketSinkInterface* sink) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);
  auto it = std::find(secondary_sinks_.begin(), secondary_sinks_.end(), sink);
  if (it == secondary_sinks_.end()) {
    // We might be rolling-back a call whose setup failed mid-way. In such a
    // case, it's simpler to remove "everything" rather than remember what
    // has already been added.
    RTC_LOG(LS_WARNING) << "Removal of unknown sink.";
    return;
  }
  secondary_sinks_.erase(it);
}

void RtpVideoStreamReceiver::ReceivePacket(const RtpPacketReceived& packet) {
  if (packet.payload_size() == 0) {
    // Padding or keep-alive packet.
    // TODO(nisse): Could drop empty packets earlier, but need to figure out how
    // they should be counted in stats.
    NotifyReceiverOfEmptyPacket(packet.SequenceNumber());
    return;
  }
  if (packet.PayloadType() == config_.rtp.red_payload_type) {
    ParseAndHandleEncapsulatingHeader(packet);
    return;
  }

  const auto codec_type_it = pt_codec_type_.find(packet.PayloadType());
  if (codec_type_it == pt_codec_type_.end()) {
    return;
  }
  auto depacketizer =
      absl::WrapUnique(RtpDepacketizer::Create(codec_type_it->second));

  if (!depacketizer) {
    RTC_LOG(LS_ERROR) << "Failed to create depacketizer.";
    return;
  }
  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!depacketizer->Parse(&parsed_payload, packet.payload().data(),
                           packet.payload().size())) {
    RTC_LOG(LS_WARNING) << "Failed parsing payload.";
    return;
  }

  WebRtcRTPHeader webrtc_rtp_header = {};
  packet.GetHeader(&webrtc_rtp_header.header);

  webrtc_rtp_header.frameType = parsed_payload.frame_type;
  webrtc_rtp_header.video_header() = parsed_payload.video_header();
  webrtc_rtp_header.video_header().rotation = kVideoRotation_0;
  webrtc_rtp_header.video_header().content_type = VideoContentType::UNSPECIFIED;
  webrtc_rtp_header.video_header().video_timing.flags =
      VideoSendTiming::kInvalid;
  webrtc_rtp_header.video_header().playout_delay.min_ms = -1;
  webrtc_rtp_header.video_header().playout_delay.max_ms = -1;
  webrtc_rtp_header.video_header().is_last_packet_in_frame =
      webrtc_rtp_header.header.markerBit;

  packet.GetExtension<VideoOrientation>(
      &webrtc_rtp_header.video_header().rotation);
  packet.GetExtension<VideoContentTypeExtension>(
      &webrtc_rtp_header.video_header().content_type);
  packet.GetExtension<VideoTimingExtension>(
      &webrtc_rtp_header.video_header().video_timing);
  packet.GetExtension<PlayoutDelayLimits>(
      &webrtc_rtp_header.video_header().playout_delay);

  absl::optional<RtpGenericFrameDescriptor> generic_descriptor_wire;
  generic_descriptor_wire.emplace();
  if (packet.GetExtension<RtpGenericFrameDescriptorExtension>(
          &generic_descriptor_wire.value())) {
    generic_descriptor_wire->SetByteRepresentation(
        packet.GetRawExtension<RtpGenericFrameDescriptorExtension>());
    webrtc_rtp_header.video_header().is_first_packet_in_frame =
        generic_descriptor_wire->FirstSubFrameInFrame() &&
        generic_descriptor_wire->FirstPacketInSubFrame();
    webrtc_rtp_header.video_header().is_last_packet_in_frame =
        webrtc_rtp_header.header.markerBit ||
        (generic_descriptor_wire->LastSubFrameInFrame() &&
         generic_descriptor_wire->LastPacketInSubFrame());
  } else {
    generic_descriptor_wire.reset();
  }

  OnReceivedPayloadData(parsed_payload.payload, parsed_payload.payload_length,
                        &webrtc_rtp_header, generic_descriptor_wire);
}

void RtpVideoStreamReceiver::ParseAndHandleEncapsulatingHeader(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);
  if (packet.PayloadType() == config_.rtp.red_payload_type &&
      packet.payload_size() > 0) {
    if (packet.payload()[0] == config_.rtp.ulpfec_payload_type) {
      rtp_receive_statistics_->FecPacketReceived(packet);
      // Notify video_receiver about received FEC packets to avoid NACKing these
      // packets.
      NotifyReceiverOfEmptyPacket(packet.SequenceNumber());
    }
    RTPHeader header;
    packet.GetHeader(&header);
    if (ulpfec_receiver_->AddReceivedRedPacket(
            header, packet.data(), packet.size(),
            config_.rtp.ulpfec_payload_type) != 0) {
      return;
    }
    ulpfec_receiver_->ProcessReceivedFec();
  }
}

// In the case of a video stream without picture ids and no rtx the
// RtpFrameReferenceFinder will need to know about padding to
// correctly calculate frame references.
void RtpVideoStreamReceiver::NotifyReceiverOfEmptyPacket(uint16_t seq_num) {
  reference_finder_->PaddingReceived(seq_num);
  packet_buffer_->PaddingReceived(seq_num);
  if (nack_module_) {
    nack_module_->OnReceivedPacket(seq_num, /* is_keyframe = */ false);
  }
}

bool RtpVideoStreamReceiver::DeliverRtcp(const uint8_t* rtcp_packet,
                                         size_t rtcp_packet_length) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);

  if (!receiving_) {
    return false;
  }

  rtp_rtcp_->IncomingRtcpPacket(rtcp_packet, rtcp_packet_length);

  int64_t rtt = 0;
  rtp_rtcp_->RTT(config_.rtp.remote_ssrc, &rtt, nullptr, nullptr, nullptr);
  if (rtt == 0) {
    // Waiting for valid rtt.
    return true;
  }
  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  uint32_t recieved_ntp_secs = 0;
  uint32_t recieved_ntp_frac = 0;
  if (rtp_rtcp_->RemoteNTP(&ntp_secs, &ntp_frac, &recieved_ntp_secs,
                           &recieved_ntp_frac, &rtp_timestamp) != 0) {
    // Waiting for RTCP.
    return true;
  }
  NtpTime recieved_ntp(recieved_ntp_secs, recieved_ntp_frac);
  int64_t time_since_recieved =
      clock_->CurrentNtpInMilliseconds() - recieved_ntp.ToMs();
  // Don't use old SRs to estimate time.
  if (time_since_recieved <= 1) {
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }

  return true;
}

void RtpVideoStreamReceiver::FrameContinuous(int64_t picture_id) {
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

void RtpVideoStreamReceiver::FrameDecoded(int64_t picture_id) {
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

int RtpVideoStreamReceiver::GetUniqueFramesSeen() const {
  return packet_buffer_->GetUniqueFramesSeen();
}

void RtpVideoStreamReceiver::StartReceive() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);
  receiving_ = true;
}

void RtpVideoStreamReceiver::StopReceive() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&worker_task_checker_);
  receiving_ = false;
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

void RtpVideoStreamReceiver::InsertSpsPpsIntoTracker(uint8_t payload_type) {
  auto codec_params_it = pt_codec_params_.find(payload_type);
  if (codec_params_it == pt_codec_params_.end())
    return;

  RTC_LOG(LS_INFO) << "Found out of band supplied codec parameters for"
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

std::vector<webrtc::RtpSource> RtpVideoStreamReceiver::GetSources() const {
  int64_t now_ms = rtc::TimeMillis();
  std::vector<RtpSource> sources;
  {
    rtc::CritScope cs(&rtp_sources_lock_);
    sources = contributing_sources_.GetSources(now_ms);
    if (last_received_rtp_system_time_ms_ >=
        now_ms - ContributingSources::kHistoryMs) {
      sources.emplace_back(*last_received_rtp_system_time_ms_,
                           config_.rtp.remote_ssrc, RtpSourceType::SSRC);
    }
  }
  return sources;
}

}  // namespace webrtc
