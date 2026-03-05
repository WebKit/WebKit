/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/environment/deprecated_global_field_trials.h"

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {
namespace {

constinit const char* global_field_trial_string = nullptr;

}  // namespace

void DeprecatedGlobalFieldTrials::Set(const char* field_trials) {
  global_field_trial_string = field_trials;
}

std::string DeprecatedGlobalFieldTrials::GetValue(absl::string_view key) const {
  if (global_field_trial_string == nullptr)
    return std::string();

  absl::string_view trials_string(global_field_trial_string);
  if (trials_string.empty())
    return std::string();

  size_t next_item = 0;
  while (next_item < trials_string.length()) {
    // Find next name/value pair in field trial configuration string.
    size_t field_name_end = trials_string.find('/', next_item);
    if (field_name_end == trials_string.npos || field_name_end == next_item)
      break;
    size_t field_value_end = trials_string.find('/', field_name_end + 1);
    if (field_value_end == trials_string.npos ||
        field_value_end == field_name_end + 1)
      break;
    absl::string_view field_name =
        trials_string.substr(next_item, field_name_end - next_item);
    absl::string_view field_value = trials_string.substr(
        field_name_end + 1, field_value_end - field_name_end - 1);
    next_item = field_value_end + 1;

    if (key == field_name)
      return std::string(field_value);
  }
  return std::string();
}
}  // namespace webrtc
