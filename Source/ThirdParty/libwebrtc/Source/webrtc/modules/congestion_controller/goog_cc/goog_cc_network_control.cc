/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "modules/congestion_controller/goog_cc/probe_controller.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

const char kCwndExperiment[] = "WebRTC-CwndExperiment";
// When CongestionWindowPushback is enabled, the pacer is oblivious to
// the congestion window. The relation between outstanding data and
// the congestion window affects encoder allocations directly.
const char kCongestionPushbackExperiment[] = "WebRTC-CongestionWindowPushback";

const int64_t kDefaultAcceptedQueueMs = 250;

// From RTCPSender video report interval.
constexpr TimeDelta kLossUpdateInterval = TimeDelta::Millis<1000>();

// Pacing-rate relative to our target send rate.
// Multiplicative factor that is applied to the target bitrate to calculate
// the number of bytes that can be transmitted per interval.
// Increasing this factor will result in lower delays in cases of bitrate
// overshoots from the encoder.
const float kDefaultPaceMultiplier = 2.5f;

bool IsCongestionWindowPushbackExperimentEnabled() {
  return webrtc::field_trial::IsEnabled(kCongestionPushbackExperiment) &&
         webrtc::field_trial::IsEnabled(kCwndExperiment);
}

std::unique_ptr<CongestionWindowPushbackController>
MaybeInitalizeCongestionWindowPushbackController() {
  return IsCongestionWindowPushbackExperimentEnabled()
             ? absl::make_unique<CongestionWindowPushbackController>()
             : nullptr;
}

bool CwndExperimentEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}
bool ReadCwndExperimentParameter(int64_t* accepted_queue_ms) {
  RTC_DCHECK(accepted_queue_ms);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%" PRId64, accepted_queue_ms);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*accepted_queue_ms, 0)
        << "Accepted must be greater than or equal to 0.";
    return true;
  }
  return false;
}

// Makes sure that the bitrate and the min, max values are in valid range.
static void ClampBitrates(int64_t* bitrate_bps,
                          int64_t* min_bitrate_bps,
                          int64_t* max_bitrate_bps) {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  if (*min_bitrate_bps < congestion_controller::GetMinBitrateBps())
    *min_bitrate_bps = congestion_controller::GetMinBitrateBps();
  if (*max_bitrate_bps > 0)
    *max_bitrate_bps = std::max(*min_bitrate_bps, *max_bitrate_bps);
  if (*bitrate_bps > 0)
    *bitrate_bps = std::max(*min_bitrate_bps, *bitrate_bps);
}

std::vector<PacketFeedback> ReceivedPacketsFeedbackAsRtp(
    const TransportPacketsFeedback report) {
  std::vector<PacketFeedback> packet_feedback_vector;
  for (auto& fb : report.PacketsWithFeedback()) {
    if (fb.receive_time.IsFinite()) {
      PacketFeedback pf(fb.receive_time.ms(), 0);
      pf.creation_time_ms = report.feedback_time.ms();
      pf.payload_size = fb.sent_packet.size.bytes();
      pf.pacing_info = fb.sent_packet.pacing_info;
      pf.send_time_ms = fb.sent_packet.send_time.ms();
      pf.unacknowledged_data = fb.sent_packet.prior_unacked_data.bytes();
      packet_feedback_vector.push_back(pf);
    }
  }
  return packet_feedback_vector;
}

int64_t GetBpsOrDefault(const absl::optional<DataRate>& rate,
                        int64_t fallback_bps) {
  if (rate && rate->IsFinite()) {
    return rate->bps();
  } else {
    return fallback_bps;
  }
}

}  // namespace

GoogCcNetworkController::GoogCcNetworkController(RtcEventLog* event_log,
                                                 NetworkControllerConfig config,
                                                 bool feedback_only)
    : event_log_(event_log),
      packet_feedback_only_(feedback_only),
      safe_reset_on_route_change_("Enabled"),
      safe_reset_acknowledged_rate_("ack"),
      use_stable_bandwidth_estimate_(
          field_trial::IsEnabled("WebRTC-Bwe-StableBandwidthEstimate")),
      probe_controller_(new ProbeController()),
      congestion_window_pushback_controller_(
          MaybeInitalizeCongestionWindowPushbackController()),
      bandwidth_estimation_(
          absl::make_unique<SendSideBandwidthEstimation>(event_log_)),
      alr_detector_(absl::make_unique<AlrDetector>()),
      probe_bitrate_estimator_(new ProbeBitrateEstimator(event_log)),
      delay_based_bwe_(new DelayBasedBwe(event_log_)),
      acknowledged_bitrate_estimator_(
          absl::make_unique<AcknowledgedBitrateEstimator>()),
      initial_config_(config),
      last_target_rate_(*config.constraints.starting_rate),
      pacing_factor_(config.stream_based_config.pacing_factor.value_or(
          kDefaultPaceMultiplier)),
      min_pacing_rate_(config.stream_based_config.min_pacing_rate.value_or(
          DataRate::Zero())),
      max_padding_rate_(config.stream_based_config.max_padding_rate.value_or(
          DataRate::Zero())),
      max_total_allocated_bitrate_(DataRate::Zero()),
      in_cwnd_experiment_(CwndExperimentEnabled()),
      accepted_queue_ms_(kDefaultAcceptedQueueMs) {
  RTC_DCHECK(config.constraints.at_time.IsFinite());
  ParseFieldTrial(
      {&safe_reset_on_route_change_, &safe_reset_acknowledged_rate_},
      field_trial::FindFullName("WebRTC-Bwe-SafeResetOnRouteChange"));

  delay_based_bwe_->SetMinBitrate(congestion_controller::GetMinBitrate());
  if (in_cwnd_experiment_ &&
      !ReadCwndExperimentParameter(&accepted_queue_ms_)) {
    RTC_LOG(LS_WARNING) << "Failed to parse parameters for CwndExperiment "
                           "from field trial string. Experiment disabled.";
    in_cwnd_experiment_ = false;
  }
}

GoogCcNetworkController::~GoogCcNetworkController() {}

NetworkControlUpdate GoogCcNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  NetworkControlUpdate update;
  update.probe_cluster_configs = probe_controller_->OnNetworkAvailability(msg);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  int64_t min_bitrate_bps = GetBpsOrDefault(msg.constraints.min_data_rate, 0);
  int64_t max_bitrate_bps = GetBpsOrDefault(msg.constraints.max_data_rate, -1);
  int64_t start_bitrate_bps =
      GetBpsOrDefault(msg.constraints.starting_rate, -1);

  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);

  if (safe_reset_on_route_change_) {
    absl::optional<uint32_t> estimated_bitrate_bps;
    if (safe_reset_acknowledged_rate_) {
      estimated_bitrate_bps = acknowledged_bitrate_estimator_->bitrate_bps();
      if (!estimated_bitrate_bps)
        estimated_bitrate_bps = acknowledged_bitrate_estimator_->PeekBps();
    } else {
      int32_t target_bitrate_bps;
      uint8_t fraction_loss;
      int64_t rtt_ms;
      bandwidth_estimation_->CurrentEstimate(&target_bitrate_bps,
                                             &fraction_loss, &rtt_ms);
      estimated_bitrate_bps = target_bitrate_bps;
    }
    if (estimated_bitrate_bps && (!msg.constraints.starting_rate ||
                                  estimated_bitrate_bps < start_bitrate_bps)) {
      start_bitrate_bps = *estimated_bitrate_bps;
      msg.constraints.starting_rate = DataRate::bps(start_bitrate_bps);
    }
  }

  acknowledged_bitrate_estimator_.reset(new AcknowledgedBitrateEstimator());
  probe_bitrate_estimator_.reset(new ProbeBitrateEstimator(event_log_));
  delay_based_bwe_.reset(new DelayBasedBwe(event_log_));
  if (msg.constraints.starting_rate)
    delay_based_bwe_->SetStartBitrate(*msg.constraints.starting_rate);
  // TODO(srte): Use original values instead of converted.
  delay_based_bwe_->SetMinBitrate(DataRate::bps(min_bitrate_bps));
  bandwidth_estimation_->OnRouteChange();
  bandwidth_estimation_->SetBitrates(
      msg.constraints.starting_rate, DataRate::bps(min_bitrate_bps),
      msg.constraints.max_data_rate.value_or(DataRate::Infinity()),
      msg.at_time);

  probe_controller_->Reset(msg.at_time.ms());
  NetworkControlUpdate update;
  update.probe_cluster_configs = probe_controller_->SetBitrates(
      min_bitrate_bps, start_bitrate_bps, max_bitrate_bps, msg.at_time.ms());
  MaybeTriggerOnNetworkChanged(&update, msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  NetworkControlUpdate update;
  if (initial_config_) {
    update.probe_cluster_configs =
        UpdateBitrateConstraints(initial_config_->constraints,
                                 initial_config_->constraints.starting_rate);
    update.pacer_config = GetPacingRates(msg.at_time);

    probe_controller_->EnablePeriodicAlrProbing(
        initial_config_->stream_based_config.requests_alr_probing);
    absl::optional<DataRate> total_bitrate =
        initial_config_->stream_based_config.max_total_allocated_bitrate;
    if (total_bitrate) {
      auto probes = probe_controller_->OnMaxTotalAllocatedBitrate(
          total_bitrate->bps(), msg.at_time.ms());
      update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                          probes.begin(), probes.end());

      max_total_allocated_bitrate_ = *total_bitrate;
    }
    initial_config_.reset();
  }

  bandwidth_estimation_->UpdateEstimate(msg.at_time);
  absl::optional<int64_t> start_time_ms =
      alr_detector_->GetApplicationLimitedRegionStartTime();
  probe_controller_->SetAlrStartTimeMs(start_time_ms);

  auto probes = probe_controller_->Process(msg.at_time.ms());
  update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                      probes.begin(), probes.end());

  MaybeTriggerOnNetworkChanged(&update, msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  if (packet_feedback_only_) {
    RTC_LOG(LS_ERROR) << "Received REMB for packet feedback only GoogCC";
    return NetworkControlUpdate();
  }
  bandwidth_estimation_->UpdateReceiverEstimate(msg.receive_time,
                                                msg.bandwidth);
  BWE_TEST_LOGGING_PLOT(1, "REMB_kbps", msg.receive_time.ms(),
                        msg.bandwidth.bps() / 1000);
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  if (packet_feedback_only_)
    return NetworkControlUpdate();
  if (msg.smoothed) {
    delay_based_bwe_->OnRttUpdate(msg.round_trip_time);
  } else {
    bandwidth_estimation_->UpdateRtt(msg.round_trip_time, msg.receive_time);
  }
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnSentPacket(
    SentPacket sent_packet) {
  alr_detector_->OnBytesSent(sent_packet.size.bytes(),
                             sent_packet.send_time.ms());
  if (!first_packet_sent_) {
    first_packet_sent_ = true;
    // Initialize feedback time to send time to allow estimation of RTT until
    // first feedback is received.
    bandwidth_estimation_->UpdatePropagationRtt(sent_packet.send_time,
                                                TimeDelta::Zero());
  }
  if (congestion_window_pushback_controller_) {
    congestion_window_pushback_controller_->UpdateOutstandingData(
        sent_packet.data_in_flight.bytes());
    NetworkControlUpdate update;
    MaybeTriggerOnNetworkChanged(&update, sent_packet.send_time);
    return update;
  } else {
    return NetworkControlUpdate();
  }
}

NetworkControlUpdate GoogCcNetworkController::OnStreamsConfig(
    StreamsConfig msg) {
  NetworkControlUpdate update;
  probe_controller_->EnablePeriodicAlrProbing(msg.requests_alr_probing);
  if (msg.max_total_allocated_bitrate &&
      *msg.max_total_allocated_bitrate != max_total_allocated_bitrate_) {
    update.probe_cluster_configs =
        probe_controller_->OnMaxTotalAllocatedBitrate(
            msg.max_total_allocated_bitrate->bps(), msg.at_time.ms());
    max_total_allocated_bitrate_ = *msg.max_total_allocated_bitrate;
  }
  bool pacing_changed = false;
  if (msg.pacing_factor && *msg.pacing_factor != pacing_factor_) {
    pacing_factor_ = *msg.pacing_factor;
    pacing_changed = true;
  }
  if (msg.min_pacing_rate && *msg.min_pacing_rate != min_pacing_rate_) {
    min_pacing_rate_ = *msg.min_pacing_rate;
    pacing_changed = true;
  }
  if (msg.max_padding_rate && *msg.max_padding_rate != max_padding_rate_) {
    max_padding_rate_ = *msg.max_padding_rate;
    pacing_changed = true;
  }
  acknowledged_bitrate_estimator_->SetAllocatedBitrateWithoutFeedback(
      msg.unacknowledged_rate_allocation.bps());

  if (pacing_changed)
    update.pacer_config = GetPacingRates(msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTargetRateConstraints(
    TargetRateConstraints constraints) {
  NetworkControlUpdate update;
  update.probe_cluster_configs =
      UpdateBitrateConstraints(constraints, constraints.starting_rate);
  MaybeTriggerOnNetworkChanged(&update, constraints.at_time);
  return update;
}

std::vector<ProbeClusterConfig>
GoogCcNetworkController::UpdateBitrateConstraints(
    TargetRateConstraints constraints,
    absl::optional<DataRate> starting_rate) {
  int64_t min_bitrate_bps = GetBpsOrDefault(constraints.min_data_rate, 0);
  int64_t max_bitrate_bps = GetBpsOrDefault(constraints.max_data_rate, -1);
  int64_t start_bitrate_bps = GetBpsOrDefault(starting_rate, -1);

  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);

  std::vector<ProbeClusterConfig> probes(probe_controller_->SetBitrates(
      min_bitrate_bps, start_bitrate_bps, max_bitrate_bps,
      constraints.at_time.ms()));

  bandwidth_estimation_->SetBitrates(
      starting_rate, DataRate::bps(min_bitrate_bps),
      constraints.max_data_rate.value_or(DataRate::Infinity()),
      constraints.at_time);
  if (starting_rate)
    delay_based_bwe_->SetStartBitrate(*starting_rate);
  delay_based_bwe_->SetMinBitrate(DataRate::bps(min_bitrate_bps));
  return probes;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  if (packet_feedback_only_)
    return NetworkControlUpdate();
  int64_t total_packets_delta =
      msg.packets_received_delta + msg.packets_lost_delta;
  bandwidth_estimation_->UpdatePacketsLost(
      msg.packets_lost_delta, total_packets_delta, msg.receive_time);
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback report) {
  if (report.packet_feedbacks.empty()) {
    // TODO(bugs.webrtc.org/10125): Design a better mechanism to safe-guard
    // against building very large network queues.
    return NetworkControlUpdate();
  }

  if (congestion_window_pushback_controller_) {
    congestion_window_pushback_controller_->UpdateOutstandingData(
        report.data_in_flight.bytes());
  }
  TimeDelta max_feedback_rtt = TimeDelta::MinusInfinity();
  TimeDelta min_propagation_rtt = TimeDelta::PlusInfinity();
  Timestamp max_recv_time = Timestamp::MinusInfinity();

  std::vector<PacketResult> feedbacks = report.ReceivedWithSendInfo();
  for (const auto& feedback : feedbacks)
    max_recv_time = std::max(max_recv_time, feedback.receive_time);

  for (const auto& feedback : feedbacks) {
    TimeDelta feedback_rtt =
        report.feedback_time - feedback.sent_packet.send_time;
    TimeDelta min_pending_time = feedback.receive_time - max_recv_time;
    TimeDelta propagation_rtt = feedback_rtt - min_pending_time;
    max_feedback_rtt = std::max(max_feedback_rtt, feedback_rtt);
    min_propagation_rtt = std::min(min_propagation_rtt, propagation_rtt);
  }

  if (max_feedback_rtt.IsFinite()) {
    feedback_max_rtts_.push_back(max_feedback_rtt.ms());
    const size_t kMaxFeedbackRttWindow = 32;
    if (feedback_max_rtts_.size() > kMaxFeedbackRttWindow)
      feedback_max_rtts_.pop_front();
    // TODO(srte): Use time since last unacknowledged packet.
    bandwidth_estimation_->UpdatePropagationRtt(report.feedback_time,
                                                min_propagation_rtt);
  }
  if (packet_feedback_only_) {
    if (!feedback_max_rtts_.empty()) {
      int64_t sum_rtt_ms = std::accumulate(feedback_max_rtts_.begin(),
                                           feedback_max_rtts_.end(), 0);
      int64_t mean_rtt_ms = sum_rtt_ms / feedback_max_rtts_.size();
      delay_based_bwe_->OnRttUpdate(TimeDelta::ms(mean_rtt_ms));
    }

    TimeDelta feedback_min_rtt = TimeDelta::PlusInfinity();
    for (const auto& packet_feedback : feedbacks) {
      TimeDelta pending_time = packet_feedback.receive_time - max_recv_time;
      TimeDelta rtt = report.feedback_time -
                      packet_feedback.sent_packet.send_time - pending_time;
      // Value used for predicting NACK round trip time in FEC controller.
      feedback_min_rtt = std::min(rtt, feedback_min_rtt);
    }
    if (feedback_min_rtt.IsFinite()) {
      bandwidth_estimation_->UpdateRtt(feedback_min_rtt, report.feedback_time);
    }

    expected_packets_since_last_loss_update_ +=
        report.PacketsWithFeedback().size();
    for (const auto& packet_feedback : report.PacketsWithFeedback()) {
      if (packet_feedback.receive_time.IsInfinite())
        lost_packets_since_last_loss_update_ += 1;
    }
    if (report.feedback_time > next_loss_update_) {
      next_loss_update_ = report.feedback_time + kLossUpdateInterval;
      bandwidth_estimation_->UpdatePacketsLost(
          lost_packets_since_last_loss_update_,
          expected_packets_since_last_loss_update_, report.feedback_time);
      expected_packets_since_last_loss_update_ = 0;
      lost_packets_since_last_loss_update_ = 0;
    }
  }

  std::vector<PacketFeedback> received_feedback_vector =
      ReceivedPacketsFeedbackAsRtp(report);

  absl::optional<int64_t> alr_start_time =
      alr_detector_->GetApplicationLimitedRegionStartTime();

  if (previously_in_alr && !alr_start_time.has_value()) {
    int64_t now_ms = report.feedback_time.ms();
    acknowledged_bitrate_estimator_->SetAlrEndedTimeMs(now_ms);
    probe_controller_->SetAlrEndedTimeMs(now_ms);
  }
  previously_in_alr = alr_start_time.has_value();
  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      received_feedback_vector);
  auto acknowledged_bitrate = acknowledged_bitrate_estimator_->bitrate();
  bandwidth_estimation_->SetAcknowledgedRate(acknowledged_bitrate,
                                             report.feedback_time);
  bandwidth_estimation_->IncomingPacketFeedbackVector(report);
  for (const auto& feedback : received_feedback_vector) {
    if (feedback.pacing_info.probe_cluster_id != PacedPacketInfo::kNotAProbe) {
      probe_bitrate_estimator_->HandleProbeAndEstimateBitrate(feedback);
    }
  }
  absl::optional<DataRate> probe_bitrate =
      probe_bitrate_estimator_->FetchAndResetLastEstimatedBitrate();

  DelayBasedBwe::Result result;
  result = delay_based_bwe_->IncomingPacketFeedbackVector(
      received_feedback_vector, acknowledged_bitrate, probe_bitrate,
      report.feedback_time);

  NetworkControlUpdate update;
  if (result.updated) {
    if (result.probe) {
      bandwidth_estimation_->SetSendBitrate(result.target_bitrate,
                                            report.feedback_time);
    }
    // Since SetSendBitrate now resets the delay-based estimate, we have to call
    // UpdateDelayBasedEstimate after SetSendBitrate.
    bandwidth_estimation_->UpdateDelayBasedEstimate(report.feedback_time,
                                                    result.target_bitrate);
    // Update the estimate in the ProbeController, in case we want to probe.
    MaybeTriggerOnNetworkChanged(&update, report.feedback_time);
  }
  if (result.recovered_from_overuse) {
    probe_controller_->SetAlrStartTimeMs(alr_start_time);
    auto probes = probe_controller_->RequestProbe(report.feedback_time.ms());
    update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                        probes.begin(), probes.end());
  }

  // No valid RTT could be because send-side BWE isn't used, in which case
  // we don't try to limit the outstanding packets.
  if (in_cwnd_experiment_ && max_feedback_rtt.IsFinite()) {
    int64_t min_feedback_max_rtt_ms =
        *std::min_element(feedback_max_rtts_.begin(), feedback_max_rtts_.end());

    const DataSize kMinCwnd = DataSize::bytes(2 * 1500);
    TimeDelta time_window =
        TimeDelta::ms(min_feedback_max_rtt_ms + accepted_queue_ms_);
    DataSize data_window = last_target_rate_ * time_window;
    if (current_data_window_) {
      data_window =
          std::max(kMinCwnd, (data_window + current_data_window_.value()) / 2);
    } else {
      data_window = std::max(kMinCwnd, data_window);
    }
    current_data_window_ = data_window;
  }
  if (congestion_window_pushback_controller_ && current_data_window_) {
    congestion_window_pushback_controller_->SetDataWindow(
        *current_data_window_);
  } else {
    update.congestion_window = current_data_window_;
  }

  return update;
}

NetworkControlUpdate GoogCcNetworkController::GetNetworkState(
    Timestamp at_time) const {
  DataRate bandwidth = DataRate::bps(last_estimated_bitrate_bps_);
  TimeDelta rtt = TimeDelta::ms(last_estimated_rtt_ms_);
  NetworkControlUpdate update;
  update.target_rate = TargetTransferRate();
  update.target_rate->network_estimate.at_time = at_time;
  update.target_rate->network_estimate.bandwidth = bandwidth;
  update.target_rate->network_estimate.loss_rate_ratio =
      last_estimated_fraction_loss_ / 255.0;
  update.target_rate->network_estimate.round_trip_time = rtt;
  update.target_rate->network_estimate.bwe_period =
      delay_based_bwe_->GetExpectedBwePeriod();
  update.target_rate->at_time = at_time;
  update.target_rate->target_rate = bandwidth;
  update.pacer_config = GetPacingRates(at_time);
  update.congestion_window = current_data_window_;
  return update;
}


void GoogCcNetworkController::MaybeTriggerOnNetworkChanged(
    NetworkControlUpdate* update,
    Timestamp at_time) {
  int32_t estimated_bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt_ms;
  bandwidth_estimation_->CurrentEstimate(&estimated_bitrate_bps, &fraction_loss,
                                         &rtt_ms);

  estimated_bitrate_bps = std::max<int32_t>(
      estimated_bitrate_bps, bandwidth_estimation_->GetMinBitrate());

  BWE_TEST_LOGGING_PLOT(1, "fraction_loss_%", at_time.ms(),
                        (fraction_loss * 100) / 256);
  BWE_TEST_LOGGING_PLOT(1, "rtt_ms", at_time.ms(), rtt_ms);
  BWE_TEST_LOGGING_PLOT(1, "Target_bitrate_kbps", at_time.ms(),
                        estimated_bitrate_bps / 1000);

  if ((estimated_bitrate_bps != last_estimated_bitrate_bps_) ||
      (fraction_loss != last_estimated_fraction_loss_) ||
      (rtt_ms != last_estimated_rtt_ms_)) {
    last_estimated_bitrate_bps_ = estimated_bitrate_bps;
    last_estimated_fraction_loss_ = fraction_loss;
    last_estimated_rtt_ms_ = rtt_ms;

    alr_detector_->SetEstimatedBitrate(estimated_bitrate_bps);

    last_target_rate_ = DataRate::bps(estimated_bitrate_bps);
    DataRate bandwidth = use_stable_bandwidth_estimate_
                             ? bandwidth_estimation_->GetEstimatedLinkCapacity()
                             : last_target_rate_;

    TimeDelta bwe_period = delay_based_bwe_->GetExpectedBwePeriod();

    // Set the target rate to the full estimated bandwidth since the estimation
    // for legacy reasons includes target rate constraints.
    DataRate target_rate = last_target_rate_;
    if (congestion_window_pushback_controller_) {
      int64_t pushback_rate =
          congestion_window_pushback_controller_->UpdateTargetBitrate(
              target_rate.bps());
      pushback_rate = std::max<int64_t>(bandwidth_estimation_->GetMinBitrate(),
                                        pushback_rate);
      target_rate = DataRate::bps(pushback_rate);
    }

    TargetTransferRate target_rate_msg;
    target_rate_msg.at_time = at_time;
    target_rate_msg.target_rate = target_rate;
    target_rate_msg.network_estimate.at_time = at_time;
    target_rate_msg.network_estimate.round_trip_time = TimeDelta::ms(rtt_ms);
    target_rate_msg.network_estimate.bandwidth = bandwidth;
    target_rate_msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0f;
    target_rate_msg.network_estimate.bwe_period = bwe_period;

    update->target_rate = target_rate_msg;

    auto probes = probe_controller_->SetEstimatedBitrate(
        last_target_rate_.bps(), at_time.ms());
    update->probe_cluster_configs.insert(update->probe_cluster_configs.end(),
                                         probes.begin(), probes.end());
    update->pacer_config = GetPacingRates(at_time);
  }
}

PacerConfig GoogCcNetworkController::GetPacingRates(Timestamp at_time) const {
  DataRate pacing_rate =
      std::max(min_pacing_rate_, last_target_rate_) * pacing_factor_;
  DataRate padding_rate = std::min(max_padding_rate_, last_target_rate_);
  PacerConfig msg;
  msg.at_time = at_time;
  msg.time_window = TimeDelta::seconds(1);
  msg.data_window = pacing_rate * msg.time_window;
  msg.pad_window = padding_rate * msg.time_window;
  return msg;
}

}  // namespace webrtc
