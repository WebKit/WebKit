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
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/transport/bandwidth_usage.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_remote_estimate.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator_interface.h"
#include "modules/congestion_controller/goog_cc/alr_detector.h"
#include "modules/congestion_controller/goog_cc/congestion_window_pushback_controller.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"
#include "modules/congestion_controller/goog_cc/probe_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/probe_controller.h"
#include "modules/congestion_controller/goog_cc/send_side_bandwidth_estimation.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// Pacing-rate relative to our target send rate.
// Multiplicative factor that is applied to the target bitrate to calculate
// the number of bytes that can be transmitted per interval.
// Increasing this factor will result in lower delays in cases of bitrate
// overshoots from the encoder.
constexpr float kDefaultPaceMultiplier = 2.5f;

// If the probe result is far below the current throughput estimate
// it's unlikely that the probe is accurate, so we don't want to drop too far.
// However, if we actually are overusing, we want to drop to something slightly
// below the current throughput estimate to drain the network queues.
constexpr double kProbeDropThroughputFraction = 0.85;

BandwidthLimitedCause GetBandwidthLimitedCause(LossBasedState loss_based_state,
                                               bool is_rtt_above_limit,
                                               BandwidthUsage bandwidth_usage) {
  if (bandwidth_usage == BandwidthUsage::kBwOverusing ||
      bandwidth_usage == BandwidthUsage::kBwUnderusing) {
    return BandwidthLimitedCause::kDelayBasedLimitedDelayIncreased;
  } else if (is_rtt_above_limit) {
    return BandwidthLimitedCause::kRttBasedBackOffHighRtt;
  }
  switch (loss_based_state) {
    case LossBasedState::kDecreasing:
      // Probes may not be sent in this state.
      return BandwidthLimitedCause::kLossLimitedBwe;
    case LossBasedState::kIncreaseUsingPadding:
      // Probes may not be sent in this state.
      return BandwidthLimitedCause::kLossLimitedBwe;
    case LossBasedState::kIncreasing:
      // Probes may be sent in this state.
      return BandwidthLimitedCause::kLossLimitedBweIncreasing;
    case LossBasedState::kDelayBasedEstimate:
      return BandwidthLimitedCause::kDelayBasedLimited;
  }
}

}  // namespace

GoogCcNetworkController::GoogCcNetworkController(NetworkControllerConfig config,
                                                 GoogCcConfig goog_cc_config)
    : env_(config.env),
      safe_reset_on_route_change_("Enabled"),
      safe_reset_acknowledged_rate_("ack"),
      use_min_allocatable_as_lower_bound_(
          !env_.field_trials().IsDisabled("WebRTC-Bwe-MinAllocAsLowerBound")),
      ignore_probes_lower_than_network_estimate_(
          !env_.field_trials().IsDisabled(
              "WebRTC-Bwe-IgnoreProbesLowerThanNetworkStateEstimate")),
      limit_probes_lower_than_throughput_estimate_(
          !env_.field_trials().IsDisabled(
              "WebRTC-Bwe-LimitProbesLowerThanThroughputEstimate")),
      rate_control_settings_(env_.field_trials()),
      limit_pacingfactor_by_upper_link_capacity_estimate_(
          env_.field_trials().IsEnabled(
              "WebRTC-Bwe-LimitPacingFactorByUpperLinkCapacityEstimate")),
      probe_controller_(
          new ProbeController(&env_.field_trials(), &env_.event_log())),
      congestion_window_pushback_controller_(
          rate_control_settings_.UseCongestionWindowPushback()
              ? std::make_unique<CongestionWindowPushbackController>(
                    env_.field_trials())
              : nullptr),
      bandwidth_estimation_(
          std::make_unique<SendSideBandwidthEstimation>(&env_.field_trials(),
                                                        &env_.event_log())),
      alr_detector_(env_),
      probe_bitrate_estimator_(new ProbeBitrateEstimator(&env_.event_log())),
      network_estimator_(std::move(goog_cc_config.network_state_estimator)),
      network_state_predictor_(
          std::move(goog_cc_config.network_state_predictor)),
      delay_based_bwe_(new DelayBasedBwe(&env_.field_trials(),
                                         &env_.event_log(),
                                         network_state_predictor_.get())),
      acknowledged_bitrate_estimator_(
          AcknowledgedBitrateEstimatorInterface::Create(&env_.field_trials())),
      initial_config_(config),
      last_loss_based_target_rate_(*config.constraints.starting_rate),
      last_pushback_target_rate_(last_loss_based_target_rate_),
      last_loss_base_state_(LossBasedState::kDelayBasedEstimate),
      pacing_factor_(config.stream_based_config.pacing_factor.value_or(
          kDefaultPaceMultiplier)),
      pacing_time_window_(config.default_pacing_time_window),
      min_total_allocated_bitrate_(
          config.stream_based_config.min_total_allocated_bitrate.value_or(
              DataRate::Zero())),
      max_padding_rate_(config.stream_based_config.max_padding_rate.value_or(
          DataRate::Zero())) {
  RTC_DCHECK(config.constraints.at_time.IsFinite());
  ParseFieldTrial(
      {&safe_reset_on_route_change_, &safe_reset_acknowledged_rate_},
      env_.field_trials().Lookup("WebRTC-Bwe-SafeResetOnRouteChange"));
  if (delay_based_bwe_)
    delay_based_bwe_->SetMinBitrate(kCongestionControllerMinBitrate);
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
  if (safe_reset_on_route_change_) {
    std::optional<DataRate> estimated_bitrate;
    if (safe_reset_acknowledged_rate_) {
      estimated_bitrate = acknowledged_bitrate_estimator_->bitrate();
      if (!estimated_bitrate)
        estimated_bitrate = acknowledged_bitrate_estimator_->PeekRate();
    } else {
      estimated_bitrate = bandwidth_estimation_->target_rate();
    }
    if (estimated_bitrate) {
      if (msg.constraints.starting_rate) {
        msg.constraints.starting_rate =
            std::min(*msg.constraints.starting_rate, *estimated_bitrate);
      } else {
        msg.constraints.starting_rate = estimated_bitrate;
      }
    }
  }

  acknowledged_bitrate_estimator_ =
      AcknowledgedBitrateEstimatorInterface::Create(&env_.field_trials());
  probe_bitrate_estimator_.reset(new ProbeBitrateEstimator(&env_.event_log()));
  if (network_estimator_)
    network_estimator_->OnRouteChange(msg);
  delay_based_bwe_.reset(new DelayBasedBwe(
      &env_.field_trials(), &env_.event_log(), network_state_predictor_.get()));
  bandwidth_estimation_->OnRouteChange();
  probe_controller_->Reset(msg.at_time);
  NetworkControlUpdate update;
  update.probe_cluster_configs = ResetConstraints(msg.constraints);
  MaybeTriggerOnNetworkChanged(&update, msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  NetworkControlUpdate update;
  if (initial_config_) {
    update.probe_cluster_configs =
        ResetConstraints(initial_config_->constraints);
    update.pacer_config = GetPacingRates(msg.at_time);

    if (initial_config_->stream_based_config.requests_alr_probing) {
      probe_controller_->EnablePeriodicAlrProbing(
          *initial_config_->stream_based_config.requests_alr_probing);
    }
    if (initial_config_->stream_based_config.enable_repeated_initial_probing) {
      probe_controller_->EnableRepeatedInitialProbing(
          *initial_config_->stream_based_config
               .enable_repeated_initial_probing);
    }
    std::optional<DataRate> total_bitrate =
        initial_config_->stream_based_config.max_total_allocated_bitrate;
    if (total_bitrate) {
      auto probes = probe_controller_->OnMaxTotalAllocatedBitrate(
          *total_bitrate, msg.at_time);
      update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                          probes.begin(), probes.end());
    }
    initial_config_.reset();
  }
  if (congestion_window_pushback_controller_ && msg.pacer_queue) {
    congestion_window_pushback_controller_->UpdatePacingQueue(
        msg.pacer_queue->bytes());
  }
  bandwidth_estimation_->UpdateEstimate(msg.at_time);
  probe_controller_->SetAlrStartTime(
      alr_detector_.GetApplicationLimitedRegionStartTime());

  auto probes = probe_controller_->Process(msg.at_time);
  update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                      probes.begin(), probes.end());

  if (rate_control_settings_.UseCongestionWindow() &&
      !feedback_max_rtts_.empty()) {
    UpdateCongestionWindowSize();
  }
  if (congestion_window_pushback_controller_ && current_data_window_) {
    congestion_window_pushback_controller_->SetDataWindow(
        *current_data_window_);
  } else {
    update.congestion_window = current_data_window_;
  }
  MaybeTriggerOnNetworkChanged(&update, msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  bandwidth_estimation_->UpdateReceiverEstimate(msg.receive_time,
                                                msg.bandwidth);
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  if (msg.smoothed) {
    return NetworkControlUpdate();
  }
  RTC_DCHECK(!msg.round_trip_time.IsZero());
  if (delay_based_bwe_)
    delay_based_bwe_->OnRttUpdate(msg.round_trip_time);
  bandwidth_estimation_->UpdateRtt(msg.round_trip_time, msg.receive_time);
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnSentPacket(
    SentPacket sent_packet) {
  alr_detector_.OnBytesSent(sent_packet.size, sent_packet.send_time);
  acknowledged_bitrate_estimator_->SetAlr(
      alr_detector_.GetApplicationLimitedRegionStartTime().has_value());

  if (!first_packet_sent_) {
    first_packet_sent_ = true;
    // Initialize feedback time to send time to allow estimation of RTT until
    // first feedback is received.
    bandwidth_estimation_->UpdatePropagationRtt(sent_packet.send_time,
                                                TimeDelta::Zero());
  }
  bandwidth_estimation_->OnSentPacket(sent_packet);

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

NetworkControlUpdate GoogCcNetworkController::OnReceivedPacket(
    ReceivedPacket /* received_packet */) {
  return NetworkControlUpdate();
}

NetworkControlUpdate GoogCcNetworkController::OnStreamsConfig(
    StreamsConfig msg) {
  NetworkControlUpdate update;
  if (msg.requests_alr_probing) {
    probe_controller_->EnablePeriodicAlrProbing(*msg.requests_alr_probing);
  }
  if (msg.max_total_allocated_bitrate) {
    update.probe_cluster_configs =
        probe_controller_->OnMaxTotalAllocatedBitrate(
            *msg.max_total_allocated_bitrate, msg.at_time);
  }

  bool pacing_changed = false;
  if (msg.pacing_factor && *msg.pacing_factor != pacing_factor_) {
    pacing_factor_ = *msg.pacing_factor;
    pacing_changed = true;
  }
  if (msg.min_total_allocated_bitrate &&
      *msg.min_total_allocated_bitrate != min_total_allocated_bitrate_) {
    min_total_allocated_bitrate_ = *msg.min_total_allocated_bitrate;
    pacing_changed = true;

    if (use_min_allocatable_as_lower_bound_) {
      ClampConstraints();
      delay_based_bwe_->SetMinBitrate(min_data_rate_);
      bandwidth_estimation_->SetMinMaxBitrate(min_data_rate_, max_data_rate_);
    }
  }
  if (msg.max_padding_rate && *msg.max_padding_rate != max_padding_rate_) {
    max_padding_rate_ = *msg.max_padding_rate;
    pacing_changed = true;
  }

  if (pacing_changed)
    update.pacer_config = GetPacingRates(msg.at_time);
  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTargetRateConstraints(
    TargetRateConstraints constraints) {
  NetworkControlUpdate update;
  update.probe_cluster_configs = ResetConstraints(constraints);
  MaybeTriggerOnNetworkChanged(&update, constraints.at_time);
  return update;
}

void GoogCcNetworkController::ClampConstraints() {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  min_data_rate_ = std::max(min_target_rate_, kCongestionControllerMinBitrate);
  if (use_min_allocatable_as_lower_bound_) {
    min_data_rate_ = std::max(min_data_rate_, min_total_allocated_bitrate_);
  }
  if (max_data_rate_ < min_data_rate_) {
    RTC_LOG(LS_WARNING) << "max bitrate smaller than min bitrate";
    max_data_rate_ = min_data_rate_;
  }
  if (starting_rate_ && starting_rate_ < min_data_rate_) {
    RTC_LOG(LS_WARNING) << "start bitrate smaller than min bitrate";
    starting_rate_ = min_data_rate_;
  }
}

std::vector<ProbeClusterConfig> GoogCcNetworkController::ResetConstraints(
    TargetRateConstraints new_constraints) {
  min_target_rate_ = new_constraints.min_data_rate.value_or(DataRate::Zero());
  max_data_rate_ =
      new_constraints.max_data_rate.value_or(DataRate::PlusInfinity());
  starting_rate_ = new_constraints.starting_rate;
  ClampConstraints();

  bandwidth_estimation_->SetBitrates(starting_rate_, min_data_rate_,
                                     max_data_rate_, new_constraints.at_time);

  if (starting_rate_)
    delay_based_bwe_->SetStartBitrate(*starting_rate_);
  delay_based_bwe_->SetMinBitrate(min_data_rate_);

  return probe_controller_->SetBitrates(
      min_data_rate_, starting_rate_.value_or(DataRate::Zero()), max_data_rate_,
      new_constraints.at_time);
}

NetworkControlUpdate GoogCcNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  int64_t total_packets_delta =
      msg.packets_received_delta + msg.packets_lost_delta;
  bandwidth_estimation_->UpdatePacketsLost(
      msg.packets_lost_delta, total_packets_delta, msg.receive_time);
  return NetworkControlUpdate();
}

void GoogCcNetworkController::UpdateCongestionWindowSize() {
  TimeDelta min_feedback_max_rtt = TimeDelta::Millis(
      *std::min_element(feedback_max_rtts_.begin(), feedback_max_rtts_.end()));

  const DataSize kMinCwnd = DataSize::Bytes(2 * 1500);
  TimeDelta time_window =
      min_feedback_max_rtt +
      TimeDelta::Millis(
          rate_control_settings_.GetCongestionWindowAdditionalTimeMs());

  DataSize data_window = last_loss_based_target_rate_ * time_window;
  if (current_data_window_) {
    data_window =
        std::max(kMinCwnd, (data_window + current_data_window_.value()) / 2);
  } else {
    data_window = std::max(kMinCwnd, data_window);
  }
  current_data_window_ = data_window;
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
    TimeDelta min_pending_time = max_recv_time - feedback.receive_time;
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

  std::optional<Timestamp> alr_start_time =
      alr_detector_.GetApplicationLimitedRegionStartTime();
  if (previously_in_alr_ && !alr_start_time.has_value()) {
    acknowledged_bitrate_estimator_->SetAlrEndedTime(report.feedback_time);
    probe_controller_->SetAlrEndedTime(report.feedback_time);
  }
  previously_in_alr_ = alr_start_time.has_value();
  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      report.SortedByReceiveTime());
  auto acknowledged_bitrate = acknowledged_bitrate_estimator_->bitrate();
  bandwidth_estimation_->SetAcknowledgedRate(acknowledged_bitrate,
                                             report.feedback_time);
  for (const auto& feedback : report.SortedByReceiveTime()) {
    if (feedback.sent_packet.pacing_info.probe_cluster_id !=
        PacedPacketInfo::kNotAProbe) {
      probe_bitrate_estimator_->HandleProbeAndEstimateBitrate(feedback);
    }
  }

  if (network_estimator_) {
    network_estimator_->OnTransportPacketsFeedback(report);
    SetNetworkStateEstimate(network_estimator_->GetCurrentEstimate());
  }
  std::optional<DataRate> probe_bitrate =
      probe_bitrate_estimator_->FetchAndResetLastEstimatedBitrate();
  if (ignore_probes_lower_than_network_estimate_ && probe_bitrate &&
      estimate_ && *probe_bitrate < delay_based_bwe_->last_estimate() &&
      *probe_bitrate < estimate_->link_capacity_lower) {
    probe_bitrate.reset();
  }
  if (limit_probes_lower_than_throughput_estimate_ && probe_bitrate &&
      acknowledged_bitrate) {
    // Limit the backoff to something slightly below the acknowledged
    // bitrate. ("Slightly below" because we want to drain the queues
    // if we are actually overusing.)
    // The acknowledged bitrate shouldn't normally be higher than the delay
    // based estimate, but it could happen e.g. due to packet bursts or
    // encoder overshoot. We use std::min to ensure that a probe result
    // below the current BWE never causes an increase.
    DataRate limit =
        std::min(delay_based_bwe_->last_estimate(),
                 *acknowledged_bitrate * kProbeDropThroughputFraction);
    probe_bitrate = std::max(*probe_bitrate, limit);
  }

  NetworkControlUpdate update;
  bool recovered_from_overuse = false;

  DelayBasedBwe::Result result;
  result = delay_based_bwe_->IncomingPacketFeedbackVector(
      report, acknowledged_bitrate, probe_bitrate, estimate_,
      alr_start_time.has_value());

  if (result.updated) {
    if (result.probe) {
      bandwidth_estimation_->SetSendBitrate(result.target_bitrate,
                                            report.feedback_time);
    }
    // Since SetSendBitrate now resets the delay-based estimate, we have to
    // call UpdateDelayBasedEstimate after SetSendBitrate.
    bandwidth_estimation_->UpdateDelayBasedEstimate(report.feedback_time,
                                                    result.target_bitrate);
  }
  bandwidth_estimation_->UpdateLossBasedEstimator(
      report, result.delay_detector_state, probe_bitrate,
      alr_start_time.has_value());
  if (result.updated) {
    // Update the estimate in the ProbeController, in case we want to probe.
    MaybeTriggerOnNetworkChanged(&update, report.feedback_time);
  }

  recovered_from_overuse = result.recovered_from_overuse;

  if (recovered_from_overuse) {
    probe_controller_->SetAlrStartTime(alr_start_time);
    auto probes = probe_controller_->RequestProbe(report.feedback_time);
    update.probe_cluster_configs.insert(update.probe_cluster_configs.end(),
                                        probes.begin(), probes.end());
  }

  // No valid RTT could be because send-side BWE isn't used, in which case
  // we don't try to limit the outstanding packets.
  if (rate_control_settings_.UseCongestionWindow() &&
      max_feedback_rtt.IsFinite()) {
    UpdateCongestionWindowSize();
  }
  if (congestion_window_pushback_controller_ && current_data_window_) {
    congestion_window_pushback_controller_->SetDataWindow(
        *current_data_window_);
  } else {
    update.congestion_window = current_data_window_;
  }

  return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  if (!network_estimator_) {
    SetNetworkStateEstimate(msg);
  }
  return NetworkControlUpdate();
}

void GoogCcNetworkController::SetNetworkStateEstimate(
    std::optional<NetworkStateEstimate> estimate) {
  auto prev_estimate = estimate_;
  estimate_ = estimate;
  if (estimate_ && (!prev_estimate ||
                    estimate_->update_time != prev_estimate->update_time)) {
    env_.event_log().Log(std::make_unique<RtcEventRemoteEstimate>(
        estimate_->link_capacity_lower, estimate_->link_capacity_upper));
    probe_controller_->SetNetworkStateEstimate(*estimate_);
  }
}

NetworkControlUpdate GoogCcNetworkController::GetNetworkState(
    Timestamp at_time) const {
  NetworkControlUpdate update;
  update.target_rate = TargetTransferRate();
  update.target_rate->network_estimate.at_time = at_time;
  update.target_rate->network_estimate.loss_rate_ratio =
      last_estimated_fraction_loss_.value_or(0) / 255.0;
  update.target_rate->network_estimate.round_trip_time =
      last_estimated_round_trip_time_;
  update.target_rate->network_estimate.bwe_period =
      delay_based_bwe_->GetExpectedBwePeriod();

  update.target_rate->at_time = at_time;
  update.target_rate->target_rate = last_pushback_target_rate_;
  update.pacer_config = GetPacingRates(at_time);
  update.congestion_window = current_data_window_;
  return update;
}

void GoogCcNetworkController::MaybeTriggerOnNetworkChanged(
    NetworkControlUpdate* update,
    Timestamp at_time) {
  uint8_t fraction_loss = bandwidth_estimation_->fraction_loss();
  TimeDelta round_trip_time = bandwidth_estimation_->round_trip_time();
  DataRate loss_based_target_rate = bandwidth_estimation_->target_rate();
  LossBasedState loss_based_state = bandwidth_estimation_->loss_based_state();
  DataRate pushback_target_rate = loss_based_target_rate;

  double cwnd_reduce_ratio = 0.0;
  if (congestion_window_pushback_controller_) {
    int64_t pushback_rate =
        congestion_window_pushback_controller_->UpdateTargetBitrate(
            loss_based_target_rate.bps());
    pushback_rate = std::max<int64_t>(bandwidth_estimation_->GetMinBitrate(),
                                      pushback_rate);
    pushback_target_rate = DataRate::BitsPerSec(pushback_rate);
    if (rate_control_settings_.UseCongestionWindowDropFrameOnly()) {
      cwnd_reduce_ratio = static_cast<double>(loss_based_target_rate.bps() -
                                              pushback_target_rate.bps()) /
                          loss_based_target_rate.bps();
    }
  }

  if ((loss_based_target_rate != last_loss_based_target_rate_) ||
      (loss_based_state != last_loss_base_state_) ||
      (fraction_loss != last_estimated_fraction_loss_) ||
      (round_trip_time != last_estimated_round_trip_time_) ||
      (pushback_target_rate != last_pushback_target_rate_)) {
    last_loss_based_target_rate_ = loss_based_target_rate;
    last_pushback_target_rate_ = pushback_target_rate;
    last_estimated_fraction_loss_ = fraction_loss;
    last_estimated_round_trip_time_ = round_trip_time;
    last_loss_base_state_ = loss_based_state;

    alr_detector_.SetEstimatedBitrate(loss_based_target_rate);

    TimeDelta bwe_period = delay_based_bwe_->GetExpectedBwePeriod();

    TargetTransferRate target_rate_msg;
    target_rate_msg.at_time = at_time;
    if (rate_control_settings_.UseCongestionWindowDropFrameOnly()) {
      target_rate_msg.target_rate = loss_based_target_rate;
      target_rate_msg.cwnd_reduce_ratio = cwnd_reduce_ratio;
    } else {
      target_rate_msg.target_rate = pushback_target_rate;
    }
    target_rate_msg.network_estimate.at_time = at_time;
    target_rate_msg.network_estimate.round_trip_time = round_trip_time;
    target_rate_msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0f;
    target_rate_msg.network_estimate.bwe_period = bwe_period;

    update->target_rate = target_rate_msg;

    auto probes = probe_controller_->SetEstimatedBitrate(
        loss_based_target_rate,
        GetBandwidthLimitedCause(bandwidth_estimation_->loss_based_state(),
                                 bandwidth_estimation_->IsRttAboveLimit(),
                                 delay_based_bwe_->last_state()),
        at_time);
    update->probe_cluster_configs.insert(update->probe_cluster_configs.end(),
                                         probes.begin(), probes.end());
    update->pacer_config = GetPacingRates(at_time);
    RTC_LOG(LS_VERBOSE) << "bwe " << at_time.ms() << " pushback_target_bps="
                        << last_pushback_target_rate_.bps()
                        << " estimate_bps=" << loss_based_target_rate.bps();
  }
}

PacerConfig GoogCcNetworkController::GetPacingRates(Timestamp at_time) const {
  // Pacing rate is based on target rate before congestion window pushback,
  // because we don't want to build queues in the pacer when pushback occurs.
  DataRate pacing_rate =
      std::max(min_total_allocated_bitrate_, last_loss_based_target_rate_) *
      pacing_factor_;

  if (limit_pacingfactor_by_upper_link_capacity_estimate_ && estimate_ &&
      estimate_->link_capacity_upper.IsFinite() &&
      pacing_rate > estimate_->link_capacity_upper) {
    pacing_rate =
        std::max({estimate_->link_capacity_upper, min_total_allocated_bitrate_,
                  last_loss_based_target_rate_});
  }

  DataRate padding_rate =
      (last_loss_base_state_ == LossBasedState::kIncreaseUsingPadding)
          ? std::max(max_padding_rate_, last_loss_based_target_rate_)
          : max_padding_rate_;
  padding_rate = std::min(padding_rate, last_pushback_target_rate_);
  return PacerConfig::Create(at_time, pacing_rate, padding_rate,
                             pacing_time_window_);
}

}  // namespace webrtc
