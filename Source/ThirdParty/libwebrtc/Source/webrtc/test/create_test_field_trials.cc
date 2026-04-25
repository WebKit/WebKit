/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/create_test_field_trials.h"

#include <string>

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "api/field_trials.h"

ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

namespace webrtc {

FieldTrials CreateTestFieldTrials(absl::string_view s) {
  FieldTrials result(absl::GetFlag(FLAGS_force_fieldtrials));
  result.Merge(FieldTrials(s));
  return result;
}

}  // namespace webrtc
