// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#include "system_wrappers/include/field_trial.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/deprecated_global_field_trials.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

// Simple field trial implementation, which allows client to
// specify desired flags in InitFieldTrialsFromString.
namespace webrtc {
namespace field_trial {
namespace {

constexpr char kPersistentStringSeparator = '/';

// Validates the given field trial string.
//  E.g.:
//    "WebRTC-experimentFoo/Enabled/WebRTC-experimentBar/Enabled100kbps/"
//    Assigns the process to group "Enabled" on WebRTCExperimentFoo trial
//    and to group "Enabled100kbps" on WebRTCExperimentBar.
//
//  E.g. invalid config:
//    "WebRTC-experiment1/Enabled"  (note missing / separator at the end).
bool FieldTrialsStringIsValidInternal(const absl::string_view trials) {
  if (trials.empty())
    return true;

  size_t next_item = 0;
  std::map<absl::string_view, absl::string_view> field_trials;
  while (next_item < trials.length()) {
    size_t name_end = trials.find(kPersistentStringSeparator, next_item);
    if (name_end == absl::string_view::npos || next_item == name_end)
      return false;
    size_t group_name_end =
        trials.find(kPersistentStringSeparator, name_end + 1);
    if (group_name_end == absl::string_view::npos ||
        name_end + 1 == group_name_end)
      return false;
    absl::string_view name = trials.substr(next_item, name_end - next_item);
    absl::string_view group_name =
        trials.substr(name_end + 1, group_name_end - name_end - 1);

    next_item = group_name_end + 1;

    // Fail if duplicate with different group name.
    if (field_trials.find(name) != field_trials.end() &&
        field_trials.find(name)->second != group_name) {
      return false;
    }

    field_trials[name] = group_name;
  }

  return true;
}

}  // namespace

bool FieldTrialsStringIsValid(absl::string_view trials_string) {
  return FieldTrialsStringIsValidInternal(trials_string);
}

void InsertOrReplaceFieldTrialStringsInMap(
    std::map<std::string, std::string>* fieldtrial_map,
    const absl::string_view trials_string) {
  if (FieldTrialsStringIsValidInternal(trials_string)) {
    std::vector<absl::string_view> tokens = split(trials_string, '/');
    // Skip last token which is empty due to trailing '/'.
    for (size_t idx = 0; idx < tokens.size() - 1; idx += 2) {
      (*fieldtrial_map)[std::string(tokens[idx])] =
          std::string(tokens[idx + 1]);
    }
  } else {
    RTC_DCHECK_NOTREACHED() << "Invalid field trials string:" << trials_string;
  }
}

std::string MergeFieldTrialsStrings(absl::string_view first,
                                    absl::string_view second) {
  std::map<std::string, std::string> fieldtrial_map;
  InsertOrReplaceFieldTrialStringsInMap(&fieldtrial_map, first);
  InsertOrReplaceFieldTrialStringsInMap(&fieldtrial_map, second);

  // Merge into fieldtrial string.
  std::string merged = "";
  for (auto const& fieldtrial : fieldtrial_map) {
    merged += fieldtrial.first + '/' + fieldtrial.second + '/';
  }
  return merged;
}

// Optionally initialize field trial from a string.
void InitFieldTrialsFromString(const char* trials_string) {
  RTC_LOG(LS_INFO) << "Setting field trial string:" << trials_string;
  if (trials_string) {
    RTC_DCHECK(FieldTrialsStringIsValidInternal(trials_string))
        << "Invalid field trials string:" << trials_string;
  };
  DeprecatedGlobalFieldTrials::Set(trials_string);
}

}  // namespace field_trial
}  // namespace webrtc
