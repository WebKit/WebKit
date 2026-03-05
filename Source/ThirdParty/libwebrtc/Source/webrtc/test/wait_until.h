/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_WAIT_UNTIL_H_
#define TEST_WAIT_UNTIL_H_

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "api/function_view.h"
#include "api/rtc_error.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until_internal.h"  // IWYU pragma: private

namespace webrtc {

using ClockVariant = std::variant<std::monostate,
                                  SimulatedClock*,
                                  FakeClock*,
                                  ThreadProcessingFakeClock*,
                                  TimeController*>;

struct WaitUntilSettings {
  // The maximum time to wait for the condition to be met.
  TimeDelta timeout = TimeDelta::Seconds(5);
  // The interval between polling the condition.
  TimeDelta polling_interval = TimeDelta::Millis(1);
  // The clock to use for timing.
  ClockVariant clock = std::monostate();
  // Name of the result to be used in the error message.
  std::string result_name = "result";
};

// Runs a function `fn`, until it returns true, or timeout from `settings`.
// Calls `fn` at least once. Returns true when `fn` return true, returns false
// after timeout if `fn` always returned false.
//
// Example:
//
//   EXPECT_TRUE(WaitUntil([&] { return client.IsConnected(); });
[[nodiscard]] bool WaitUntil(FunctionView<bool()> fn,
                             WaitUntilSettings settings = {});

// Runs a function `fn`, which returns a result, until `matcher` matches the
// result.
//
// The function is called repeatedly until the result matches the matcher or the
// timeout is reached. If the matcher matches the result, the result is
// returned. Otherwise, an error is returned.
//
// Example:
//
//   int counter = 0;
//   RTCErrorOr<int> result = WaitUntil([&] { return ++counter; }, Eq(3))
//   EXPECT_THAT(result, IsOkAndHolds(3));
template <typename Fn>
[[nodiscard]] RTCErrorOr<std::invoke_result_t<Fn>> WaitUntil(
    const Fn& fn,
    ::testing::Matcher<std::invoke_result_t<Fn>> matcher,
    WaitUntilSettings settings = {}) {
  // Wrap `result` into optional to support types that are not default
  // constructable.
  std::optional<std::invoke_result_t<Fn>> result;
  bool ok = WaitUntil(
      [&] {
        // `emplace` instead of assigning to support return types that do not
        // have an assign operator.
        result.emplace(fn());
        return ::testing::Value(*result, matcher);
      },
      settings);

  // WaitUntil promise to call `fn` at least once and thus `result` is
  // populated.
  RTC_CHECK(result.has_value());
  if (ok) {
    return *std::move(result);
  }

  ::testing::StringMatchResultListener listener;
  wait_until_internal::ExplainMatchResult(matcher, *result, &listener,
                                          settings.result_name);
  return RTCError(RTCErrorType::INTERNAL_ERROR, listener.str());
}

}  // namespace webrtc

#endif  // TEST_WAIT_UNTIL_H_
