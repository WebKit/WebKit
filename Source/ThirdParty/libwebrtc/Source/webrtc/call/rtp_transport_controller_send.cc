/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "call/rtp_transport_controller_send.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/congestion_controller/rtp/include/send_side_congestion_controller.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/rate_limiter.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
static const int64_t kRetransmitWindowSizeMs = 500;
static const size_t kMaxOverheadBytes = 500;
const char kTaskQueueExperiment[] = "WebRTC-TaskQueueCongestionControl";
using TaskQueueController = webrtc::webrtc_cc::SendSideCongestionController;

std::unique_ptr<SendSideCongestionControllerInterface> CreateController(
    Clock* clock,
    rtc::TaskQueue* task_queue,
    webrtc::RtcEventLog* event_log,
    PacedSender* pacer,
    const BitrateConstraints& bitrate_config,
    bool task_queue_controller,
    NetworkControllerFactoryInterface* controller_factory) {
  if (task_queue_controller) {
    RTC_LOG(LS_INFO) << "Using TaskQueue based SSCC";
    return absl::make_unique<webrtc::webrtc_cc::SendSideCongestionController>(
        clock, task_queue, event_log, pacer, bitrate_config.start_bitrate_bps,
        bitrate_config.min_bitrate_bps, bitrate_config.max_bitrate_bps,
        controller_factory);
  }
  RTC_LOG(LS_INFO) << "Using Legacy SSCC";
  auto cc = absl::make_unique<webrtc::SendSideCongestionController>(
      clock, nullptr /* observer */, event_log, pacer);
  cc->SignalNetworkState(kNetworkDown);
  cc->SetBweBitrates(bitrate_config.min_bitrate_bps,
                     bitrate_config.start_bitrate_bps,
                     bitrate_config.max_bitrate_bps);
  return std::move(cc);
}
}  // namespace

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    NetworkControllerFactoryInterface* controller_factory,
    const BitrateConstraints& bitrate_config)
    : clock_(clock),
      pacer_(clock, &packet_router_, event_log),
      bitrate_configurator_(bitrate_config),
      process_thread_(ProcessThread::Create("SendControllerThread")),
      observer_(nullptr),
      retransmission_rate_limiter_(clock, kRetransmitWindowSizeMs),
      task_queue_("rtp_send_controller") {
  // Created after task_queue to be able to post to the task queue internally.
  send_side_cc_ = CreateController(
      clock, &task_queue_, event_log, &pacer_, bitrate_config,
      !field_trial::IsDisabled(kTaskQueueExperiment), controller_factory);

  process_thread_->RegisterModule(&pacer_, RTC_FROM_HERE);
  process_thread_->RegisterModule(send_side_cc_.get(), RTC_FROM_HERE);
  process_thread_->Start();
}

RtpTransportControllerSend::~RtpTransportControllerSend() {
  process_thread_->Stop();
  process_thread_->DeRegisterModule(send_side_cc_.get());
  process_thread_->DeRegisterModule(&pacer_);
}

RtpVideoSenderInterface* RtpTransportControllerSend::CreateRtpVideoSender(
    const std::vector<uint32_t>& ssrcs,
    std::map<uint32_t, RtpState> suspended_ssrcs,
    const std::map<uint32_t, RtpPayloadState>& states,
    const RtpConfig& rtp_config,
    int rtcp_report_interval_ms,
    Transport* send_transport,
    const RtpSenderObservers& observers,
    RtcEventLog* event_log,
    std::unique_ptr<FecController> fec_controller,
    const RtpSenderFrameEncryptionConfig& frame_encryption_config) {
  video_rtp_senders_.push_back(absl::make_unique<RtpVideoSender>(
      ssrcs, suspended_ssrcs, states, rtp_config, rtcp_report_interval_ms,
      send_transport, observers,
      // TODO(holmer): Remove this circular dependency by injecting
      // the parts of RtpTransportControllerSendInterface that are really used.
      this, event_log, &retransmission_rate_limiter_, std::move(fec_controller),
      frame_encryption_config.frame_encryptor,
      frame_encryption_config.crypto_options));
  return video_rtp_senders_.back().get();
}

void RtpTransportControllerSend::DestroyRtpVideoSender(
    RtpVideoSenderInterface* rtp_video_sender) {
  std::vector<std::unique_ptr<RtpVideoSenderInterface>>::iterator it =
      video_rtp_senders_.end();
  for (it = video_rtp_senders_.begin(); it != video_rtp_senders_.end(); ++it) {
    if (it->get() == rtp_video_sender) {
      break;
    }
  }
  RTC_DCHECK(it != video_rtp_senders_.end());
  video_rtp_senders_.erase(it);
}

void RtpTransportControllerSend::OnNetworkChanged(uint32_t bitrate_bps,
                                                  uint8_t fraction_loss,
                                                  int64_t rtt_ms,
                                                  int64_t probing_interval_ms) {
  // TODO(srte): Skip this step when old SendSideCongestionController is
  // deprecated.
  TargetTransferRate msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.target_rate = DataRate::bps(bitrate_bps);
  msg.network_estimate.at_time = msg.at_time;
  msg.network_estimate.bwe_period = TimeDelta::ms(probing_interval_ms);
  uint32_t bandwidth_bps;
  if (send_side_cc_->AvailableBandwidth(&bandwidth_bps))
    msg.network_estimate.bandwidth = DataRate::bps(bandwidth_bps);
  msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0;
  msg.network_estimate.round_trip_time = TimeDelta::ms(rtt_ms);

  retransmission_rate_limiter_.SetMaxRate(bandwidth_bps);

  if (!task_queue_.IsCurrent()) {
    task_queue_.PostTask([this, msg] {
      rtc::CritScope cs(&observer_crit_);
      // We won't register as observer until we have an observers.
      RTC_DCHECK(observer_ != nullptr);
      observer_->OnTargetTransferRate(msg);
    });
  } else {
    rtc::CritScope cs(&observer_crit_);
    // We won't register as observer until we have an observers.
    RTC_DCHECK(observer_ != nullptr);
    observer_->OnTargetTransferRate(msg);
  }
}

rtc::TaskQueue* RtpTransportControllerSend::GetWorkerQueue() {
  return &task_queue_;
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return send_side_cc_.get();
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpTransportControllerSend::keepalive_config() const {
  return keepalive_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps,
    int max_total_bitrate_bps) {
  send_side_cc_->SetAllocatedSendBitrateLimits(
      min_send_bitrate_bps, max_padding_bitrate_bps, max_total_bitrate_bps);
}

void RtpTransportControllerSend::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  send_side_cc_->SetPacingFactor(pacing_factor);
}
void RtpTransportControllerSend::SetQueueTimeLimit(int limit_ms) {
  pacer_.SetQueueTimeLimit(limit_ms);
}
CallStatsObserver* RtpTransportControllerSend::GetCallStatsObserver() {
  return send_side_cc_.get();
}
void RtpTransportControllerSend::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_->RegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_->DeRegisterPacketFeedbackObserver(observer);
}

void RtpTransportControllerSend::RegisterTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
  {
    rtc::CritScope cs(&observer_crit_);
    RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
  }
  send_side_cc_->RegisterNetworkObserver(this);
}
void RtpTransportControllerSend::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  // Check if the network route is connected.
  if (!network_route.connected) {
    RTC_LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
    // TODO(honghaiz): Perhaps handle this in SignalChannelNetworkState and
    // consider merging these two methods.
    return;
  }

  // Check whether the network route has changed on each transport.
  auto result =
      network_routes_.insert(std::make_pair(transport_name, network_route));
  auto kv = result.first;
  bool inserted = result.second;
  if (inserted) {
    // No need to reset BWE if this is the first time the network connects.
    return;
  }
  if (kv->second != network_route) {
    kv->second = network_route;
    BitrateConstraints bitrate_config = bitrate_configurator_.GetConfig();
    RTC_LOG(LS_INFO) << "Network route changed on transport " << transport_name
                     << ": new local network id "
                     << network_route.local_network_id
                     << " new remote network id "
                     << network_route.remote_network_id
                     << " Reset bitrates to min: "
                     << bitrate_config.min_bitrate_bps
                     << " bps, start: " << bitrate_config.start_bitrate_bps
                     << " bps,  max: " << bitrate_config.max_bitrate_bps
                     << " bps.";
    RTC_DCHECK_GT(bitrate_config.start_bitrate_bps, 0);
    send_side_cc_->OnNetworkRouteChanged(
        network_route, bitrate_config.start_bitrate_bps,
        bitrate_config.min_bitrate_bps, bitrate_config.max_bitrate_bps);
  }
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  send_side_cc_->SignalNetworkState(network_available ? kNetworkUp
                                                      : kNetworkDown);
  for (auto& rtp_sender : video_rtp_senders_) {
    rtp_sender->OnNetworkAvailability(network_available);
  }
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return send_side_cc_->GetBandwidthObserver();
}
int64_t RtpTransportControllerSend::GetPacerQueuingDelayMs() const {
  return pacer_.QueueInMs();
}
int64_t RtpTransportControllerSend::GetFirstPacketTimeMs() const {
  return pacer_.FirstSentPacketTimeMs();
}
void RtpTransportControllerSend::SetPerPacketFeedbackAvailable(bool available) {
  send_side_cc_->SetPerPacketFeedbackAvailable(available);
}
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  send_side_cc_->EnablePeriodicAlrProbing(enable);
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  send_side_cc_->OnSentPacket(sent_packet);
}

void RtpTransportControllerSend::SetSdpBitrateParameters(
    const BitrateConstraints& constraints) {
  absl::optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithSdpParameters(constraints);
  if (updated.has_value()) {
    send_side_cc_->SetBweBitrates(updated->min_bitrate_bps,
                                  updated->start_bitrate_bps,
                                  updated->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetSdpBitrateParameters: "
        << "nothing to update";
  }
}

void RtpTransportControllerSend::SetClientBitratePreferences(
    const BitrateSettings& preferences) {
  absl::optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithClientPreferences(preferences);
  if (updated.has_value()) {
    send_side_cc_->SetBweBitrates(updated->min_bitrate_bps,
                                  updated->start_bitrate_bps,
                                  updated->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetClientBitratePreferences: "
        << "nothing to update";
  }
}

void RtpTransportControllerSend::SetAllocatedBitrateWithoutFeedback(
    uint32_t bitrate_bps) {
  // Audio transport feedback will not be reported in this mode, instead update
  // acknowledged bitrate estimator with the bitrate allocated for audio.
  if (field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")) {
    // TODO(srte): Make sure it's safe to always report this and remove the
    // field trial check.
    send_side_cc_->SetAllocatedBitrateWithoutFeedback(bitrate_bps);
  }
}

void RtpTransportControllerSend::OnTransportOverheadChanged(
    size_t transport_overhead_bytes_per_packet) {
  if (transport_overhead_bytes_per_packet >= kMaxOverheadBytes) {
    RTC_LOG(LS_ERROR) << "Transport overhead exceeds " << kMaxOverheadBytes;
    return;
  }

  // TODO(holmer): Call AudioRtpSenders when they have been moved to
  // RtpTransportControllerSend.
  for (auto& rtp_video_sender : video_rtp_senders_) {
    rtp_video_sender->OnTransportOverheadChanged(
        transport_overhead_bytes_per_packet);
  }
}
}  // namespace webrtc
