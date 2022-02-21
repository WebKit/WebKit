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
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "media/base/media_constants.h"
#include "modules/pacing/packet_router.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/ulpfec_receiver.h"
#include "modules/rtp_rtcp/source/create_video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_raw.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/deprecated/nack_module.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/ntp_time.h"
#include "video/receive_statistics_proxy.h"

namespace webrtc {

namespace {
// TODO(philipel): Change kPacketBufferStartSize back to 32 in M63 see:
//                 crbug.com/752886
constexpr int kPacketBufferStartSize = 512;
constexpr int kPacketBufferMaxSize = 2048;

int PacketBufferMaxSize() {
  // The group here must be a positive power of 2, in which case that is used as
  // size. All other values shall result in the default value being used.
  const std::string group_name =
      webrtc::field_trial::FindFullName("WebRTC-PacketBufferMaxSize");
  int packet_buffer_max_size = kPacketBufferMaxSize;
  if (!group_name.empty() &&
      (sscanf(group_name.c_str(), "%d", &packet_buffer_max_size) != 1 ||
       packet_buffer_max_size <= 0 ||
       // Verify that the number is a positive power of 2.
       (packet_buffer_max_size & (packet_buffer_max_size - 1)) != 0)) {
    RTC_LOG(LS_WARNING) << "Invalid packet buffer max size: " << group_name;
    packet_buffer_max_size = kPacketBufferMaxSize;
  }
  return packet_buffer_max_size;
}

std::unique_ptr<RtpRtcp> CreateRtpRtcpModule(
    Clock* clock,
    ReceiveStatistics* receive_statistics,
    Transport* outgoing_transport,
    RtcpRttStats* rtt_stats,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RtcpCnameCallback* rtcp_cname_callback,
    bool non_sender_rtt_measurement,
    uint32_t local_ssrc) {
  RtpRtcpInterface::Configuration configuration;
  configuration.clock = clock;
  configuration.audio = false;
  configuration.receiver_only = true;
  configuration.receive_statistics = receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer =
      rtcp_packet_type_counter_observer;
  configuration.rtcp_cname_callback = rtcp_cname_callback;
  configuration.local_media_ssrc = local_ssrc;
  configuration.non_sender_rtt_measurement = non_sender_rtt_measurement;

  std::unique_ptr<RtpRtcp> rtp_rtcp = RtpRtcp::DEPRECATED_Create(configuration);
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);

  return rtp_rtcp;
}

static const int kPacketLogIntervalMs = 10000;

}  // namespace

RtpVideoStreamReceiver::RtcpFeedbackBuffer::RtcpFeedbackBuffer(
    KeyFrameRequestSender* key_frame_request_sender,
    NackSender* nack_sender,
    LossNotificationSender* loss_notification_sender)
    : key_frame_request_sender_(key_frame_request_sender),
      nack_sender_(nack_sender),
      loss_notification_sender_(loss_notification_sender),
      request_key_frame_(false) {
  RTC_DCHECK(key_frame_request_sender_);
  RTC_DCHECK(nack_sender_);
  RTC_DCHECK(loss_notification_sender_);
}

void RtpVideoStreamReceiver::RtcpFeedbackBuffer::RequestKeyFrame() {
  MutexLock lock(&mutex_);
  request_key_frame_ = true;
}

void RtpVideoStreamReceiver::RtcpFeedbackBuffer::SendNack(
    const std::vector<uint16_t>& sequence_numbers,
    bool buffering_allowed) {
  RTC_DCHECK(!sequence_numbers.empty());
  MutexLock lock(&mutex_);
  nack_sequence_numbers_.insert(nack_sequence_numbers_.end(),
                                sequence_numbers.cbegin(),
                                sequence_numbers.cend());
  if (!buffering_allowed) {
    // Note that while *buffering* is not allowed, *batching* is, meaning that
    // previously buffered messages may be sent along with the current message.
    SendRtcpFeedback(ConsumeRtcpFeedbackLocked());
  }
}

void RtpVideoStreamReceiver::RtcpFeedbackBuffer::SendLossNotification(
    uint16_t last_decoded_seq_num,
    uint16_t last_received_seq_num,
    bool decodability_flag,
    bool buffering_allowed) {
  RTC_DCHECK(buffering_allowed);
  MutexLock lock(&mutex_);
  RTC_DCHECK(!lntf_state_)
      << "SendLossNotification() called twice in a row with no call to "
         "SendBufferedRtcpFeedback() in between.";
  lntf_state_ = absl::make_optional<LossNotificationState>(
      last_decoded_seq_num, last_received_seq_num, decodability_flag);
}

void RtpVideoStreamReceiver::RtcpFeedbackBuffer::SendBufferedRtcpFeedback() {
  SendRtcpFeedback(ConsumeRtcpFeedback());
}

RtpVideoStreamReceiver::RtcpFeedbackBuffer::ConsumedRtcpFeedback
RtpVideoStreamReceiver::RtcpFeedbackBuffer::ConsumeRtcpFeedback() {
  MutexLock lock(&mutex_);
  return ConsumeRtcpFeedbackLocked();
}

RtpVideoStreamReceiver::RtcpFeedbackBuffer::ConsumedRtcpFeedback
RtpVideoStreamReceiver::RtcpFeedbackBuffer::ConsumeRtcpFeedbackLocked() {
  ConsumedRtcpFeedback feedback;
  std::swap(feedback.request_key_frame, request_key_frame_);
  std::swap(feedback.nack_sequence_numbers, nack_sequence_numbers_);
  std::swap(feedback.lntf_state, lntf_state_);
  return feedback;
}

void RtpVideoStreamReceiver::RtcpFeedbackBuffer::SendRtcpFeedback(
    ConsumedRtcpFeedback feedback) {
  if (feedback.lntf_state) {
    // If either a NACK or a key frame request is sent, we should buffer
    // the LNTF and wait for them (NACK or key frame request) to trigger
    // the compound feedback message.
    // Otherwise, the LNTF should be sent out immediately.
    const bool buffering_allowed =
        feedback.request_key_frame || !feedback.nack_sequence_numbers.empty();

    loss_notification_sender_->SendLossNotification(
        feedback.lntf_state->last_decoded_seq_num,
        feedback.lntf_state->last_received_seq_num,
        feedback.lntf_state->decodability_flag, buffering_allowed);
  }

  if (feedback.request_key_frame) {
    key_frame_request_sender_->RequestKeyFrame();
  } else if (!feedback.nack_sequence_numbers.empty()) {
    nack_sender_->SendNack(feedback.nack_sequence_numbers, true);
  }
}

// DEPRECATED
RtpVideoStreamReceiver::RtpVideoStreamReceiver(
    Clock* clock,
    Transport* transport,
    RtcpRttStats* rtt_stats,
    PacketRouter* packet_router,
    const VideoReceiveStream::Config* config,
    ReceiveStatistics* rtp_receive_statistics,
    ReceiveStatisticsProxy* receive_stats_proxy,
    ProcessThread* process_thread,
    NackSender* nack_sender,
    KeyFrameRequestSender* keyframe_request_sender,
    OnCompleteFrameCallback* complete_frame_callback,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : RtpVideoStreamReceiver(clock,
                             transport,
                             rtt_stats,
                             packet_router,
                             config,
                             rtp_receive_statistics,
                             receive_stats_proxy,
                             receive_stats_proxy,
                             process_thread,
                             nack_sender,
                             keyframe_request_sender,
                             complete_frame_callback,
                             frame_decryptor,
                             frame_transformer) {}

RtpVideoStreamReceiver::RtpVideoStreamReceiver(
    Clock* clock,
    Transport* transport,
    RtcpRttStats* rtt_stats,
    PacketRouter* packet_router,
    const VideoReceiveStream::Config* config,
    ReceiveStatistics* rtp_receive_statistics,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RtcpCnameCallback* rtcp_cname_callback,
    ProcessThread* process_thread,
    NackSender* nack_sender,
    KeyFrameRequestSender* keyframe_request_sender,
    OnCompleteFrameCallback* complete_frame_callback,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : clock_(clock),
      config_(*config),
      packet_router_(packet_router),
      process_thread_(process_thread),
      ntp_estimator_(clock),
      rtp_header_extensions_(config_.rtp.extensions),
      forced_playout_delay_max_ms_("max_ms", absl::nullopt),
      forced_playout_delay_min_ms_("min_ms", absl::nullopt),
      rtp_receive_statistics_(rtp_receive_statistics),
      ulpfec_receiver_(UlpfecReceiver::Create(config->rtp.remote_ssrc,
                                              this,
                                              config->rtp.extensions)),
      receiving_(false),
      last_packet_log_ms_(-1),
      rtp_rtcp_(CreateRtpRtcpModule(
          clock,
          rtp_receive_statistics_,
          transport,
          rtt_stats,
          rtcp_packet_type_counter_observer,
          rtcp_cname_callback,
          config_.rtp.rtcp_xr.receiver_reference_time_report,
          config_.rtp.local_ssrc)),
      complete_frame_callback_(complete_frame_callback),
      keyframe_request_sender_(keyframe_request_sender),
      // TODO(bugs.webrtc.org/10336): Let `rtcp_feedback_buffer_` communicate
      // directly with `rtp_rtcp_`.
      rtcp_feedback_buffer_(this, nack_sender, this),
      packet_buffer_(kPacketBufferStartSize, PacketBufferMaxSize()),
      reference_finder_(std::make_unique<RtpFrameReferenceFinder>()),
      has_received_frame_(false),
      frames_decryptable_(false),
      absolute_capture_time_interpolator_(clock) {
  constexpr bool remb_candidate = true;
  if (packet_router_)
    packet_router_->AddReceiveRtpModule(rtp_rtcp_.get(), remb_candidate);

  RTC_DCHECK(config_.rtp.rtcp_mode != RtcpMode::kOff)
      << "A stream should not be configured with RTCP disabled. This value is "
         "reserved for internal usage.";
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  RTC_DCHECK(config_.rtp.local_ssrc != 0);
  RTC_DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  rtp_rtcp_->SetRTCPStatus(config_.rtp.rtcp_mode);
  rtp_rtcp_->SetRemoteSSRC(config_.rtp.remote_ssrc);

  static const int kMaxPacketAgeToNack = 450;
  const int max_reordering_threshold = (config_.rtp.nack.rtp_history_ms > 0)
                                           ? kMaxPacketAgeToNack
                                           : kDefaultMaxReorderingThreshold;
  rtp_receive_statistics_->SetMaxReorderingThreshold(config_.rtp.remote_ssrc,
                                                     max_reordering_threshold);
  // TODO(nisse): For historic reasons, we applied the above
  // max_reordering_threshold also for RTX stats, which makes little sense since
  // we don't NACK rtx packets. Consider deleting the below block, and rely on
  // the default threshold.
  if (config_.rtp.rtx_ssrc) {
    rtp_receive_statistics_->SetMaxReorderingThreshold(
        config_.rtp.rtx_ssrc, max_reordering_threshold);
  }
  ParseFieldTrial(
      {&forced_playout_delay_max_ms_, &forced_playout_delay_min_ms_},
      field_trial::FindFullName("WebRTC-ForcePlayoutDelay"));

  process_thread_->RegisterModule(rtp_rtcp_.get(), RTC_FROM_HERE);

  if (config_.rtp.lntf.enabled) {
    loss_notification_controller_ =
        std::make_unique<LossNotificationController>(&rtcp_feedback_buffer_,
                                                     &rtcp_feedback_buffer_);
  }

  if (config_.rtp.nack.rtp_history_ms != 0) {
    nack_module_ = std::make_unique<DEPRECATED_NackModule>(
        clock_, &rtcp_feedback_buffer_, &rtcp_feedback_buffer_);
    process_thread_->RegisterModule(nack_module_.get(), RTC_FROM_HERE);
  }

  // Only construct the encrypted receiver if frame encryption is enabled.
  if (config_.crypto_options.sframe.require_frame_encryption) {
    buffered_frame_decryptor_ =
        std::make_unique<BufferedFrameDecryptor>(this, this);
    if (frame_decryptor != nullptr) {
      buffered_frame_decryptor_->SetFrameDecryptor(std::move(frame_decryptor));
    }
  }

  if (frame_transformer) {
    frame_transformer_delegate_ =
        rtc::make_ref_counted<RtpVideoStreamReceiverFrameTransformerDelegate>(
            this, std::move(frame_transformer), rtc::Thread::Current(),
            config_.rtp.remote_ssrc);
    frame_transformer_delegate_->Init();
  }
}

RtpVideoStreamReceiver::~RtpVideoStreamReceiver() {
  RTC_DCHECK(secondary_sinks_.empty());

  if (nack_module_) {
    process_thread_->DeRegisterModule(nack_module_.get());
  }

  process_thread_->DeRegisterModule(rtp_rtcp_.get());

  if (packet_router_)
    packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  UpdateHistograms();
  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();
}

void RtpVideoStreamReceiver::AddReceiveCodec(
    uint8_t payload_type,
    VideoCodecType codec_type,
    const std::map<std::string, std::string>& codec_params,
    bool raw_payload) {
  if (codec_params.count(cricket::kH264FmtpSpsPpsIdrInKeyframe) ||
      field_trial::IsEnabled("WebRTC-SpsPpsIdrIsH264Keyframe")) {
    MutexLock lock(&packet_buffer_lock_);
    packet_buffer_.ForceSpsPpsIdrIsH264Keyframe();
  }
  payload_type_map_.emplace(
      payload_type, raw_payload ? std::make_unique<VideoRtpDepacketizerRaw>()
                                : CreateVideoRtpDepacketizer(codec_type));
  pt_codec_params_.emplace(payload_type, codec_params);
}

absl::optional<Syncable::Info> RtpVideoStreamReceiver::GetSyncInfo() const {
  Syncable::Info info;
  if (rtp_rtcp_->RemoteNTP(&info.capture_time_ntp_secs,
                           &info.capture_time_ntp_frac,
                           /*rtcp_arrival_time_secs=*/nullptr,
                           /*rtcp_arrival_time_frac=*/nullptr,
                           &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }
  {
    MutexLock lock(&sync_info_lock_);
    if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_) {
      return absl::nullopt;
    }
    info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
    info.latest_receive_time_ms = last_received_rtp_system_time_->ms();
  }

  // Leaves info.current_delay_ms uninitialized.
  return info;
}

RtpVideoStreamReceiver::ParseGenericDependenciesResult
RtpVideoStreamReceiver::ParseGenericDependenciesExtension(
    const RtpPacketReceived& rtp_packet,
    RTPVideoHeader* video_header) {
  if (rtp_packet.HasExtension<RtpDependencyDescriptorExtension>()) {
    webrtc::DependencyDescriptor dependency_descriptor;
    if (!rtp_packet.GetExtension<RtpDependencyDescriptorExtension>(
            video_structure_.get(), &dependency_descriptor)) {
      // Descriptor is there, but failed to parse. Either it is invalid,
      // or too old packet (after relevant video_structure_ changed),
      // or too new packet (before relevant video_structure_ arrived).
      // Drop such packet to be on the safe side.
      // TODO(bugs.webrtc.org/10342): Stash too new packet.
      RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                          << " Failed to parse dependency descriptor.";
      return kDropPacket;
    }
    if (dependency_descriptor.attached_structure != nullptr &&
        !dependency_descriptor.first_packet_in_frame) {
      RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.Ssrc()
                          << "Invalid dependency descriptor: structure "
                             "attached to non first packet of a frame.";
      return kDropPacket;
    }
    video_header->is_first_packet_in_frame =
        dependency_descriptor.first_packet_in_frame;
    video_header->is_last_packet_in_frame =
        dependency_descriptor.last_packet_in_frame;

    int64_t frame_id =
        frame_id_unwrapper_.Unwrap(dependency_descriptor.frame_number);
    auto& generic_descriptor_info = video_header->generic.emplace();
    generic_descriptor_info.frame_id = frame_id;
    generic_descriptor_info.spatial_index =
        dependency_descriptor.frame_dependencies.spatial_id;
    generic_descriptor_info.temporal_index =
        dependency_descriptor.frame_dependencies.temporal_id;
    for (int fdiff : dependency_descriptor.frame_dependencies.frame_diffs) {
      generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
    }
    generic_descriptor_info.decode_target_indications =
        dependency_descriptor.frame_dependencies.decode_target_indications;
    if (dependency_descriptor.resolution) {
      video_header->width = dependency_descriptor.resolution->Width();
      video_header->height = dependency_descriptor.resolution->Height();
    }

    // FrameDependencyStructure is sent in dependency descriptor of the first
    // packet of a key frame and required for parsed dependency descriptor in
    // all the following packets until next key frame.
    // Save it if there is a (potentially) new structure.
    if (dependency_descriptor.attached_structure) {
      RTC_DCHECK(dependency_descriptor.first_packet_in_frame);
      if (video_structure_frame_id_ > frame_id) {
        RTC_LOG(LS_WARNING)
            << "Arrived key frame with id " << frame_id << " and structure id "
            << dependency_descriptor.attached_structure->structure_id
            << " is older than the latest received key frame with id "
            << *video_structure_frame_id_ << " and structure id "
            << video_structure_->structure_id;
        return kDropPacket;
      }
      video_structure_ = std::move(dependency_descriptor.attached_structure);
      video_structure_frame_id_ = frame_id;
      video_header->frame_type = VideoFrameType::kVideoFrameKey;
    } else {
      video_header->frame_type = VideoFrameType::kVideoFrameDelta;
    }
    return kHasGenericDescriptor;
  }

  RtpGenericFrameDescriptor generic_frame_descriptor;
  if (!rtp_packet.GetExtension<RtpGenericFrameDescriptorExtension00>(
          &generic_frame_descriptor)) {
    return kNoGenericDescriptor;
  }

  video_header->is_first_packet_in_frame =
      generic_frame_descriptor.FirstPacketInSubFrame();
  video_header->is_last_packet_in_frame =
      generic_frame_descriptor.LastPacketInSubFrame();

  if (generic_frame_descriptor.FirstPacketInSubFrame()) {
    video_header->frame_type =
        generic_frame_descriptor.FrameDependenciesDiffs().empty()
            ? VideoFrameType::kVideoFrameKey
            : VideoFrameType::kVideoFrameDelta;

    auto& generic_descriptor_info = video_header->generic.emplace();
    int64_t frame_id =
        frame_id_unwrapper_.Unwrap(generic_frame_descriptor.FrameId());
    generic_descriptor_info.frame_id = frame_id;
    generic_descriptor_info.spatial_index =
        generic_frame_descriptor.SpatialLayer();
    generic_descriptor_info.temporal_index =
        generic_frame_descriptor.TemporalLayer();
    for (uint16_t fdiff : generic_frame_descriptor.FrameDependenciesDiffs()) {
      generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
    }
  }
  video_header->width = generic_frame_descriptor.Width();
  video_header->height = generic_frame_descriptor.Height();
  return kHasGenericDescriptor;
}

void RtpVideoStreamReceiver::OnReceivedPayloadData(
    rtc::CopyOnWriteBuffer codec_payload,
    const RtpPacketReceived& rtp_packet,
    const RTPVideoHeader& video) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);

  auto packet =
      std::make_unique<video_coding::PacketBuffer::Packet>(rtp_packet, video);

  RTPVideoHeader& video_header = packet->video_header;
  video_header.rotation = kVideoRotation_0;
  video_header.content_type = VideoContentType::UNSPECIFIED;
  video_header.video_timing.flags = VideoSendTiming::kInvalid;
  video_header.is_last_packet_in_frame |= rtp_packet.Marker();

  if (const auto* vp9_header =
          absl::get_if<RTPVideoHeaderVP9>(&video_header.video_type_header)) {
    video_header.is_last_packet_in_frame |= vp9_header->end_of_frame;
    video_header.is_first_packet_in_frame |= vp9_header->beginning_of_frame;
  }

  rtp_packet.GetExtension<VideoOrientation>(&video_header.rotation);
  rtp_packet.GetExtension<VideoContentTypeExtension>(
      &video_header.content_type);
  rtp_packet.GetExtension<VideoTimingExtension>(&video_header.video_timing);
  if (forced_playout_delay_max_ms_ && forced_playout_delay_min_ms_) {
    video_header.playout_delay.max_ms = *forced_playout_delay_max_ms_;
    video_header.playout_delay.min_ms = *forced_playout_delay_min_ms_;
  } else {
    rtp_packet.GetExtension<PlayoutDelayLimits>(&video_header.playout_delay);
  }

  ParseGenericDependenciesResult generic_descriptor_state =
      ParseGenericDependenciesExtension(rtp_packet, &video_header);

  if (!rtp_packet.recovered()) {
    UpdatePacketReceiveTimestamps(
        rtp_packet, video_header.frame_type == VideoFrameType::kVideoFrameKey);
  }

  if (generic_descriptor_state == kDropPacket)
    return;

  // Color space should only be transmitted in the last packet of a frame,
  // therefore, neglect it otherwise so that last_color_space_ is not reset by
  // mistake.
  if (video_header.is_last_packet_in_frame) {
    video_header.color_space = rtp_packet.GetExtension<ColorSpaceExtension>();
    if (video_header.color_space ||
        video_header.frame_type == VideoFrameType::kVideoFrameKey) {
      // Store color space since it's only transmitted when changed or for key
      // frames. Color space will be cleared if a key frame is transmitted
      // without color space information.
      last_color_space_ = video_header.color_space;
    } else if (last_color_space_) {
      video_header.color_space = last_color_space_;
    }
  }
  video_header.video_frame_tracking_id =
      rtp_packet.GetExtension<VideoFrameTrackingIdExtension>();

  if (loss_notification_controller_) {
    if (rtp_packet.recovered()) {
      // TODO(bugs.webrtc.org/10336): Implement support for reordering.
      RTC_LOG(LS_INFO)
          << "LossNotificationController does not support reordering.";
    } else if (generic_descriptor_state == kNoGenericDescriptor) {
      RTC_LOG(LS_WARNING) << "LossNotificationController requires generic "
                             "frame descriptor, but it is missing.";
    } else {
      if (video_header.is_first_packet_in_frame) {
        RTC_DCHECK(video_header.generic);
        LossNotificationController::FrameDetails frame;
        frame.is_keyframe =
            video_header.frame_type == VideoFrameType::kVideoFrameKey;
        frame.frame_id = video_header.generic->frame_id;
        frame.frame_dependencies = video_header.generic->dependencies;
        loss_notification_controller_->OnReceivedPacket(
            rtp_packet.SequenceNumber(), &frame);
      } else {
        loss_notification_controller_->OnReceivedPacket(
            rtp_packet.SequenceNumber(), nullptr);
      }
    }
  }

  if (nack_module_) {
    const bool is_keyframe =
        video_header.is_first_packet_in_frame &&
        video_header.frame_type == VideoFrameType::kVideoFrameKey;

    packet->times_nacked = nack_module_->OnReceivedPacket(
        rtp_packet.SequenceNumber(), is_keyframe, rtp_packet.recovered());
  } else {
    packet->times_nacked = -1;
  }

  if (codec_payload.size() == 0) {
    NotifyReceiverOfEmptyPacket(packet->seq_num);
    rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
    return;
  }

  if (packet->codec() == kVideoCodecH264) {
    // Only when we start to receive packets will we know what payload type
    // that will be used. When we know the payload type insert the correct
    // sps/pps into the tracker.
    if (packet->payload_type != last_payload_type_) {
      last_payload_type_ = packet->payload_type;
      InsertSpsPpsIntoTracker(packet->payload_type);
    }

    video_coding::H264SpsPpsTracker::FixedBitstream fixed =
        tracker_.CopyAndFixBitstream(
            rtc::MakeArrayView(codec_payload.cdata(), codec_payload.size()),
            &packet->video_header);

    switch (fixed.action) {
      case video_coding::H264SpsPpsTracker::kRequestKeyframe:
        rtcp_feedback_buffer_.RequestKeyFrame();
        rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
        ABSL_FALLTHROUGH_INTENDED;
      case video_coding::H264SpsPpsTracker::kDrop:
        return;
      case video_coding::H264SpsPpsTracker::kInsert:
        packet->video_payload = std::move(fixed.bitstream);
        break;
    }
#ifndef DISABLE_H265
  } else if (packet->codec() == kVideoCodecH265) {
    video_coding::H265VpsSpsPpsTracker::FixedBitstream fixed =
        h265_tracker_.CopyAndFixBitstream(
            rtc::MakeArrayView(codec_payload.cdata(), codec_payload.size()),
            &packet->video_header);
    switch (fixed.action) {
      case video_coding::H265VpsSpsPpsTracker::kRequestKeyframe:
        rtcp_feedback_buffer_.RequestKeyFrame();
        rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
        ABSL_FALLTHROUGH_INTENDED;
      case video_coding::H265VpsSpsPpsTracker::kDrop:
        return;
      case video_coding::H265VpsSpsPpsTracker::kInsert:
        packet->video_payload = std::move(fixed.bitstream);
        break;
    }
#endif
  } else {
    packet->video_payload = std::move(codec_payload);
  }

  rtcp_feedback_buffer_.SendBufferedRtcpFeedback();
  frame_counter_.Add(packet->timestamp);
  video_coding::PacketBuffer::InsertResult insert_result;
  {
    MutexLock lock(&packet_buffer_lock_);
    int64_t unwrapped_rtp_seq_num =
        rtp_seq_num_unwrapper_.Unwrap(rtp_packet.SequenceNumber());
    auto& packet_info =
        packet_infos_
            .emplace(
                unwrapped_rtp_seq_num,
                RtpPacketInfo(
                    rtp_packet.Ssrc(), rtp_packet.Csrcs(),
                    rtp_packet.Timestamp(),
                    /*audio_level=*/absl::nullopt,
                    rtp_packet.GetExtension<AbsoluteCaptureTimeExtension>(),
                    /*receive_time_ms=*/clock_->TimeInMilliseconds()))
            .first->second;

    // Try to extrapolate absolute capture time if it is missing.
    packet_info.set_absolute_capture_time(
        absolute_capture_time_interpolator_.OnReceivePacket(
            AbsoluteCaptureTimeInterpolator::GetSource(packet_info.ssrc(),
                                                       packet_info.csrcs()),
            packet_info.rtp_timestamp(),
            // Assume frequency is the same one for all video frames.
            kVideoPayloadTypeFrequency, packet_info.absolute_capture_time()));

    insert_result = packet_buffer_.InsertPacket(std::move(packet));
  }
  OnInsertedPacket(std::move(insert_result));
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
  RTC_DCHECK_RUN_ON(&worker_task_checker_);

  if (!receiving_) {
    return;
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

void RtpVideoStreamReceiver::RequestKeyFrame() {
  // TODO(bugs.webrtc.org/10336): Allow the sender to ignore key frame requests
  // issued by anything other than the LossNotificationController if it (the
  // sender) is relying on LNTF alone.
  if (keyframe_request_sender_) {
    keyframe_request_sender_->RequestKeyFrame();
  } else {
    rtp_rtcp_->SendPictureLossIndication();
  }
}

void RtpVideoStreamReceiver::SendLossNotification(
    uint16_t last_decoded_seq_num,
    uint16_t last_received_seq_num,
    bool decodability_flag,
    bool buffering_allowed) {
  RTC_DCHECK(config_.rtp.lntf.enabled);
  rtp_rtcp_->SendLossNotification(last_decoded_seq_num, last_received_seq_num,
                                  decodability_flag, buffering_allowed);
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

bool RtpVideoStreamReceiver::IsDecryptable() const {
  return frames_decryptable_.load();
}

void RtpVideoStreamReceiver::OnInsertedPacket(
    video_coding::PacketBuffer::InsertResult result) {
  std::vector<std::unique_ptr<RtpFrameObject>> assembled_frames;
  {
    MutexLock lock(&packet_buffer_lock_);
    video_coding::PacketBuffer::Packet* first_packet = nullptr;
    int max_nack_count;
    int64_t min_recv_time;
    int64_t max_recv_time;
    std::vector<rtc::ArrayView<const uint8_t>> payloads;
    RtpPacketInfos::vector_type packet_infos;

    bool frame_boundary = true;
    for (auto& packet : result.packets) {
      // PacketBuffer promisses frame boundaries are correctly set on each
      // packet. Document that assumption with the DCHECKs.
      RTC_DCHECK_EQ(frame_boundary, packet->is_first_packet_in_frame());
      int64_t unwrapped_rtp_seq_num =
          rtp_seq_num_unwrapper_.Unwrap(packet->seq_num);
      RTC_DCHECK(packet_infos_.count(unwrapped_rtp_seq_num) > 0);
      RtpPacketInfo& packet_info = packet_infos_[unwrapped_rtp_seq_num];
      if (packet->is_first_packet_in_frame()) {
        first_packet = packet.get();
        max_nack_count = packet->times_nacked;
        min_recv_time = packet_info.receive_time().ms();
        max_recv_time = packet_info.receive_time().ms();
        payloads.clear();
        packet_infos.clear();
      } else {
        max_nack_count = std::max(max_nack_count, packet->times_nacked);
        min_recv_time =
            std::min(min_recv_time, packet_info.receive_time().ms());
        max_recv_time =
            std::max(max_recv_time, packet_info.receive_time().ms());
      }
      payloads.emplace_back(packet->video_payload);
      packet_infos.push_back(packet_info);

      frame_boundary = packet->is_last_packet_in_frame();
      if (packet->is_last_packet_in_frame()) {
        auto depacketizer_it =
            payload_type_map_.find(first_packet->payload_type);
        RTC_CHECK(depacketizer_it != payload_type_map_.end());

        rtc::scoped_refptr<EncodedImageBuffer> bitstream =
            depacketizer_it->second->AssembleFrame(payloads);
        if (!bitstream) {
          // Failed to assemble a frame. Discard and continue.
          continue;
        }

        const video_coding::PacketBuffer::Packet& last_packet = *packet;
        assembled_frames.push_back(std::make_unique<RtpFrameObject>(
            first_packet->seq_num,                             //
            last_packet.seq_num,                               //
            last_packet.marker_bit,                            //
            max_nack_count,                                    //
            min_recv_time,                                     //
            max_recv_time,                                     //
            first_packet->timestamp,                           //
            ntp_estimator_.Estimate(first_packet->timestamp),  //
            last_packet.video_header.video_timing,             //
            first_packet->payload_type,                        //
            first_packet->codec(),                             //
            last_packet.video_header.rotation,                 //
            last_packet.video_header.content_type,             //
            first_packet->video_header,                        //
            last_packet.video_header.color_space,              //
            RtpPacketInfos(std::move(packet_infos)),           //
            std::move(bitstream)));
      }
    }
    RTC_DCHECK(frame_boundary);

    if (result.buffer_cleared) {
      packet_infos_.clear();
    }
  }  // packet_buffer_lock_

  if (result.buffer_cleared) {
    {
      MutexLock lock(&sync_info_lock_);
      last_received_rtp_system_time_.reset();
      last_received_keyframe_rtp_system_time_.reset();
      last_received_keyframe_rtp_timestamp_.reset();
    }
    RequestKeyFrame();
  }

  for (auto& frame : assembled_frames) {
    OnAssembledFrame(std::move(frame));
  }
}

void RtpVideoStreamReceiver::OnAssembledFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  RTC_DCHECK_RUN_ON(&network_tc_);
  RTC_DCHECK(frame);

#if defined(WEBRTC_WEBKIT_BUILD)
    static const unsigned MaxFrameDelayCount = 3;
    static const unsigned TimeStampQueueDurationInMs = 2000;
    static const unsigned OneSecondInMs = 1000;
    // FIXME: Consider using MovingAverage instead.
    ++frameCount_;

    auto frameTime = clock_->TimeInMilliseconds();
    auto lastFrameTime = observedFrameTimeStamps_.empty() ? frameTime : observedFrameTimeStamps_.back();

    if (observedFrameRate_) {
      auto delay = frameTime - lastFrameTime;
      if (delay > (MaxFrameDelayCount * OneSecondInMs / observedFrameRate_)) {
          RTC_LOG(LS_INFO) << "RtpVideoStreamReceiver::OnAssembledFrame at " << frameTime << ", previous frame was at " << lastFrameTime << ", observed frame rate is " << observedFrameRate_ << ", delay since last frame is " << (frameTime - lastFrameTime) <<  " ms, frame count is " << frameCount_ << ", size is " << frame->EncodedImage().size() << ", frame reception duration is " << (frame->video_timing().receive_finish_ms - frame->video_timing().receive_start_ms) << " ms, " << " observed samples count is " << observedFrameTimeStamps_.size() << ", observed samples duration is " << (observedFrameTimeStamps_.back() - observedFrameTimeStamps_.front());
      }
    }

    observedFrameTimeStamps_.push_back(frameTime);
    while (observedFrameTimeStamps_.front() <= (frameTime - TimeStampQueueDurationInMs))
        observedFrameTimeStamps_.pop_front();

    auto queueDuration = frameTime - observedFrameTimeStamps_.front();
    if (queueDuration > OneSecondInMs)
        observedFrameRate_ = observedFrameTimeStamps_.size() * OneSecondInMs / queueDuration;
#endif

  const absl::optional<RTPVideoHeader::GenericDescriptorInfo>& descriptor =
      frame->GetRtpVideoHeader().generic;

  if (loss_notification_controller_ && descriptor) {
    loss_notification_controller_->OnAssembledFrame(
        frame->first_seq_num(), descriptor->frame_id,
        absl::c_linear_search(descriptor->decode_target_indications,
                              DecodeTargetIndication::kDiscardable),
        descriptor->dependencies);
  }

  // If frames arrive before a key frame, they would not be decodable.
  // In that case, request a key frame ASAP.
  if (!has_received_frame_) {
    if (frame->FrameType() != VideoFrameType::kVideoFrameKey) {
      // `loss_notification_controller_`, if present, would have already
      // requested a key frame when the first packet for the non-key frame
      // had arrived, so no need to replicate the request.
      if (!loss_notification_controller_) {
        RequestKeyFrame();
      }
    }
    has_received_frame_ = true;
  }

  MutexLock lock(&reference_finder_lock_);
  // Reset `reference_finder_` if `frame` is new and the codec have changed.
  if (current_codec_) {
    bool frame_is_newer =
        AheadOf(frame->Timestamp(), last_assembled_frame_rtp_timestamp_);

    if (frame->codec_type() != current_codec_) {
      if (frame_is_newer) {
        // When we reset the `reference_finder_` we don't want new picture ids
        // to overlap with old picture ids. To ensure that doesn't happen we
        // start from the `last_completed_picture_id_` and add an offset in
        // case of reordering.
        reference_finder_ = std::make_unique<RtpFrameReferenceFinder>(
            last_completed_picture_id_ + std::numeric_limits<uint16_t>::max());
        current_codec_ = frame->codec_type();
      } else {
        // Old frame from before the codec switch, discard it.
        return;
      }
    }

    if (frame_is_newer) {
      last_assembled_frame_rtp_timestamp_ = frame->Timestamp();
    }
  } else {
    current_codec_ = frame->codec_type();
    last_assembled_frame_rtp_timestamp_ = frame->Timestamp();
  }

  if (buffered_frame_decryptor_ != nullptr) {
    buffered_frame_decryptor_->ManageEncryptedFrame(std::move(frame));
  } else if (frame_transformer_delegate_) {
    frame_transformer_delegate_->TransformFrame(std::move(frame));
  } else {
    OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
  }
}

void RtpVideoStreamReceiver::OnCompleteFrames(
    RtpFrameReferenceFinder::ReturnVector frames) {
  {
    MutexLock lock(&last_seq_num_mutex_);
    for (const auto& frame : frames) {
      RtpFrameObject* rtp_frame = static_cast<RtpFrameObject*>(frame.get());
      last_seq_num_for_pic_id_[rtp_frame->Id()] = rtp_frame->last_seq_num();
    }
  }
  for (auto& frame : frames) {
    last_completed_picture_id_ =
        std::max(last_completed_picture_id_, frame->Id());
    complete_frame_callback_->OnCompleteFrame(std::move(frame));
  }
}

void RtpVideoStreamReceiver::OnDecryptedFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  MutexLock lock(&reference_finder_lock_);
  OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
}

void RtpVideoStreamReceiver::OnDecryptionStatusChange(
    FrameDecryptorInterface::Status status) {
  frames_decryptable_.store(
      (status == FrameDecryptorInterface::Status::kOk) ||
      (status == FrameDecryptorInterface::Status::kRecoverable));
}

void RtpVideoStreamReceiver::SetFrameDecryptor(
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  RTC_DCHECK_RUN_ON(&network_tc_);
  if (buffered_frame_decryptor_ == nullptr) {
    buffered_frame_decryptor_ =
        std::make_unique<BufferedFrameDecryptor>(this, this);
  }
  buffered_frame_decryptor_->SetFrameDecryptor(std::move(frame_decryptor));
}

void RtpVideoStreamReceiver::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&network_tc_);
  frame_transformer_delegate_ =
      rtc::make_ref_counted<RtpVideoStreamReceiverFrameTransformerDelegate>(
          this, std::move(frame_transformer), rtc::Thread::Current(),
          config_.rtp.remote_ssrc);
  frame_transformer_delegate_->Init();
}

void RtpVideoStreamReceiver::UpdateRtt(int64_t max_rtt_ms) {
  if (nack_module_)
    nack_module_->UpdateRtt(max_rtt_ms);
}

absl::optional<int64_t> RtpVideoStreamReceiver::LastReceivedPacketMs() const {
  MutexLock lock(&sync_info_lock_);
  if (last_received_rtp_system_time_) {
    return absl::optional<int64_t>(last_received_rtp_system_time_->ms());
  }
  return absl::nullopt;
}

absl::optional<int64_t> RtpVideoStreamReceiver::LastReceivedKeyframePacketMs()
    const {
  MutexLock lock(&sync_info_lock_);
  if (last_received_keyframe_rtp_system_time_) {
    return absl::optional<int64_t>(
        last_received_keyframe_rtp_system_time_->ms());
  }
  return absl::nullopt;
}

void RtpVideoStreamReceiver::AddSecondarySink(RtpPacketSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  RTC_DCHECK(!absl::c_linear_search(secondary_sinks_, sink));
  secondary_sinks_.push_back(sink);
}

void RtpVideoStreamReceiver::RemoveSecondarySink(
    const RtpPacketSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  auto it = absl::c_find(secondary_sinks_, sink);
  if (it == secondary_sinks_.end()) {
    // We might be rolling-back a call whose setup failed mid-way. In such a
    // case, it's simpler to remove "everything" rather than remember what
    // has already been added.
    RTC_LOG(LS_WARNING) << "Removal of unknown sink.";
    return;
  }
  secondary_sinks_.erase(it);
}

void RtpVideoStreamReceiver::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  MutexLock lock(&reference_finder_lock_);
  OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
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

  const auto type_it = payload_type_map_.find(packet.PayloadType());
  if (type_it == payload_type_map_.end()) {
    return;
  }
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload =
      type_it->second->Parse(packet.PayloadBuffer());
  if (parsed_payload == absl::nullopt) {
    RTC_LOG(LS_WARNING) << "Failed parsing payload.";
    return;
  }

  OnReceivedPayloadData(std::move(parsed_payload->video_payload), packet,
                        parsed_payload->video_header);
}

void RtpVideoStreamReceiver::ParseAndHandleEncapsulatingHeader(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  if (packet.PayloadType() == config_.rtp.red_payload_type &&
      packet.payload_size() > 0) {
    if (packet.payload()[0] == config_.rtp.ulpfec_payload_type) {
      // Notify video_receiver about received FEC packets to avoid NACKing these
      // packets.
      NotifyReceiverOfEmptyPacket(packet.SequenceNumber());
    }
    if (!ulpfec_receiver_->AddReceivedRedPacket(
            packet, config_.rtp.ulpfec_payload_type)) {
      return;
    }
    ulpfec_receiver_->ProcessReceivedFec();
  }
}

// In the case of a video stream without picture ids and no rtx the
// RtpFrameReferenceFinder will need to know about padding to
// correctly calculate frame references.
void RtpVideoStreamReceiver::NotifyReceiverOfEmptyPacket(uint16_t seq_num) {
  {
    MutexLock lock(&reference_finder_lock_);
    OnCompleteFrames(reference_finder_->PaddingReceived(seq_num));
  }

  video_coding::PacketBuffer::InsertResult insert_result;
  {
    MutexLock lock(&packet_buffer_lock_);
    insert_result = packet_buffer_.InsertPadding(seq_num);
  }
  OnInsertedPacket(std::move(insert_result));

  if (nack_module_) {
    nack_module_->OnReceivedPacket(seq_num, /* is_keyframe = */ false,
                                   /* is _recovered = */ false);
  }
  if (loss_notification_controller_) {
    // TODO(bugs.webrtc.org/10336): Handle empty packets.
    RTC_LOG(LS_WARNING)
        << "LossNotificationController does not expect empty packets.";
  }
}

bool RtpVideoStreamReceiver::DeliverRtcp(const uint8_t* rtcp_packet,
                                         size_t rtcp_packet_length) {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);

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
    absl::optional<int64_t> remote_to_local_clock_offset_ms =
        ntp_estimator_.EstimateRemoteToLocalClockOffsetMs();
    if (remote_to_local_clock_offset_ms.has_value()) {
      capture_clock_offset_updater_.SetRemoteToLocalClockOffset(
          Int64MsToQ32x32(*remote_to_local_clock_offset_ms));
    }
  }

  return true;
}

void RtpVideoStreamReceiver::FrameContinuous(int64_t picture_id) {
  if (!nack_module_)
    return;

  int seq_num = -1;
  {
    MutexLock lock(&last_seq_num_mutex_);
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
    MutexLock lock(&last_seq_num_mutex_);
    auto seq_num_it = last_seq_num_for_pic_id_.find(picture_id);
    if (seq_num_it != last_seq_num_for_pic_id_.end()) {
      seq_num = seq_num_it->second;
      last_seq_num_for_pic_id_.erase(last_seq_num_for_pic_id_.begin(),
                                     ++seq_num_it);
    }
  }
  if (seq_num != -1) {
    {
      MutexLock lock(&packet_buffer_lock_);
      packet_buffer_.ClearTo(seq_num);
      int64_t unwrapped_rtp_seq_num = rtp_seq_num_unwrapper_.Unwrap(seq_num);
      packet_infos_.erase(packet_infos_.begin(),
                          packet_infos_.upper_bound(unwrapped_rtp_seq_num));
    }
    MutexLock lock(&reference_finder_lock_);
    reference_finder_->ClearTo(seq_num);
  }
}

void RtpVideoStreamReceiver::SignalNetworkState(NetworkState state) {
  rtp_rtcp_->SetRTCPStatus(state == kNetworkUp ? config_.rtp.rtcp_mode
                                               : RtcpMode::kOff);
}

void RtpVideoStreamReceiver::StartReceive() {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
  receiving_ = true;
}

void RtpVideoStreamReceiver::StopReceive() {
  RTC_DCHECK_RUN_ON(&worker_task_checker_);
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
  if (config_.rtp.ulpfec_payload_type != -1) {
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.FecBitrateReceivedInKbps",
        static_cast<int>(counter.num_bytes * 8 / elapsed_sec / 1000));
  }
}

void RtpVideoStreamReceiver::InsertSpsPpsIntoTracker(uint8_t payload_type) {
  auto codec_params_it = pt_codec_params_.find(payload_type);
  if (codec_params_it == pt_codec_params_.end())
    return;

  RTC_LOG(LS_INFO) << "Found out of band supplied codec parameters for"
                      " payload type: "
                   << static_cast<int>(payload_type);

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

void RtpVideoStreamReceiver::UpdatePacketReceiveTimestamps(
    const RtpPacketReceived& packet,
    bool is_keyframe) {
  Timestamp now = clock_->CurrentTime();
  {
    MutexLock lock(&sync_info_lock_);
    if (is_keyframe ||
        last_received_keyframe_rtp_timestamp_ == packet.Timestamp()) {
      last_received_keyframe_rtp_timestamp_ = packet.Timestamp();
      last_received_keyframe_rtp_system_time_ = now;
    }
    last_received_rtp_system_time_ = now;
    last_received_rtp_timestamp_ = packet.Timestamp();
  }

  // Periodically log the RTP header of incoming packets.
  if (now.ms() - last_packet_log_ms_ > kPacketLogIntervalMs) {
    rtc::StringBuilder ss;
    ss << "Packet received on SSRC: " << packet.Ssrc()
       << " with payload type: " << static_cast<int>(packet.PayloadType())
       << ", timestamp: " << packet.Timestamp()
       << ", sequence number: " << packet.SequenceNumber()
       << ", arrival time: " << ToString(packet.arrival_time());
    int32_t time_offset;
    if (packet.GetExtension<TransmissionOffset>(&time_offset)) {
      ss << ", toffset: " << time_offset;
    }
    uint32_t send_time;
    if (packet.GetExtension<AbsoluteSendTime>(&send_time)) {
      ss << ", abs send time: " << send_time;
    }
    RTC_LOG(LS_INFO) << ss.str();
    last_packet_log_ms_ = now.ms();
  }
}

}  // namespace webrtc
