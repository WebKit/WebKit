/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "call/rtp_transport_controller_send.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/rtp_video_sender.h"
#include "logging/rtc_event_log/events/rtc_event_route_change.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/rate_limiter.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
static const int64_t kRetransmitWindowSizeMs = 500;
static const size_t kMaxOverheadBytes = 500;

constexpr TimeDelta kPacerQueueUpdateInterval = TimeDelta::Millis<25>();

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         int start_bitrate_bps,
                                         Clock* clock) {
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  if (start_bitrate_bps > 0)
    msg.starting_rate = DataRate::bps(start_bitrate_bps);
  return msg;
}

TargetRateConstraints ConvertConstraints(const BitrateConstraints& contraints,
                                         Clock* clock) {
  return ConvertConstraints(contraints.min_bitrate_bps,
                            contraints.max_bitrate_bps,
                            contraints.start_bitrate_bps, clock);
}
}  // namespace

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    NetworkStatePredictorFactoryInterface* predictor_factory,
    NetworkControllerFactoryInterface* controller_factory,
    const BitrateConstraints& bitrate_config,
    std::unique_ptr<ProcessThread> process_thread,
    TaskQueueFactory* task_queue_factory)
    : clock_(clock),
      event_log_(event_log),
      bitrate_configurator_(bitrate_config),
      process_thread_(std::move(process_thread)),
      pacer_(clock, &packet_router_, event_log, nullptr, process_thread_.get()),
      observer_(nullptr),
      controller_factory_override_(controller_factory),
      controller_factory_fallback_(
          absl::make_unique<GoogCcNetworkControllerFactory>(predictor_factory)),
      process_interval_(controller_factory_fallback_->GetProcessInterval()),
      last_report_block_time_(Timestamp::ms(clock_->TimeInMilliseconds())),
      reset_feedback_on_route_change_(
          !field_trial::IsEnabled("WebRTC-Bwe-NoFeedbackReset")),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      add_pacing_to_cwin_(
          field_trial::IsEnabled("WebRTC-AddPacingToCongestionWindowPushback")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(false),
      retransmission_rate_limiter_(clock, kRetransmitWindowSizeMs),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "rtp_send_controller",
          TaskQueueFactory::Priority::NORMAL)) {
  initial_config_.constraints = ConvertConstraints(bitrate_config, clock_);
  initial_config_.event_log = event_log;
  initial_config_.key_value_config = &trial_based_config_;
  RTC_DCHECK(bitrate_config.start_bitrate_bps > 0);

  pacer()->SetPacingRates(DataRate::bps(bitrate_config.start_bitrate_bps),
                          DataRate::Zero());

  process_thread_->Start();
}

RtpTransportControllerSend::~RtpTransportControllerSend() {
  process_thread_->Stop();
}

RtpVideoSenderInterface* RtpTransportControllerSend::CreateRtpVideoSender(
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
      clock_, suspended_ssrcs, states, rtp_config, rtcp_report_interval_ms,
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

void RtpTransportControllerSend::UpdateControlState() {
  absl::optional<TargetTransferRate> update = control_handler_->GetUpdate();
  if (!update)
    return;
  retransmission_rate_limiter_.SetMaxRate(
      update->network_estimate.bandwidth.bps());
  // We won't create control_handler_ until we have an observers.
  RTC_DCHECK(observer_ != nullptr);
  observer_->OnTargetTransferRate(*update);
}

RtpPacketPacer* RtpTransportControllerSend::pacer() {
  // TODO(bugs.webrtc.org/10809): Return reference to the correct
  // pacer implementation.
  return &pacer_;
}

const RtpPacketPacer* RtpTransportControllerSend::pacer() const {
  // TODO(bugs.webrtc.org/10809): Return reference to the correct
  // pacer implementation.
  return &pacer_;
}

rtc::TaskQueue* RtpTransportControllerSend::GetWorkerQueue() {
  return &task_queue_;
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

NetworkStateEstimateObserver*
RtpTransportControllerSend::network_state_estimate_observer() {
  return this;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return this;
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  // TODO(bugs.webrtc.org/10809): Return reference to the correct
  // pacer implementation.
  return &pacer_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps,
    int max_total_bitrate_bps) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  streams_config_.min_total_allocated_bitrate =
      DataRate::bps(min_send_bitrate_bps);
  streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
  streams_config_.max_total_allocated_bitrate =
      DataRate::bps(max_total_bitrate_bps);
  UpdateStreamsConfig();
}
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  streams_config_.pacing_factor = pacing_factor;
  UpdateStreamsConfig();
}
void RtpTransportControllerSend::SetQueueTimeLimit(int limit_ms) {
  pacer()->SetQueueTimeLimit(TimeDelta::ms(limit_ms));
}
void RtpTransportControllerSend::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void RtpTransportControllerSend::RegisterTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
  task_queue_.PostTask([this, observer] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
    observer_->OnStartRateUpdate(*initial_config_.constraints.starting_rate);
    MaybeCreateControllers();
  });
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
  if (kv->second.connected != network_route.connected ||
      kv->second.local_network_id != network_route.local_network_id ||
      kv->second.remote_network_id != network_route.remote_network_id) {
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

    if (reset_feedback_on_route_change_)
      transport_feedback_adapter_.SetNetworkIds(
          network_route.local_network_id, network_route.remote_network_id);
    transport_overhead_bytes_per_packet_ = network_route.packet_overhead;

    if (event_log_) {
      event_log_->Log(absl::make_unique<RtcEventRouteChange>(
          network_route.connected, network_route.packet_overhead));
    }
    NetworkRouteChange msg;
    msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
    msg.constraints = ConvertConstraints(bitrate_config, clock_);
    task_queue_.PostTask([this, msg] {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        PostUpdates(controller_->OnNetworkRouteChange(msg));
      } else {
        UpdateInitialConstraints(msg.constraints);
      }
      pacer()->UpdateOutstandingData(DataSize::Zero());
    });
  }
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (network_available ? "Up" : "Down");
  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = network_available;
  task_queue_.PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (network_available_ == msg.network_available)
      return;
    network_available_ = msg.network_available;
    if (network_available_) {
      pacer()->Resume();
    } else {
      pacer()->Pause();
    }
    pacer()->UpdateOutstandingData(DataSize::Zero());

    if (controller_) {
      control_handler_->SetNetworkAvailability(network_available_);
      PostUpdates(controller_->OnNetworkAvailability(msg));
      UpdateControlState();
    } else {
      MaybeCreateControllers();
    }
  });

  for (auto& rtp_sender : video_rtp_senders_) {
    rtp_sender->OnNetworkAvailability(network_available);
  }
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return this;
}
int64_t RtpTransportControllerSend::GetPacerQueuingDelayMs() const {
  return pacer()->OldestPacketWaitTime().ms();
}
absl::optional<Timestamp> RtpTransportControllerSend::GetFirstPacketTime()
    const {
  return pacer()->FirstSentPacketTime();
}
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  task_queue_.PostTask([this, enable]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    streams_config_.requests_alr_probing = enable;
    UpdateStreamsConfig();
  });
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  absl::optional<SentPacket> packet_msg =
      transport_feedback_adapter_.ProcessSentPacket(sent_packet);
  if (packet_msg) {
    task_queue_.PostTask([this, packet_msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_)
        PostUpdates(controller_->OnSentPacket(*packet_msg));
    });
  }
  pacer()->UpdateOutstandingData(
      transport_feedback_adapter_.GetOutstandingData());
}

void RtpTransportControllerSend::OnReceivedPacket(
    const ReceivedPacket& packet_msg) {
  task_queue_.PostTask([this, packet_msg]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (controller_)
      PostUpdates(controller_->OnReceivedPacket(packet_msg));
  });
}

void RtpTransportControllerSend::SetSdpBitrateParameters(
    const BitrateConstraints& constraints) {
  absl::optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithSdpParameters(constraints);
  if (updated.has_value()) {
    TargetRateConstraints msg = ConvertConstraints(*updated, clock_);
    task_queue_.PostTask([this, msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        PostUpdates(controller_->OnTargetRateConstraints(msg));
      } else {
        UpdateInitialConstraints(msg);
      }
    });
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
    TargetRateConstraints msg = ConvertConstraints(*updated, clock_);
    task_queue_.PostTask([this, msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        PostUpdates(controller_->OnTargetRateConstraints(msg));
      } else {
        UpdateInitialConstraints(msg);
      }
    });
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetClientBitratePreferences: "
        << "nothing to update";
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

void RtpTransportControllerSend::AccountForAudioPacketsInPacedSender(
    bool account_for_audio) {
  pacer()->SetAccountForAudioPackets(account_for_audio);
}

void RtpTransportControllerSend::OnReceivedEstimatedBitrate(uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  task_queue_.PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (controller_)
      PostUpdates(controller_->OnRemoteBitrateReport(msg));
  });
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReport(
    const ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  task_queue_.PostTask([this, report_blocks, now_ms]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);
  });

  task_queue_.PostTask([this, now_ms, rtt_ms]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RoundTripTimeUpdate report;
    report.receive_time = Timestamp::ms(now_ms);
    report.round_trip_time = TimeDelta::ms(rtt_ms);
    report.smoothed = false;
    if (controller_ && !report.round_trip_time.IsZero())
      PostUpdates(controller_->OnRoundTripTimeUpdate(report));
  });
}

void RtpTransportControllerSend::OnAddPacket(
    const RtpPacketSendInfo& packet_info) {
  transport_feedback_adapter_.AddPacket(
      packet_info,
      send_side_bwe_with_overhead_ ? transport_overhead_bytes_per_packet_.load()
                                   : 0,
      Timestamp::ms(clock_->TimeInMilliseconds()));
}

void RtpTransportControllerSend::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);

  absl::optional<TransportPacketsFeedback> feedback_msg =
      transport_feedback_adapter_.ProcessTransportFeedback(
          feedback, Timestamp::ms(clock_->TimeInMilliseconds()));
  if (feedback_msg) {
    task_queue_.PostTask([this, feedback_msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_)
        PostUpdates(controller_->OnTransportPacketsFeedback(*feedback_msg));
    });
  }
  pacer()->UpdateOutstandingData(
      transport_feedback_adapter_.GetOutstandingData());
}

void RtpTransportControllerSend::OnRemoteNetworkEstimate(
    NetworkStateEstimate estimate) {
  estimate.update_time = Timestamp::ms(clock_->TimeInMilliseconds());
  task_queue_.PostTask([this, estimate] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (controller_)
      controller_->OnNetworkStateEstimate(estimate);
  });
}

void RtpTransportControllerSend::MaybeCreateControllers() {
  RTC_DCHECK(!controller_);
  RTC_DCHECK(!control_handler_);

  if (!network_available_ || !observer_)
    return;
  control_handler_ = absl::make_unique<CongestionControlHandler>();

  initial_config_.constraints.at_time =
      Timestamp::ms(clock_->TimeInMilliseconds());
  initial_config_.stream_based_config = streams_config_;

  // TODO(srte): Use fallback controller if no feedback is available.
  if (controller_factory_override_) {
    RTC_LOG(LS_INFO) << "Creating overridden congestion controller";
    controller_ = controller_factory_override_->Create(initial_config_);
    process_interval_ = controller_factory_override_->GetProcessInterval();
  } else {
    RTC_LOG(LS_INFO) << "Creating fallback congestion controller";
    controller_ = controller_factory_fallback_->Create(initial_config_);
    process_interval_ = controller_factory_fallback_->GetProcessInterval();
  }
  UpdateControllerWithTimeInterval();
  StartProcessPeriodicTasks();
}

void RtpTransportControllerSend::UpdateInitialConstraints(
    TargetRateConstraints new_contraints) {
  if (!new_contraints.starting_rate)
    new_contraints.starting_rate = initial_config_.constraints.starting_rate;
  RTC_DCHECK(new_contraints.starting_rate);
  initial_config_.constraints = new_contraints;
}

void RtpTransportControllerSend::StartProcessPeriodicTasks() {
  if (!pacer_queue_update_task_.Running()) {
    pacer_queue_update_task_ = RepeatingTaskHandle::DelayedStart(
        task_queue_.Get(), kPacerQueueUpdateInterval, [this]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          TimeDelta expected_queue_time = pacer()->ExpectedQueueTime();
          control_handler_->SetPacerQueue(expected_queue_time);
          UpdateControlState();
          return kPacerQueueUpdateInterval;
        });
  }
  controller_task_.Stop();
  if (process_interval_.IsFinite()) {
    controller_task_ = RepeatingTaskHandle::DelayedStart(
        task_queue_.Get(), process_interval_, [this]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          UpdateControllerWithTimeInterval();
          return process_interval_;
        });
  }
}

void RtpTransportControllerSend::UpdateControllerWithTimeInterval() {
  RTC_DCHECK(controller_);
  ProcessInterval msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  if (add_pacing_to_cwin_)
    msg.pacer_queue = pacer()->QueueSizeData();
  PostUpdates(controller_->OnProcessInterval(msg));
}

void RtpTransportControllerSend::UpdateStreamsConfig() {
  streams_config_.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  if (controller_)
    PostUpdates(controller_->OnStreamsConfig(streams_config_));
}

void RtpTransportControllerSend::PostUpdates(NetworkControlUpdate update) {
  if (update.congestion_window) {
    pacer()->SetCongestionWindow(*update.congestion_window);
  }
  if (update.pacer_config) {
    pacer()->SetPacingRates(update.pacer_config->data_rate(),
                            update.pacer_config->pad_rate());
  }
  for (const auto& probe : update.probe_cluster_configs) {
    pacer()->CreateProbeCluster(probe.target_data_rate, probe.id);
  }
  if (update.target_rate) {
    control_handler_->SetTargetRate(*update.target_rate);
    UpdateControlState();
  }
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReportBlocks(
    const ReportBlockList& report_blocks,
    int64_t now_ms) {
  if (report_blocks.empty())
    return;

  int total_packets_lost_delta = 0;
  int total_packets_delta = 0;

  // Compute the packet loss from all report blocks.
  for (const RTCPReportBlock& report_block : report_blocks) {
    auto it = last_report_blocks_.find(report_block.source_ssrc);
    if (it != last_report_blocks_.end()) {
      auto number_of_packets = report_block.extended_highest_sequence_number -
                               it->second.extended_highest_sequence_number;
      total_packets_delta += number_of_packets;
      auto lost_delta = report_block.packets_lost - it->second.packets_lost;
      total_packets_lost_delta += lost_delta;
    }
    last_report_blocks_[report_block.source_ssrc] = report_block;
  }
  // Can only compute delta if there has been previous blocks to compare to. If
  // not, total_packets_delta will be unchanged and there's nothing more to do.
  if (!total_packets_delta)
    return;
  int packets_received_delta = total_packets_delta - total_packets_lost_delta;
  // To detect lost packets, at least one packet has to be received. This check
  // is needed to avoid bandwith detection update in
  // VideoSendStreamTest.SuspendBelowMinBitrate

  if (packets_received_delta < 1)
    return;
  Timestamp now = Timestamp::ms(now_ms);
  TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;
  if (controller_)
    PostUpdates(controller_->OnTransportLossReport(msg));
  last_report_block_time_ = now;
}

}  // namespace webrtc
