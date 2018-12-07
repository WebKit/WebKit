/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/include/send_side_congestion_controller.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>
#include "absl/memory/memory.h"
#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_types.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

using absl::make_unique;

namespace webrtc {
namespace webrtc_cc {
namespace {
using send_side_cc_internal::PeriodicTask;
const int64_t PacerQueueUpdateIntervalMs = 25;

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         int start_bitrate_bps,
                                         const Clock* clock) {
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

// The template closure pattern is based on rtc::ClosureTask.
template <class Closure>
class PeriodicTaskImpl final : public PeriodicTask {
 public:
  PeriodicTaskImpl(rtc::TaskQueue* task_queue,
                   int64_t period_ms,
                   Closure&& closure)
      : task_queue_(task_queue),
        period_ms_(period_ms),
        closure_(std::forward<Closure>(closure)) {}
  bool Run() override {
    if (!running_)
      return true;
    closure_();
    // absl::WrapUnique lets us repost this task on the TaskQueue.
    task_queue_->PostDelayedTask(absl::WrapUnique(this), period_ms_);
    // Return false to tell TaskQueue to not destruct this object, since we have
    // taken ownership with absl::WrapUnique.
    return false;
  }
  void Stop() override {
    if (task_queue_->IsCurrent()) {
      RTC_DCHECK(running_);
      running_ = false;
    } else {
      task_queue_->PostTask([this] { Stop(); });
    }
  }

 private:
  rtc::TaskQueue* const task_queue_;
  const int64_t period_ms_;
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
  bool running_ = true;
};

template <class Closure>
static PeriodicTask* StartPeriodicTask(rtc::TaskQueue* task_queue,
                                       int64_t period_ms,
                                       Closure&& closure) {
  auto periodic_task = absl::make_unique<PeriodicTaskImpl<Closure>>(
      task_queue, period_ms, std::forward<Closure>(closure));
  PeriodicTask* periodic_task_ptr = periodic_task.get();
  task_queue->PostDelayedTask(std::move(periodic_task), period_ms);
  return periodic_task_ptr;
}

}  // namespace

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    rtc::TaskQueue* task_queue,
    RtcEventLog* event_log,
    PacedSender* pacer,
    int start_bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps,
    NetworkControllerFactoryInterface* controller_factory)
    : clock_(clock),
      pacer_(pacer),
      transport_feedback_adapter_(clock_),
      controller_factory_with_feedback_(controller_factory),
      controller_factory_fallback_(
          absl::make_unique<GoogCcNetworkControllerFactory>(event_log)),
      process_interval_(controller_factory_fallback_->GetProcessInterval()),
      last_report_block_time_(Timestamp::ms(clock_->TimeInMilliseconds())),
      observer_(nullptr),
      reset_feedback_on_route_change_(
          !field_trial::IsEnabled("WebRTC-Bwe-NoFeedbackReset")),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(false),
      periodic_tasks_enabled_(true),
      packet_feedback_available_(false),
      pacer_queue_update_task_(nullptr),
      controller_task_(nullptr),
      task_queue_(task_queue) {
  initial_config_.constraints = ConvertConstraints(
      min_bitrate_bps, max_bitrate_bps, start_bitrate_bps, clock_);
  RTC_DCHECK(start_bitrate_bps > 0);
  // To be fully compatible with legacy SendSideCongestionController, make sure
  // pacer is initialized even if there are no registered streams. This should
  // not happen under normal circumstances, but some tests rely on it and there
  // are no checks detecting when the legacy SendSideCongestionController is
  // used. This way of setting the value has the drawback that it might be wrong
  // compared to what the actual value from the congestion controller will be.
  // TODO(srte): Remove this when the legacy SendSideCongestionController is
  // removed.
  pacer_->SetEstimatedBitrate(start_bitrate_bps);
}

// There is no point in having a network controller for a network that is not
// yet available and if we don't have any observer of it's state.
// MaybeCreateControllers is used to trigger creation if those things are
// fulfilled. This is needed since dependent code uses the period until network
// is signalled to be avaliabile to set the expected start bitrate which is sent
// to the initializer for NetworkControllers. The observer is injected later due
// to a circular dependency between RtpTransportControllerSend and Call.
// TODO(srte): Break the circular dependency issue and make sure that starting
// bandwidth is set before this class is initialized so the controllers can be
// created in the constructor.
void SendSideCongestionController::MaybeCreateControllers() {
  if (!controller_)
    MaybeRecreateControllers();
}

void SendSideCongestionController::MaybeRecreateControllers() {
  if (!network_available_ || !observer_)
    return;
  if (!control_handler_) {
    control_handler_ =
        absl::make_unique<CongestionControlHandler>(observer_, pacer_);
  }

  initial_config_.constraints.at_time =
      Timestamp::ms(clock_->TimeInMilliseconds());
  initial_config_.stream_based_config = streams_config_;

  if (!controller_) {
    // TODO(srte): Use fallback controller if no feedback is available.
    if (controller_factory_with_feedback_) {
      RTC_LOG(LS_INFO) << "Creating feedback based only controller";
      controller_ = controller_factory_with_feedback_->Create(initial_config_);
      process_interval_ =
          controller_factory_with_feedback_->GetProcessInterval();
    } else {
      RTC_LOG(LS_INFO) << "Creating fallback controller";
      controller_ = controller_factory_fallback_->Create(initial_config_);
      process_interval_ = controller_factory_fallback_->GetProcessInterval();
    }
    UpdateControllerWithTimeInterval();
    StartProcessPeriodicTasks();
  }
  RTC_DCHECK(controller_);
}

void SendSideCongestionController::UpdateInitialConstraints(
    TargetRateConstraints new_contraints) {
  if (!new_contraints.starting_rate)
    new_contraints.starting_rate = initial_config_.constraints.starting_rate;
  RTC_DCHECK(new_contraints.starting_rate);
  initial_config_.constraints = new_contraints;
}

SendSideCongestionController::~SendSideCongestionController() = default;

void SendSideCongestionController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::RegisterNetworkObserver(
    NetworkChangedObserver* observer) {
  task_queue_->PostTask([this, observer]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
    MaybeCreateControllers();
  });
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  TargetRateConstraints constraints = ConvertConstraints(
      min_bitrate_bps, max_bitrate_bps, start_bitrate_bps, clock_);
  task_queue_->PostTask([this, constraints]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (controller_) {
      control_handler_->PostUpdates(
          controller_->OnTargetRateConstraints(constraints));
    } else {
      UpdateInitialConstraints(constraints);
    }
  });
}

void SendSideCongestionController::SetAllocatedSendBitrateLimits(
    int64_t min_send_bitrate_bps,
    int64_t max_padding_bitrate_bps,
    int64_t max_total_bitrate_bps) {
  RTC_DCHECK_RUN_ON(task_queue_);
  streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
  streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
  streams_config_.max_total_allocated_bitrate =
      DataRate::bps(max_total_bitrate_bps);
  UpdateStreamsConfig();
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void SendSideCongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int start_bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  if (reset_feedback_on_route_change_)
    transport_feedback_adapter_.SetNetworkIds(network_route.local_network_id,
                                              network_route.remote_network_id);
  transport_overhead_bytes_per_packet_ = network_route.packet_overhead;

  NetworkRouteChange msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.constraints = ConvertConstraints(min_bitrate_bps, max_bitrate_bps,
                                       start_bitrate_bps, clock_);

  task_queue_->PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (controller_) {
      control_handler_->PostUpdates(controller_->OnNetworkRouteChange(msg));
    } else {
      UpdateInitialConstraints(msg.constraints);
    }
    pacer_->UpdateOutstandingData(0);
  });
}

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  // This is only called in the OnNetworkChanged callback in
  // RtpTransportControllerSend which is called from ControlHandler, which is
  // running on the task queue.
  // TODO(srte): Remove this function when RtpTransportControllerSend stops
  // calling it.
  RTC_DCHECK_RUN_ON(task_queue_);
  if (!control_handler_) {
    return false;
  }
  // TODO(srte): Remove this interface and push information about bandwidth
  // estimation to users of this class, thereby reducing synchronous calls.
  if (control_handler_->last_transfer_rate().has_value()) {
    *bandwidth = control_handler_->last_transfer_rate()
                     ->network_estimate.bandwidth.bps();
    return true;
  }
  return false;
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver() {
  return this;
}

void SendSideCongestionController::SetPerPacketFeedbackAvailable(
    bool available) {
  RTC_DCHECK_RUN_ON(task_queue_);
  packet_feedback_available_ = available;
  MaybeRecreateControllers();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  task_queue_->PostTask([this, enable]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    streams_config_.requests_alr_probing = enable;
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::UpdateStreamsConfig() {
  streams_config_.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  if (controller_)
    control_handler_->PostUpdates(
        controller_->OnStreamsConfig(streams_config_));
}

TransportFeedbackObserver*
SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void SendSideCongestionController::SignalNetworkState(NetworkState state) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (state == kNetworkUp ? "Up" : "Down");
  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = state == kNetworkUp;
  task_queue_->PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    network_available_ = msg.network_available;
    if (controller_) {
      control_handler_->PostUpdates(controller_->OnNetworkAvailability(msg));
      control_handler_->OnNetworkAvailability(msg);
    } else {
      MaybeCreateControllers();
    }
  });
}

void SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  absl::optional<SentPacket> packet_msg =
      transport_feedback_adapter_.ProcessSentPacket(sent_packet);
  if (packet_msg) {
    task_queue_->PostTask([this, packet_msg]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      if (controller_)
        control_handler_->PostUpdates(controller_->OnSentPacket(*packet_msg));
    });
  }
  MaybeUpdateOutstandingData();
}

void SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                               int64_t max_rtt_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  RoundTripTimeUpdate report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(avg_rtt_ms);
  report.smoothed = true;
  task_queue_->PostTask([this, report]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRoundTripTimeUpdate(report));
  });
}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  // Using task queue to process, just sleep long to avoid wasting resources.
  return 60 * 1000;
}

void SendSideCongestionController::Process() {
  // Ignored, using task queue to process.
}

void SendSideCongestionController::StartProcessPeriodicTasks() {
  if (!periodic_tasks_enabled_)
    return;
  if (!pacer_queue_update_task_) {
    pacer_queue_update_task_ =
        StartPeriodicTask(task_queue_, PacerQueueUpdateIntervalMs, [this]() {
          RTC_DCHECK_RUN_ON(task_queue_);
          UpdatePacerQueue();
        });
  }
  if (controller_task_) {
    // Stop is not synchronous, but is guaranteed to occur before the first
    // invocation of the new controller task started below.
    controller_task_->Stop();
    controller_task_ = nullptr;
  }
  if (process_interval_.IsFinite()) {
    // The controller task is owned by the task queue and lives until the task
    // queue is destroyed or some time after Stop() is called, whichever comes
    // first.
    controller_task_ =
        StartPeriodicTask(task_queue_, process_interval_.ms(), [this]() {
          RTC_DCHECK_RUN_ON(task_queue_);
          UpdateControllerWithTimeInterval();
        });
  }
}

void SendSideCongestionController::UpdateControllerWithTimeInterval() {
  if (controller_) {
    ProcessInterval msg;
    msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
    control_handler_->PostUpdates(controller_->OnProcessInterval(msg));
  }
}

void SendSideCongestionController::UpdatePacerQueue() {
  if (control_handler_) {
    TimeDelta expected_queue_time =
        TimeDelta::ms(pacer_->ExpectedQueueTimeMs());
    control_handler_->OnPacerQueueUpdate(expected_queue_time);
  }
}

void SendSideCongestionController::AddPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    size_t length,
    const PacedPacketInfo& pacing_info) {
  if (send_side_bwe_with_overhead_) {
    length += transport_overhead_bytes_per_packet_;
  }
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);

  absl::optional<TransportPacketsFeedback> feedback_msg =
      transport_feedback_adapter_.ProcessTransportFeedback(feedback);
  if (feedback_msg) {
    task_queue_->PostTask([this, feedback_msg]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      if (controller_)
        control_handler_->PostUpdates(
            controller_->OnTransportPacketsFeedback(*feedback_msg));
    });
  }
  MaybeUpdateOutstandingData();
}

void SendSideCongestionController::MaybeUpdateOutstandingData() {
  DataSize in_flight_data = transport_feedback_adapter_.GetOutstandingData();
  task_queue_->PostTask([this, in_flight_data]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (control_handler_)
      control_handler_->OnOutstandingData(in_flight_data);
  });
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::PostPeriodicTasksForTest() {
  task_queue_->PostTask([this]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    UpdateControllerWithTimeInterval();
    UpdatePacerQueue();
  });
}

void SendSideCongestionController::WaitOnTasksForTest() {
  rtc::Event event;
  task_queue_->PostTask([&event]() { event.Set(); });
  event.Wait(rtc::Event::kForever);
}

void SendSideCongestionController::SetPacingFactor(float pacing_factor) {
  RTC_DCHECK_RUN_ON(task_queue_);
  streams_config_.pacing_factor = pacing_factor;
  UpdateStreamsConfig();
}

void SendSideCongestionController::SetAllocatedBitrateWithoutFeedback(
    uint32_t bitrate_bps) {
  task_queue_->PostTask([this, bitrate_bps]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    streams_config_.unacknowledged_rate_allocation = DataRate::bps(bitrate_bps);
    UpdateStreamsConfig();
  });
}

void SendSideCongestionController::DisablePeriodicTasks() {
  task_queue_->PostTask([this]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    periodic_tasks_enabled_ = false;
  });
}

void SendSideCongestionController::OnReceivedEstimatedBitrate(
    uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  task_queue_->PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRemoteBitrateReport(msg));
  });
}

void SendSideCongestionController::OnReceivedRtcpReceiverReport(
    const webrtc::ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  task_queue_->PostTask([this, report_blocks, now_ms]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);
  });

  task_queue_->PostTask([this, now_ms, rtt_ms]() {
    RTC_DCHECK_RUN_ON(task_queue_);
    RoundTripTimeUpdate report;
    report.receive_time = Timestamp::ms(now_ms);
    report.round_trip_time = TimeDelta::ms(rtt_ms);
    report.smoothed = false;
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRoundTripTimeUpdate(report));
  });
}

void SendSideCongestionController::OnReceivedRtcpReceiverReportBlocks(
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
    control_handler_->PostUpdates(controller_->OnTransportLossReport(msg));
  last_report_block_time_ = now;
}
}  // namespace webrtc_cc
}  // namespace webrtc
