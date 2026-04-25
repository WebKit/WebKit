/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_CREATE_TEST_ENVIRONMENT_H_
#define TEST_CREATE_TEST_ENVIRONMENT_H_

#include <memory>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/field_trials.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/test/time_controller.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Creates Environment for unittests. Uses test specific defaults unlike the
// production CreateEnvironment.
// Supports test only interface TimeController for testing with simulated time.
// TODO: bugs.webrtc.org/437878267 - Remove `FieldTrialsView*` variant when
// tests are refactored not to rely on it.
struct CreateTestEnvironmentOptions {
  std::variant<absl::string_view,
               const FieldTrialsView * absl_nullable,
               absl_nonnull std::unique_ptr<FieldTrialsView>,
               FieldTrials>
      field_trials;
  std::variant<Clock * absl_nullable,  //
               TimeController * absl_nonnull>
      time;
  RtcEventLog* event_log = nullptr;
};
Environment CreateTestEnvironment(CreateTestEnvironmentOptions o = {});

}  // namespace webrtc

#endif  // TEST_CREATE_TEST_ENVIRONMENT_H_
