/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_CREATE_TEST_FIELD_TRIALS_H_
#define TEST_CREATE_TEST_FIELD_TRIALS_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/field_trials.h"

namespace webrtc {

// Creates field trials from command line flag --force_fieldtrials
// and passed field trial string. Field trials in the `s` take priority over
// the command line flag.
// Crashes if command line flag or the `s` are not a valid field trial string.
//
// The intention of these functions is to be the default source of field trials
// in tests so that tests always use the command line flag.
// The behavior of these two functions is identical, they differ only in the
// return types for convenience.
FieldTrials CreateTestFieldTrials(absl::string_view s = "");
inline absl_nonnull std::unique_ptr<FieldTrials> CreateTestFieldTrialsPtr(
    absl::string_view s = "") {
  return std::make_unique<FieldTrials>(CreateTestFieldTrials(s));
}

}  // namespace webrtc

#endif  // TEST_CREATE_TEST_FIELD_TRIALS_H_
