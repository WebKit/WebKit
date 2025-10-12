/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// absl flags are not allowed when building with chromium, so if this helper
// happend to be used from chromium tests, disable populating field trials from
// the command line flag by default.

#include "absl/strings/string_view.h"
#include "api/field_trials.h"
#include "test/create_test_field_trials.h"

namespace webrtc {

FieldTrials CreateTestFieldTrials(absl::string_view s) {
  return FieldTrials(s);
}

}  // namespace webrtc
