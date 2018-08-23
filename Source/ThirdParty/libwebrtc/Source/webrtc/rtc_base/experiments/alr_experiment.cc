/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/alr_experiment.h"

#include <string>

#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

const char AlrExperimentSettings::kScreenshareProbingBweExperimentName[] =
    "WebRTC-ProbingScreenshareBwe";
const char AlrExperimentSettings::kStrictPacingAndProbingExperimentName[] =
    "WebRTC-StrictPacingAndProbing";
const char kDefaultProbingScreenshareBweSettings[] = "1.0,2875,80,40,-60,3";

bool AlrExperimentSettings::MaxOneFieldTrialEnabled() {
  return field_trial::FindFullName(kStrictPacingAndProbingExperimentName)
             .empty() ||
         field_trial::FindFullName(kScreenshareProbingBweExperimentName)
             .empty();
}

absl::optional<AlrExperimentSettings>
AlrExperimentSettings::CreateFromFieldTrial(const char* experiment_name) {
  absl::optional<AlrExperimentSettings> ret;
  std::string group_name = field_trial::FindFullName(experiment_name);

  const std::string kIgnoredSuffix = "_Dogfood";
  std::string::size_type suffix_pos = group_name.rfind(kIgnoredSuffix);
  if (suffix_pos != std::string::npos &&
      suffix_pos == group_name.length() - kIgnoredSuffix.length()) {
    group_name.resize(group_name.length() - kIgnoredSuffix.length());
  }

  if (experiment_name == kScreenshareProbingBweExperimentName) {
    // This experiment is now default-on with fixed settings.
    // TODO(sprang): Remove this kill-switch and clean up experiment code.
    if (group_name != "Disabled") {
      group_name = kDefaultProbingScreenshareBweSettings;
    }
  }

  if (group_name.empty())
    return ret;

  AlrExperimentSettings settings;
  if (sscanf(group_name.c_str(), "%f,%" PRId64 ",%d,%d,%d,%d",
             &settings.pacing_factor, &settings.max_paced_queue_time,
             &settings.alr_bandwidth_usage_percent,
             &settings.alr_start_budget_level_percent,
             &settings.alr_stop_budget_level_percent,
             &settings.group_id) == 6) {
    ret.emplace(settings);
    RTC_LOG(LS_INFO) << "Using ALR experiment settings: "
                        "pacing factor: "
                     << settings.pacing_factor << ", max pacer queue length: "
                     << settings.max_paced_queue_time
                     << ", ALR start bandwidth usage percent: "
                     << settings.alr_bandwidth_usage_percent
                     << ", ALR end budget level percent: "
                     << settings.alr_start_budget_level_percent
                     << ", ALR end budget level percent: "
                     << settings.alr_stop_budget_level_percent
                     << ", ALR experiment group ID: " << settings.group_id;
  } else {
    RTC_LOG(LS_INFO) << "Failed to parse ALR experiment: " << experiment_name;
  }

  return ret;
}

}  // namespace webrtc
