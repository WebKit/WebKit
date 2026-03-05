//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef SYSTEM_WRAPPERS_INCLUDE_FIELD_TRIAL_H_
#define SYSTEM_WRAPPERS_INCLUDE_FIELD_TRIAL_H_

#include <string>

#include "absl/strings/string_view.h"

// Field trials allow webrtc clients (such as Chrome) to turn on feature code
// in binaries out in the field and gather information with that.
//
// Field trials interface provided in this file is deprecated.
// Please use `api/field_trials.h` to create field trials.
// Please use `api/field_trials_view.h` to query field trials.

namespace webrtc {
namespace field_trial {

// Optionally initialize field trial from a string.
// This method can be called at most once before any other call into webrtc.
// E.g. before the peer connection factory is constructed.
// Note: trials_string must never be destroyed.
// TODO: bugs.webrtc.org/42220378 - Delete after January 1, 2026.
[[deprecated(
    "Create FieldTrials and pass is where FieldTrialsView is expected")]]
void InitFieldTrialsFromString(const char* trials_string);

// Validates the given field trial string.
// TODO: bugs.webrtc.org/42220378 - Delete after January 1, 2026.
[[deprecated("Use FieldTrials::Create to validate field trial string")]]
bool FieldTrialsStringIsValid(absl::string_view trials_string);

// Merges two field trial strings.
//
// If a key (trial) exists twice with conflicting values (groups), the value
// in 'second' takes precedence.
// Shall only be called with valid FieldTrial strings.
// TODO: bugs.webrtc.org/42220378 - Delete after January 1, 2026.
[[deprecated("Use FieldTrials::Merge")]]
std::string MergeFieldTrialsStrings(absl::string_view first,
                                    absl::string_view second);

}  // namespace field_trial
}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_FIELD_TRIAL_H_
