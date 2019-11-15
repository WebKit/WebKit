/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_video_sender.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/transport/field_trial_based_config.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/playout_delay_oracle.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace webrtc_internal_rtp_video_sender {

RtpStreamSender::RtpStreamSender(
    std::unique_ptr<PlayoutDelayOracle> playout_delay_oracle,
    std::unique_ptr<RtpRtcp> rtp_rtcp,
    std::unique_ptr<RTPSenderVideo> sender_video)
    : playout_delay_oracle(std::move(playout_delay_oracle)),
      rtp_rtcp(std::move(rtp_rtcp)),
      sender_video(std::move(sender_video)) {}

RtpStreamSender::~RtpStreamSender() = default;

}  // namespace webrtc_internal_rtp_video_sender

namespace {
static const int kMinSendSidePacketHistorySize = 600;
// We don't do MTU discovery, so assume that we have the standard ethernet MTU.
static const size_t kPathMTU = 1500;

using webrtc_internal_rtp_video_sender::RtpStreamSender;

std::vector<RtpStreamSender> CreateRtpStreamSenders(
    Clock* clock,
    const RtpConfig& rtp_config,
    int rtcp_report_interval_ms,
    Transport* send_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpLossNotificationObserver* rtcp_loss_notification_observer,
    RtcpBandwidthObserver* bandwidth_callback,
    RtpTransportControllerSendInterface* transport,
    RtcpRttStats* rtt_stats,
    FlexfecSender* flexfec_sender,
    BitrateStatisticsObserver* bitrate_observer,
    RtcpPacketTypeCounterObserver* rtcp_type_observer,
    SendSideDelayObserver* send_delay_observer,
    SendPacketObserver* send_packet_observer,
    RtcEventLog* event_log,
    RateLimiter* retransmission_rate_limiter,
    OverheadObserver* overhead_observer,
    FrameEncryptorInterface* frame_encryptor,
    const CryptoOptions& crypto_options) {
  RTC_DCHECK_GT(rtp_config.ssrcs.size(), 0);

  RtpRtcp::Configuration configuration;
  configuration.clock = clock;
  configuration.audio = false;
  configuration.receiver_only = false;
  configuration.outgoing_transport = send_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.rtcp_loss_notification_observer =
      rtcp_loss_notification_observer;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.network_state_estimate_observer =
      transport->network_state_estimate_observer();
  configuration.transport_feedback_callback =
      transport->transport_feedback_observer();
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer = rtcp_type_observer;
  configuration.paced_sender = transport->packet_sender();
  configuration.send_bitrate_observer = bitrate_observer;
  configuration.send_side_delay_observer = send_delay_observer;
  configuration.send_packet_observer = send_packet_observer;
  configuration.event_log = event_log;
  configuration.retransmission_rate_limiter = retransmission_rate_limiter;
  configuration.overhead_observer = overhead_observer;
  configuration.frame_encryptor = frame_encryptor;
  configuration.require_frame_encryption =
      crypto_options.sframe.require_frame_encryption;
  configuration.extmap_allow_mixed = rtp_config.extmap_allow_mixed;
  configuration.rtcp_report_interval_ms = rtcp_report_interval_ms;

  std::vector<RtpStreamSender> rtp_streams;
  const std::vector<uint32_t>& flexfec_protected_ssrcs =
      rtp_config.flexfec.protected_media_ssrcs;
  RTC_DCHECK(rtp_config.rtx.ssrcs.empty() ||
             rtp_config.rtx.ssrcs.size() == rtp_config.rtx.ssrcs.size());
  for (size_t i = 0; i < rtp_config.ssrcs.size(); ++i) {
    configuration.local_media_ssrc = rtp_config.ssrcs[i];
    bool enable_flexfec = flexfec_sender != nullptr &&
                          std::find(flexfec_protected_ssrcs.begin(),
                                    flexfec_protected_ssrcs.end(),
                                    *configuration.local_media_ssrc) !=
                              flexfec_protected_ssrcs.end();
    configuration.flexfec_sender = enable_flexfec ? flexfec_sender : nullptr;
    auto playout_delay_oracle = absl::make_unique<PlayoutDelayOracle>();

    configuration.ack_observer = playout_delay_oracle.get();
    if (rtp_config.rtx.ssrcs.size() > i) {
      configuration.rtx_send_ssrc = rtp_config.rtx.ssrcs[i];
    }

    auto rtp_rtcp = RtpRtcp::Create(configuration);
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);

    auto sender_video = absl::make_unique<RTPSenderVideo>(
        configuration.clock, rtp_rtcp->RtpSender(),
        configuration.flexfec_sender, playout_delay_oracle.get(),
        frame_encryptor, crypto_options.sframe.require_frame_encryption,
        rtp_config.lntf.enabled, FieldTrialBasedConfig());
    rtp_streams.emplace_back(std::move(playout_delay_oracle),
                             std::move(rtp_rtcp), std::move(sender_video));
  }
  return rtp_streams;
}

bool PayloadTypeSupportsSkippingFecPackets(const std::string& payload_name) {
  const VideoCodecType codecType = PayloadStringToCodecType(payload_name);
  if (codecType == kVideoCodecVP8 || codecType == kVideoCodecVP9) {
    return true;
  }
  if (codecType == kVideoCodecGeneric &&
      field_trial::IsEnabled("WebRTC-GenericPictureId")) {
    return true;
  }
  return false;
}

// TODO(brandtr): Update this function when we support multistream protection.
std::unique_ptr<FlexfecSender> MaybeCreateFlexfecSender(
    Clock* clock,
    const RtpConfig& rtp,
    const std::map<uint32_t, RtpState>& suspended_ssrcs) {
  if (rtp.flexfec.payload_type < 0) {
    return nullptr;
  }
  RTC_DCHECK_GE(rtp.flexfec.payload_type, 0);
  RTC_DCHECK_LE(rtp.flexfec.payload_type, 127);
  if (rtp.flexfec.ssrc == 0) {
    RTC_LOG(LS_WARNING) << "FlexFEC is enabled, but no FlexFEC SSRC given. "
                           "Therefore disabling FlexFEC.";
    return nullptr;
  }
  if (rtp.flexfec.protected_media_ssrcs.empty()) {
    RTC_LOG(LS_WARNING)
        << "FlexFEC is enabled, but no protected media SSRC given. "
           "Therefore disabling FlexFEC.";
    return nullptr;
  }

  if (rtp.flexfec.protected_media_ssrcs.size() > 1) {
    RTC_LOG(LS_WARNING)
        << "The supplied FlexfecConfig contained multiple protected "
           "media streams, but our implementation currently only "
           "supports protecting a single media stream. "
           "To avoid confusion, disabling FlexFEC completely.";
    return nullptr;
  }

  const RtpState* rtp_state = nullptr;
  auto it = suspended_ssrcs.find(rtp.flexfec.ssrc);
  if (it != suspended_ssrcs.end()) {
    rtp_state = &it->second;
  }

  RTC_DCHECK_EQ(1U, rtp.flexfec.protected_media_ssrcs.size());
  return absl::make_unique<FlexfecSender>(
      rtp.flexfec.payload_type, rtp.flexfec.ssrc,
      rtp.flexfec.protected_media_ssrcs[0], rtp.mid, rtp.extensions,
      RTPSender::FecExtensionSizes(), rtp_state, clock);
}

DataRate CalculateOverheadRate(DataRate data_rate,
                               DataSize packet_size,
                               DataSize overhead_per_packet) {
  Frequency packet_rate = data_rate / packet_size;
  // TOSO(srte): We should not need to round to nearest whole packet per second
  // rate here.
  return packet_rate.RoundUpTo(Frequency::hertz(1)) * overhead_per_packet;
}
}  // namespace

RtpVideoSender::RtpVideoSender(
    Clock* clock,
    std::map<uint32_t, RtpState> suspended_ssrcs,
    const std::map<uint32_t, RtpPayloadState>& states,
    const RtpConfig& rtp_config,
    int rtcp_report_interval_ms,
    Transport* send_transport,
    const RtpSenderObservers& observers,
    RtpTransportControllerSendInterface* transport,
    RtcEventLog* event_log,
    RateLimiter* retransmission_limiter,
    std::unique_ptr<FecController> fec_controller,
    FrameEncryptorInterface* frame_encryptor,
    const CryptoOptions& crypto_options)
    : send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      account_for_packetization_overhead_(!webrtc::field_trial::IsDisabled(
          "WebRTC-SubtractPacketizationOverhead")),
      use_early_loss_detection_(
          !webrtc::field_trial::IsDisabled("WebRTC-UseEarlyLossDetection")),
      active_(false),
      module_process_thread_(nullptr),
      suspended_ssrcs_(std::move(suspended_ssrcs)),
      flexfec_sender_(
          MaybeCreateFlexfecSender(clock, rtp_config, suspended_ssrcs_)),
      fec_controller_(std::move(fec_controller)),
      fec_allowed_(true),
      rtp_streams_(
          CreateRtpStreamSenders(clock,
                                 rtp_config,
                                 rtcp_report_interval_ms,
                                 send_transport,
                                 observers.intra_frame_callback,
                                 observers.rtcp_loss_notification_observer,
                                 transport->GetBandwidthObserver(),
                                 transport,
                                 observers.rtcp_rtt_stats,
                                 flexfec_sender_.get(),
                                 observers.bitrate_observer,
                                 observers.rtcp_type_observer,
                                 observers.send_delay_observer,
                                 observers.send_packet_observer,
                                 event_log,
                                 retransmission_limiter,
                                 this,
                                 frame_encryptor,
                                 crypto_options)),
      rtp_config_(rtp_config),
      transport_(transport),
      transport_overhead_bytes_per_packet_(0),
      overhead_bytes_per_packet_(0),
      encoder_target_rate_bps_(0),
      frame_counts_(rtp_config.ssrcs.size()),
      frame_count_observer_(observers.frame_count_observer) {
  RTC_DCHECK_EQ(rtp_config_.ssrcs.size(), rtp_streams_.size());
  module_process_thread_checker_.Detach();
  // SSRCs are assumed to be sorted in the same order as |rtp_modules|.
  for (uint32_t ssrc : rtp_config_.ssrcs) {
    // Restore state if it previously existed.
    const RtpPayloadState* state = nullptr;
    auto it = states.find(ssrc);
    if (it != states.end()) {
      state = &it->second;
      shared_frame_id_ = std::max(shared_frame_id_, state->shared_frame_id);
    }
    params_.push_back(RtpPayloadParams(ssrc, state));
  }

  // RTP/RTCP initialization.

  // We add the highest spatial layer first to ensure it'll be prioritized
  // when sending padding, with the hope that the packet rate will be smaller,
  // and that it's more important to protect than the lower layers.

  // TODO(nisse): Consider moving registration with PacketRouter last, after the
  // modules are fully configured.
  for (const RtpStreamSender& stream : rtp_streams_) {
    constexpr bool remb_candidate = true;
    transport->packet_router()->AddSendRtpModule(stream.rtp_rtcp.get(),
                                                 remb_candidate);
  }

  for (size_t i = 0; i < rtp_config_.extensions.size(); ++i) {
    const std::string& extension = rtp_config_.extensions[i].uri;
    int id = rtp_config_.extensions[i].id;
    RTC_DCHECK(RtpExtension::IsSupportedForVideo(extension));
    for (const RtpStreamSender& stream : rtp_streams_) {
      RTC_CHECK(stream.rtp_rtcp->RegisterRtpHeaderExtension(extension, id));
    }
  }

  ConfigureProtection();
  ConfigureSsrcs();
  ConfigureRids();

  if (!rtp_config_.mid.empty()) {
    for (const RtpStreamSender& stream : rtp_streams_) {
      stream.rtp_rtcp->SetMid(rtp_config_.mid);
    }
  }

  for (const RtpStreamSender& stream : rtp_streams_) {
    // Simulcast has one module for each layer. Set the CNAME on all modules.
    stream.rtp_rtcp->SetCNAME(rtp_config_.c_name.c_str());
    stream.rtp_rtcp->RegisterRtcpStatisticsCallback(observers.rtcp_stats);
    stream.rtp_rtcp->SetReportBlockDataObserver(
        observers.report_block_data_observer);
    stream.rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(
        observers.rtp_stats);
    stream.rtp_rtcp->SetMaxRtpPacketSize(rtp_config_.max_packet_size);
    stream.rtp_rtcp->RegisterSendPayloadFrequency(rtp_config_.payload_type,
                                                  kVideoPayloadTypeFrequency);
    stream.sender_video->RegisterPayloadType(rtp_config_.payload_type,
                                             rtp_config_.payload_name,
                                             rtp_config_.raw_payload);
  }
  // Currently, both ULPFEC and FlexFEC use the same FEC rate calculation logic,
  // so enable that logic if either of those FEC schemes are enabled.
  fec_controller_->SetProtectionMethod(FecEnabled(), NackEnabled());

  fec_controller_->SetProtectionCallback(this);
  // Signal congestion controller this object is ready for OnPacket* callbacks.
  transport_->RegisterPacketFeedbackObserver(this);
}

RtpVideoSender::~RtpVideoSender() {
  for (const RtpStreamSender& stream : rtp_streams_) {
    transport_->packet_router()->RemoveSendRtpModule(stream.rtp_rtcp.get());
  }
  transport_->DeRegisterPacketFeedbackObserver(this);
}

void RtpVideoSender::RegisterProcessThread(
    ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;

  for (const RtpStreamSender& stream : rtp_streams_) {
    module_process_thread_->RegisterModule(stream.rtp_rtcp.get(),
                                           RTC_FROM_HERE);
  }
}

void RtpVideoSender::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  for (const RtpStreamSender& stream : rtp_streams_)
    module_process_thread_->DeRegisterModule(stream.rtp_rtcp.get());
}

void RtpVideoSender::SetActive(bool active) {
  rtc::CritScope lock(&crit_);
  if (active_ == active)
    return;
  const std::vector<bool> active_modules(rtp_streams_.size(), active);
  SetActiveModules(active_modules);
}

void RtpVideoSender::SetActiveModules(const std::vector<bool> active_modules) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK_EQ(rtp_streams_.size(), active_modules.size());
  active_ = false;
  for (size_t i = 0; i < active_modules.size(); ++i) {
    if (active_modules[i]) {
      active_ = true;
    }
    // Sends a kRtcpByeCode when going from true to false.
    rtp_streams_[i].rtp_rtcp->SetSendingStatus(active_modules[i]);
    // If set to false this module won't send media.
    rtp_streams_[i].rtp_rtcp->SetSendingMediaStatus(active_modules[i]);
  }
}

bool RtpVideoSender::IsActive() {
  rtc::CritScope lock(&crit_);
  return active_ && !rtp_streams_.empty();
}

EncodedImageCallback::Result RtpVideoSender::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  fec_controller_->UpdateWithEncodedData(encoded_image.size(),
                                         encoded_image._frameType);
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(!rtp_streams_.empty());
  if (!active_)
    return Result(Result::ERROR_SEND_FAILED);

  shared_frame_id_++;
  size_t stream_index = 0;
  if (codec_specific_info &&
      (codec_specific_info->codecType == kVideoCodecVP8 ||
       codec_specific_info->codecType == kVideoCodecH264 ||
       codec_specific_info->codecType == kVideoCodecGeneric)) {
    // Map spatial index to simulcast.
    stream_index = encoded_image.SpatialIndex().value_or(0);
  }
  RTC_DCHECK_LT(stream_index, rtp_streams_.size());
  RTPVideoHeader rtp_video_header = params_[stream_index].GetRtpVideoHeader(
      encoded_image, codec_specific_info, shared_frame_id_);

  uint32_t rtp_timestamp =
      encoded_image.Timestamp() +
      rtp_streams_[stream_index].rtp_rtcp->StartTimestamp();

  // RTCPSender has it's own copy of the timestamp offset, added in
  // RTCPSender::BuildSR, hence we must not add the in the offset for this call.
  // TODO(nisse): Delete RTCPSender:timestamp_offset_, and see if we can confine
  // knowledge of the offset to a single place.
  if (!rtp_streams_[stream_index].rtp_rtcp->OnSendingRtpFrame(
          encoded_image.Timestamp(), encoded_image.capture_time_ms_,
          rtp_config_.payload_type,
          encoded_image._frameType == VideoFrameType::kVideoFrameKey)) {
    // The payload router could be active but this module isn't sending.
    return Result(Result::ERROR_SEND_FAILED);
  }

  absl::optional<int64_t> expected_retransmission_time_ms;
  if (encoded_image.RetransmissionAllowed()) {
    expected_retransmission_time_ms =
        rtp_streams_[stream_index].rtp_rtcp->ExpectedRetransmissionTimeMs();
  }

  bool send_result = rtp_streams_[stream_index].sender_video->SendVideo(
      encoded_image._frameType, rtp_config_.payload_type, rtp_timestamp,
      encoded_image.capture_time_ms_, encoded_image.data(),
      encoded_image.size(), fragmentation, &rtp_video_header,
      expected_retransmission_time_ms);
  if (frame_count_observer_) {
    FrameCounts& counts = frame_counts_[stream_index];
    if (encoded_image._frameType == VideoFrameType::kVideoFrameKey) {
      ++counts.key_frames;
    } else if (encoded_image._frameType == VideoFrameType::kVideoFrameDelta) {
      ++counts.delta_frames;
    } else {
      RTC_DCHECK(encoded_image._frameType == VideoFrameType::kEmptyFrame);
    }
    frame_count_observer_->FrameCountUpdated(counts,
                                             rtp_config_.ssrcs[stream_index]);
  }
  if (!send_result)
    return Result(Result::ERROR_SEND_FAILED);

  return Result(Result::OK, rtp_timestamp);
}

void RtpVideoSender::OnBitrateAllocationUpdated(
    const VideoBitrateAllocation& bitrate) {
  rtc::CritScope lock(&crit_);
  if (IsActive()) {
    if (rtp_streams_.size() == 1) {
      // If spatial scalability is enabled, it is covered by a single stream.
      rtp_streams_[0].rtp_rtcp->SetVideoBitrateAllocation(bitrate);
    } else {
      std::vector<absl::optional<VideoBitrateAllocation>> layer_bitrates =
          bitrate.GetSimulcastAllocations();
      // Simulcast is in use, split the VideoBitrateAllocation into one struct
      // per rtp stream, moving over the temporal layer allocation.
      for (size_t i = 0; i < rtp_streams_.size(); ++i) {
        // The next spatial layer could be used if the current one is
        // inactive.
        if (layer_bitrates[i]) {
          rtp_streams_[i].rtp_rtcp->SetVideoBitrateAllocation(
              *layer_bitrates[i]);
        } else {
          // Signal a 0 bitrate on a simulcast stream.
          rtp_streams_[i].rtp_rtcp->SetVideoBitrateAllocation(
              VideoBitrateAllocation());
        }
      }
    }
  }
}

void RtpVideoSender::ConfigureProtection() {
  // Consistency of FlexFEC parameters is checked in MaybeCreateFlexfecSender.
  const bool flexfec_enabled = (flexfec_sender_ != nullptr);

  // Consistency of NACK and RED+ULPFEC parameters is checked in this function.
  const bool nack_enabled = rtp_config_.nack.rtp_history_ms > 0;
  int red_payload_type = rtp_config_.ulpfec.red_payload_type;
  int ulpfec_payload_type = rtp_config_.ulpfec.ulpfec_payload_type;

  // Shorthands.
  auto IsRedEnabled = [&]() { return red_payload_type >= 0; };
  auto IsUlpfecEnabled = [&]() { return ulpfec_payload_type >= 0; };
  auto DisableRedAndUlpfec = [&]() {
    red_payload_type = -1;
    ulpfec_payload_type = -1;
  };

  if (webrtc::field_trial::IsEnabled("WebRTC-DisableUlpFecExperiment")) {
    RTC_LOG(LS_INFO) << "Experiment to disable sending ULPFEC is enabled.";
    DisableRedAndUlpfec();
  }

  // If enabled, FlexFEC takes priority over RED+ULPFEC.
  if (flexfec_enabled) {
    if (IsUlpfecEnabled()) {
      RTC_LOG(LS_INFO)
          << "Both FlexFEC and ULPFEC are configured. Disabling ULPFEC.";
    }
    DisableRedAndUlpfec();
  }

  // Payload types without picture ID cannot determine that a stream is complete
  // without retransmitting FEC, so using ULPFEC + NACK for H.264 (for instance)
  // is a waste of bandwidth since FEC packets still have to be transmitted.
  // Note that this is not the case with FlexFEC.
  if (nack_enabled && IsUlpfecEnabled() &&
      !PayloadTypeSupportsSkippingFecPackets(rtp_config_.payload_name)) {
    RTC_LOG(LS_WARNING)
        << "Transmitting payload type without picture ID using "
           "NACK+ULPFEC is a waste of bandwidth since ULPFEC packets "
           "also have to be retransmitted. Disabling ULPFEC.";
    DisableRedAndUlpfec();
  }

  // Verify payload types.
  if (IsUlpfecEnabled() ^ IsRedEnabled()) {
    RTC_LOG(LS_WARNING)
        << "Only RED or only ULPFEC enabled, but not both. Disabling both.";
    DisableRedAndUlpfec();
  }

  for (const RtpStreamSender& stream : rtp_streams_) {
    // Set NACK.
    stream.rtp_rtcp->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);
    // Set RED/ULPFEC information.
    stream.sender_video->SetUlpfecConfig(red_payload_type, ulpfec_payload_type);
  }
}

bool RtpVideoSender::FecEnabled() const {
  const bool flexfec_enabled = (flexfec_sender_ != nullptr);
  const bool ulpfec_enabled =
      !webrtc::field_trial::IsEnabled("WebRTC-DisableUlpFecExperiment") &&
      (rtp_config_.ulpfec.ulpfec_payload_type >= 0);
  return flexfec_enabled || ulpfec_enabled;
}

bool RtpVideoSender::NackEnabled() const {
  const bool nack_enabled = rtp_config_.nack.rtp_history_ms > 0;
  return nack_enabled;
}

uint32_t RtpVideoSender::GetPacketizationOverheadRate() const {
  uint32_t packetization_overhead_bps = 0;
  for (size_t i = 0; i < rtp_streams_.size(); ++i) {
    if (rtp_streams_[i].rtp_rtcp->SendingMedia()) {
      packetization_overhead_bps +=
          rtp_streams_[i].sender_video->PacketizationOverheadBps();
    }
  }
  return packetization_overhead_bps;
}

void RtpVideoSender::DeliverRtcp(const uint8_t* packet, size_t length) {
  // Runs on a network thread.
  for (const RtpStreamSender& stream : rtp_streams_)
    stream.rtp_rtcp->IncomingRtcpPacket(packet, length);
}

void RtpVideoSender::ConfigureSsrcs() {
  // Configure regular SSRCs.
  RTC_CHECK(ssrc_to_rtp_sender_.empty());
  for (size_t i = 0; i < rtp_config_.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_streams_[i].rtp_rtcp.get();

    // Restore RTP state if previous existed.
    auto it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtpState(it->second);

    RTPSender* rtp_sender = rtp_rtcp->RtpSender();
    RTC_DCHECK(rtp_sender != nullptr);
    ssrc_to_rtp_sender_[ssrc] = rtp_sender;
  }

  // Set up RTX if available.
  if (rtp_config_.rtx.ssrcs.empty())
    return;

  RTC_DCHECK_EQ(rtp_config_.rtx.ssrcs.size(), rtp_config_.ssrcs.size());
  for (size_t i = 0; i < rtp_config_.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.rtx.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_streams_[i].rtp_rtcp.get();
    auto it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtxState(it->second);
  }

  // Configure RTX payload types.
  RTC_DCHECK_GE(rtp_config_.rtx.payload_type, 0);
  for (const RtpStreamSender& stream : rtp_streams_) {
    stream.rtp_rtcp->SetRtxSendPayloadType(rtp_config_.rtx.payload_type,
                                           rtp_config_.payload_type);
    stream.rtp_rtcp->SetRtxSendStatus(kRtxRetransmitted |
                                      kRtxRedundantPayloads);
  }
  if (rtp_config_.ulpfec.red_payload_type != -1 &&
      rtp_config_.ulpfec.red_rtx_payload_type != -1) {
    for (const RtpStreamSender& stream : rtp_streams_) {
      stream.rtp_rtcp->SetRtxSendPayloadType(
          rtp_config_.ulpfec.red_rtx_payload_type,
          rtp_config_.ulpfec.red_payload_type);
    }
  }
}

void RtpVideoSender::ConfigureRids() {
  if (rtp_config_.rids.empty())
    return;

  // Some streams could have been disabled, but the rids are still there.
  // This will occur when simulcast has been disabled for a codec (e.g. VP9)
  RTC_DCHECK(rtp_config_.rids.size() >= rtp_streams_.size());
  for (size_t i = 0; i < rtp_streams_.size(); ++i) {
    rtp_streams_[i].rtp_rtcp->SetRid(rtp_config_.rids[i]);
  }
}

void RtpVideoSender::OnNetworkAvailability(bool network_available) {
  for (const RtpStreamSender& stream : rtp_streams_) {
    stream.rtp_rtcp->SetRTCPStatus(network_available ? rtp_config_.rtcp_mode
                                                     : RtcpMode::kOff);
  }
}

std::map<uint32_t, RtpState> RtpVideoSender::GetRtpStates() const {
  std::map<uint32_t, RtpState> rtp_states;

  for (size_t i = 0; i < rtp_config_.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.ssrcs[i];
    RTC_DCHECK_EQ(ssrc, rtp_streams_[i].rtp_rtcp->SSRC());
    rtp_states[ssrc] = rtp_streams_[i].rtp_rtcp->GetRtpState();
  }

  for (size_t i = 0; i < rtp_config_.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = rtp_config_.rtx.ssrcs[i];
    rtp_states[ssrc] = rtp_streams_[i].rtp_rtcp->GetRtxState();
  }

  if (flexfec_sender_) {
    uint32_t ssrc = rtp_config_.flexfec.ssrc;
    rtp_states[ssrc] = flexfec_sender_->GetRtpState();
  }

  return rtp_states;
}

std::map<uint32_t, RtpPayloadState> RtpVideoSender::GetRtpPayloadStates()
    const {
  rtc::CritScope lock(&crit_);
  std::map<uint32_t, RtpPayloadState> payload_states;
  for (const auto& param : params_) {
    payload_states[param.ssrc()] = param.state();
    payload_states[param.ssrc()].shared_frame_id = shared_frame_id_;
  }
  return payload_states;
}

void RtpVideoSender::OnTransportOverheadChanged(
    size_t transport_overhead_bytes_per_packet) {
  rtc::CritScope lock(&crit_);
  transport_overhead_bytes_per_packet_ = transport_overhead_bytes_per_packet;

  size_t max_rtp_packet_size =
      std::min(rtp_config_.max_packet_size,
               kPathMTU - transport_overhead_bytes_per_packet_);
  for (const RtpStreamSender& stream : rtp_streams_) {
    stream.rtp_rtcp->SetMaxRtpPacketSize(max_rtp_packet_size);
  }
}

void RtpVideoSender::OnOverheadChanged(size_t overhead_bytes_per_packet) {
  rtc::CritScope lock(&crit_);
  overhead_bytes_per_packet_ = overhead_bytes_per_packet;
}

void RtpVideoSender::OnBitrateUpdated(uint32_t bitrate_bps,
                                      uint8_t fraction_loss,
                                      int64_t rtt,
                                      int framerate) {
  // Substract overhead from bitrate.
  rtc::CritScope lock(&crit_);
  DataSize packet_overhead = DataSize::bytes(
      overhead_bytes_per_packet_ + transport_overhead_bytes_per_packet_);
  DataSize max_total_packet_size = DataSize::bytes(
      rtp_config_.max_packet_size + transport_overhead_bytes_per_packet_);
  uint32_t payload_bitrate_bps = bitrate_bps;
  if (send_side_bwe_with_overhead_) {
    DataRate overhead_rate = CalculateOverheadRate(
        DataRate::bps(bitrate_bps), max_total_packet_size, packet_overhead);
    // TODO(srte): We probably should not accept 0 payload bitrate here.
    payload_bitrate_bps =
        rtc::saturated_cast<uint32_t>(bitrate_bps - overhead_rate.bps());
  }

  // Get the encoder target rate. It is the estimated network rate -
  // protection overhead.
  encoder_target_rate_bps_ = fec_controller_->UpdateFecRates(
      payload_bitrate_bps, framerate, fraction_loss, loss_mask_vector_, rtt);
  if (!fec_allowed_) {
    encoder_target_rate_bps_ = payload_bitrate_bps;
    // fec_controller_->UpdateFecRates() was still called so as to allow
    // |fec_controller_| to update whatever internal state it might have,
    // since |fec_allowed_| may be toggled back on at any moment.
  }

  uint32_t packetization_rate_bps = 0;
  if (account_for_packetization_overhead_) {
    // Subtract packetization overhead from the encoder target. If target rate
    // is really low, cap the overhead at 50%. This also avoids the case where
    // |encoder_target_rate_bps_| is 0 due to encoder pause event while the
    // packetization rate is positive since packets are still flowing.
    packetization_rate_bps =
        std::min(GetPacketizationOverheadRate(), encoder_target_rate_bps_ / 2);
    encoder_target_rate_bps_ -= packetization_rate_bps;
  }

  loss_mask_vector_.clear();

  uint32_t encoder_overhead_rate_bps = 0;
  if (send_side_bwe_with_overhead_) {
    // TODO(srte): The packet size should probably be the same as in the
    // CalculateOverheadRate call above (just max_total_packet_size), it doesn't
    // make sense to use different packet rates for different overhead
    // calculations.
    DataRate encoder_overhead_rate = CalculateOverheadRate(
        DataRate::bps(encoder_target_rate_bps_),
        max_total_packet_size - DataSize::bytes(overhead_bytes_per_packet_),
        packet_overhead);
    encoder_overhead_rate_bps =
        std::min(encoder_overhead_rate.bps<uint32_t>(),
                 bitrate_bps - encoder_target_rate_bps_);
  }
  // When the field trial "WebRTC-SendSideBwe-WithOverhead" is enabled
  // protection_bitrate includes overhead.
  const uint32_t media_rate = encoder_target_rate_bps_ +
                              encoder_overhead_rate_bps +
                              packetization_rate_bps;
  RTC_DCHECK_GE(bitrate_bps, media_rate);
  protection_bitrate_bps_ = bitrate_bps - media_rate;
}

uint32_t RtpVideoSender::GetPayloadBitrateBps() const {
  return encoder_target_rate_bps_;
}

uint32_t RtpVideoSender::GetProtectionBitrateBps() const {
  return protection_bitrate_bps_;
}

std::vector<RtpSequenceNumberMap::Info> RtpVideoSender::GetSentRtpPacketInfos(
    uint32_t ssrc,
    rtc::ArrayView<const uint16_t> sequence_numbers) const {
  for (const auto& rtp_stream : rtp_streams_) {
    if (ssrc == rtp_stream.rtp_rtcp->SSRC()) {
      return rtp_stream.sender_video->GetSentRtpPacketInfos(sequence_numbers);
    }
  }
  return std::vector<RtpSequenceNumberMap::Info>();
}

int RtpVideoSender::ProtectionRequest(const FecProtectionParams* delta_params,
                                      const FecProtectionParams* key_params,
                                      uint32_t* sent_video_rate_bps,
                                      uint32_t* sent_nack_rate_bps,
                                      uint32_t* sent_fec_rate_bps) {
  *sent_video_rate_bps = 0;
  *sent_nack_rate_bps = 0;
  *sent_fec_rate_bps = 0;
  for (const RtpStreamSender& stream : rtp_streams_) {
    uint32_t not_used = 0;
    uint32_t module_nack_rate = 0;
    stream.sender_video->SetFecParameters(*delta_params, *key_params);
    *sent_video_rate_bps += stream.sender_video->VideoBitrateSent();
    *sent_fec_rate_bps += stream.sender_video->FecOverheadRate();
    stream.rtp_rtcp->BitrateSent(&not_used, /*video_rate=*/nullptr,
                                 /*fec_rate=*/nullptr, &module_nack_rate);
    *sent_nack_rate_bps += module_nack_rate;
  }
  return 0;
}

void RtpVideoSender::SetFecAllowed(bool fec_allowed) {
  rtc::CritScope cs(&crit_);
  fec_allowed_ = fec_allowed;
}

void RtpVideoSender::OnPacketFeedbackVector(
    const std::vector<PacketFeedback>& packet_feedback_vector) {
  if (fec_controller_->UseLossVectorMask()) {
    rtc::CritScope cs(&crit_);
    for (const PacketFeedback& packet : packet_feedback_vector) {
      if (packet.send_time_ms == PacketFeedback::kNoSendTime || !packet.ssrc ||
          absl::c_find(rtp_config_.ssrcs, *packet.ssrc) ==
              rtp_config_.ssrcs.end()) {
        // If packet send time is missing, the feedback for this packet has
        // probably already been processed, so ignore it.
        // If packet does not belong to a registered media ssrc, we are also
        // not interested in it.
        continue;
      }
      loss_mask_vector_.push_back(packet.arrival_time_ms ==
                                  PacketFeedback::kNotReceived);
    }
  }

  // Map from SSRC to all acked packets for that RTP module.
  std::map<uint32_t, std::vector<uint16_t>> acked_packets_per_ssrc;
  for (const PacketFeedback& packet : packet_feedback_vector) {
    if (packet.ssrc && packet.arrival_time_ms != PacketFeedback::kNotReceived) {
      acked_packets_per_ssrc[*packet.ssrc].push_back(
          packet.rtp_sequence_number);
    }
  }

  if (use_early_loss_detection_) {
    // Map from SSRC to vector of RTP sequence numbers that are indicated as
    // lost by feedback, without being trailed by any received packets.
    std::map<uint32_t, std::vector<uint16_t>> early_loss_detected_per_ssrc;

    for (const PacketFeedback& packet : packet_feedback_vector) {
      if (packet.send_time_ms == PacketFeedback::kNoSendTime || !packet.ssrc ||
          absl::c_find(rtp_config_.ssrcs, *packet.ssrc) ==
              rtp_config_.ssrcs.end()) {
        // If packet send time is missing, the feedback for this packet has
        // probably already been processed, so ignore it.
        // If packet does not belong to a registered media ssrc, we are also
        // not interested in it.
        continue;
      }

      if (packet.arrival_time_ms == PacketFeedback::kNotReceived) {
        // Last known lost packet, might not be detectable as lost by remote
        // jitter buffer.
        early_loss_detected_per_ssrc[*packet.ssrc].push_back(
            packet.rtp_sequence_number);
      } else {
        // Packet received, so any loss prior to this is already detectable.
        early_loss_detected_per_ssrc.erase(*packet.ssrc);
      }
    }

    for (const auto& kv : early_loss_detected_per_ssrc) {
      const uint32_t ssrc = kv.first;
      auto it = ssrc_to_rtp_sender_.find(ssrc);
      RTC_DCHECK(it != ssrc_to_rtp_sender_.end());
      RTPSender* rtp_sender = it->second;
      for (uint16_t sequence_number : kv.second) {
        rtp_sender->ReSendPacket(sequence_number);
      }
    }
  }

  for (const auto& kv : acked_packets_per_ssrc) {
    const uint32_t ssrc = kv.first;
    auto it = ssrc_to_rtp_sender_.find(ssrc);
    if (it == ssrc_to_rtp_sender_.end()) {
      // Packets not for a media SSRC, so likely RTX or FEC. If so, ignore
      // since there's no RTP history to clean up anyway.
      continue;
    }
    rtc::ArrayView<const uint16_t> rtp_sequence_numbers(kv.second);
    it->second->OnPacketsAcknowledged(rtp_sequence_numbers);
  }
}

void RtpVideoSender::SetEncodingData(size_t width,
                                     size_t height,
                                     size_t num_temporal_layers) {
  fec_controller_->SetEncodingData(width, height, num_temporal_layers,
                                   rtp_config_.max_packet_size);
}
}  // namespace webrtc
