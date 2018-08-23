/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/field_trial.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/field_trial_default.h"

namespace webrtc {
namespace {
bool field_trials_initiated_ = false;
}  // namespace

namespace test {

void ValidateFieldTrialsStringOrDie(const std::string& trials_string) {
  static const char kPersistentStringSeparator = '/';

  // Catch an error if this is called more than once.
  assert(!field_trials_initiated_);
  field_trials_initiated_ = true;

  if (trials_string.empty())
    return;

  size_t next_item = 0;
  std::map<std::string, std::string> field_trials;
  while (next_item < trials_string.length()) {
    size_t name_end = trials_string.find(kPersistentStringSeparator, next_item);
    if (name_end == trials_string.npos || next_item == name_end)
      break;
    size_t group_name_end =
        trials_string.find(kPersistentStringSeparator, name_end + 1);
    if (group_name_end == trials_string.npos || name_end + 1 == group_name_end)
      break;
    std::string name(trials_string, next_item, name_end - next_item);
    std::string group_name(trials_string, name_end + 1,
                           group_name_end - name_end - 1);
    next_item = group_name_end + 1;

    // Fail if duplicate with different group name.
    if (field_trials.find(name) != field_trials.end() &&
        field_trials.find(name)->second != group_name) {
      break;
    }

    field_trials[name] = group_name;

    // Successfully parsed all field trials from the string.
    if (next_item == trials_string.length()) {
      // webrtc::field_trial::InitFieldTrialsFromString(trials_string.c_str());
      return;
    }
  }
  // Using fprintf as RTC_LOG does not print when this is called early in main.
  fprintf(stderr, "Invalid field trials string.\n");

  // Using abort so it crashes in both debug and release mode.
  abort();
}

ScopedFieldTrials::ScopedFieldTrials(const std::string& config)
    : previous_field_trials_(webrtc::field_trial::GetFieldTrialString()) {
  assert(field_trials_initiated_);
  field_trials_initiated_ = false;
  current_field_trials_ = config;
  ValidateFieldTrialsStringOrDie(current_field_trials_);
  webrtc::field_trial::InitFieldTrialsFromString(current_field_trials_.c_str());
}

ScopedFieldTrials::~ScopedFieldTrials() {
  // Should still be initialized, since InitFieldTrials is called from ctor.
  // That's why we don't restore the flag.
  assert(field_trials_initiated_);
  webrtc::field_trial::InitFieldTrialsFromString(previous_field_trials_);
}

}  // namespace test
}  // namespace webrtc
