/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/control_handler.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

// When PacerPushbackExperiment is enabled, build-up in the pacer due to
// the congestion window and/or data spikes reduces encoder allocations.
bool IsPacerPushbackExperimentEnabled() {
  return field_trial::IsEnabled("WebRTC-PacerPushbackExperiment");
}

// By default, pacer emergency stops encoder when buffer reaches a high level.
bool IsPacerEmergencyStopDisabled() {
  return field_trial::IsEnabled("WebRTC-DisablePacerEmergencyStop");
}

}  // namespace
CongestionControlHandler::CongestionControlHandler(
    NetworkChangedObserver* observer,
    PacedSender* pacer)
    : observer_(observer),
      pacer_(pacer),
      pacer_pushback_experiment_(IsPacerPushbackExperimentEnabled()),
      disable_pacer_emergency_stop_(IsPacerEmergencyStopDisabled()) {
  sequenced_checker_.Detach();
}

CongestionControlHandler::~CongestionControlHandler() {}

void CongestionControlHandler::PostUpdates(NetworkControlUpdate update) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (update.congestion_window) {
    if (update.congestion_window->IsFinite())
      pacer_->SetCongestionWindow(update.congestion_window->bytes());
    else
      pacer_->SetCongestionWindow(PacedSender::kNoCongestionWindow);
  }
  if (update.pacer_config) {
    pacer_->SetPacingRates(update.pacer_config->data_rate().bps(),
                           update.pacer_config->pad_rate().bps());
  }
  for (const auto& probe : update.probe_cluster_configs) {
    int64_t bitrate_bps = probe.target_data_rate.bps();
    pacer_->CreateProbeCluster(bitrate_bps);
  }
  if (update.target_rate) {
    current_target_rate_msg_ = *update.target_rate;
    OnNetworkInvalidation();
  }
}

void CongestionControlHandler::OnNetworkAvailability(NetworkAvailability msg) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (network_available_ != msg.network_available) {
    network_available_ = msg.network_available;
    pacer_->UpdateOutstandingData(0);
    SetPacerState(!msg.network_available);
    OnNetworkInvalidation();
  }
}

void CongestionControlHandler::OnOutstandingData(DataSize in_flight_data) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_->UpdateOutstandingData(in_flight_data.bytes());
  OnNetworkInvalidation();
}

void CongestionControlHandler::OnPacerQueueUpdate(
    TimeDelta expected_queue_time) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  pacer_expected_queue_ms_ = expected_queue_time.ms();
  OnNetworkInvalidation();
}

void CongestionControlHandler::SetPacerState(bool paused) {
  if (paused && !pacer_paused_)
    pacer_->Pause();
  else if (!paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = paused;
}

void CongestionControlHandler::OnNetworkInvalidation() {
  if (!current_target_rate_msg_.has_value())
    return;

  uint32_t target_bitrate_bps = current_target_rate_msg_->target_rate.bps();
  int64_t rtt_ms =
      current_target_rate_msg_->network_estimate.round_trip_time.ms();
  float loss_rate_ratio =
      current_target_rate_msg_->network_estimate.loss_rate_ratio;

  int loss_ratio_255 = loss_rate_ratio * 255;
  uint8_t fraction_loss =
      rtc::dchecked_cast<uint8_t>(rtc::SafeClamp(loss_ratio_255, 0, 255));

  int64_t probing_interval_ms =
      current_target_rate_msg_->network_estimate.bwe_period.ms();

  if (!network_available_) {
    target_bitrate_bps = 0;
  } else if (pacer_pushback_experiment_) {
    int64_t queue_length_ms = pacer_expected_queue_ms_;

    if (queue_length_ms == 0) {
      encoding_rate_ratio_ = 1.0;
    } else if (queue_length_ms > 50) {
      double encoding_ratio = 1.0 - queue_length_ms / 1000.0;
      encoding_rate_ratio_ = std::min(encoding_rate_ratio_, encoding_ratio);
      encoding_rate_ratio_ = std::max(encoding_rate_ratio_, 0.0);
    }

    target_bitrate_bps *= encoding_rate_ratio_;
    target_bitrate_bps = target_bitrate_bps < 50000 ? 0 : target_bitrate_bps;
  } else if (!disable_pacer_emergency_stop_) {
    target_bitrate_bps = IsSendQueueFull() ? 0 : target_bitrate_bps;
  }

  if (HasNetworkParametersToReportChanged(target_bitrate_bps, fraction_loss,
                                          rtt_ms)) {
    observer_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt_ms,
                                probing_interval_ms);
  }
}
bool CongestionControlHandler::HasNetworkParametersToReportChanged(
    int64_t target_bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt_ms) {
  bool changed = last_reported_target_bitrate_bps_ != target_bitrate_bps ||
                 (target_bitrate_bps > 0 &&
                  (last_reported_fraction_loss_ != fraction_loss ||
                   last_reported_rtt_ms_ != rtt_ms));
  if (changed &&
      (last_reported_target_bitrate_bps_ == 0 || target_bitrate_bps == 0)) {
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: "
                     << target_bitrate_bps << " bps.";
  }
  last_reported_target_bitrate_bps_ = target_bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ms_ = rtt_ms;
  return changed;
}

bool CongestionControlHandler::IsSendQueueFull() const {
  return pacer_expected_queue_ms_ > PacedSender::kMaxQueueLengthMs;
}

absl::optional<TargetTransferRate>
CongestionControlHandler::last_transfer_rate() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  return current_target_rate_msg_;
}

}  // namespace webrtc
