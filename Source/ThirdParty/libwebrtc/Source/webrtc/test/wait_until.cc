/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/wait_until.h"

#include <variant>

#include "absl/functional/overload.h"
#include "api/function_view.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

[[nodiscard]] bool WaitUntil(FunctionView<bool()> fn,
                             WaitUntilSettings settings) {
  if (std::holds_alternative<std::monostate>(settings.clock)) {
    RTC_CHECK(Thread::Current()) << "A current thread is required. An "
                                    "webrtc::AutoThread can work for tests.";
  }

  auto now = [&] {
    return std::visit(
        absl::Overload{
            [](const std::monostate&) {
              return Timestamp::Micros(TimeMicros());
            },
            [](SimulatedClock* clock) { return clock->CurrentTime(); },
            [](TimeController* time_controller) {
              return time_controller->GetClock()->CurrentTime();
            },
            [](auto* clock) {
              return Timestamp::Micros(clock->TimeNanos() / 1000);
            },
        },
        settings.clock);
  };

  auto sleep = [&](TimeDelta delta) {
    std::visit(absl::Overload{
                   [&](const std::monostate&) {
                     Thread::Current()->ProcessMessages(0);
                     Thread::Current()->SleepMs(delta.ms());
                   },
                   [&](auto* clock) { clock->AdvanceTime(delta); },
               },
               settings.clock);
  };

  if (fn()) {
    return true;
  }

  Timestamp deadline = now() + settings.timeout;

  // Run pending tasks first as they might change result of the `fn` and
  // thus avoid unnecessary advancing time.
  sleep(TimeDelta::Zero());

  for (;;) {
    if (fn()) {
      return true;
    } else if (now() >= deadline) {
      return false;
    }
    sleep(settings.polling_interval);
  }
}

}  // namespace webrtc
