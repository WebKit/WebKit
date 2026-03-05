/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/alr_detector.h"

#include <memory>
#include <optional>

#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

AlrDetector::AlrDetectorConfig::AlrDetectorConfig(
    const FieldTrialsView& key_value_config) {
  RTC_CHECK(AlrExperimentSettings::MaxOneFieldTrialEnabled(key_value_config));
  std::optional<AlrExperimentSettings> experiment_settings =
      AlrExperimentSettings::CreateFromFieldTrial(
          key_value_config,
          AlrExperimentSettings::kScreenshareProbingBweExperimentName);
  if (!experiment_settings) {
    experiment_settings = AlrExperimentSettings::CreateFromFieldTrial(
        key_value_config,
        AlrExperimentSettings::kStrictPacingAndProbingExperimentName);
  }
  if (experiment_settings) {
    bandwidth_usage_ratio =
        experiment_settings->alr_bandwidth_usage_percent / 100.0;
    start_budget_level_ratio =
        experiment_settings->alr_start_budget_level_percent / 100.0;
    stop_budget_level_ratio =
        experiment_settings->alr_stop_budget_level_percent / 100.0;
  }
  StructParametersParser::Create(          //
      "bw_usage", &bandwidth_usage_ratio,  //
      "start", &start_budget_level_ratio,  //
      "stop", &stop_budget_level_ratio)
      ->Parse(key_value_config.Lookup("WebRTC-AlrDetectorParameters"));
}

AlrDetector::AlrDetector(const Environment& env)
    : env_(env), conf_(env_.field_trials()), alr_budget_(0, true) {}

AlrDetector::~AlrDetector() = default;

void AlrDetector::OnBytesSent(DataSize bytes_sent, Timestamp send_time) {
  if (!last_send_time_.has_value()) {
    last_send_time_ = send_time;
    // Since the duration for sending the bytes is unknwon, return without
    // updating alr state.
    return;
  }
  TimeDelta delta_time = send_time - *last_send_time_;
  last_send_time_ = send_time;

  alr_budget_.UseBudget(bytes_sent.bytes());
  alr_budget_.IncreaseBudget(delta_time.ms());
  bool state_changed = false;
  if (alr_budget_.budget_ratio() > conf_.start_budget_level_ratio &&
      !alr_started_time_) {
    alr_started_time_ = env_.clock().CurrentTime();
    state_changed = true;
  } else if (alr_budget_.budget_ratio() < conf_.stop_budget_level_ratio &&
             alr_started_time_) {
    state_changed = true;
    alr_started_time_ = std::nullopt;
  }
  if (state_changed) {
    env_.event_log().Log(
        std::make_unique<RtcEventAlrState>(alr_started_time_.has_value()));
  }
}

void AlrDetector::SetEstimatedBitrate(DataRate bitrate) {
  RTC_DCHECK_GT(bitrate, DataRate::Zero());
  alr_budget_.set_target_rate_kbps(
      (bitrate * conf_.bandwidth_usage_ratio).kbps());
}

std::optional<Timestamp> AlrDetector::GetApplicationLimitedRegionStartTime()
    const {
  return alr_started_time_;
}

}  // namespace webrtc
