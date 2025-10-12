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
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace wait_until_internal {

Timestamp GetTimeFromClockVariant(const ClockVariant& clock) {
  return std::visit(
      absl::Overload{
          [](const std::monostate&) { return Timestamp::Micros(TimeMicros()); },
          [](SimulatedClock* clock) { return clock->CurrentTime(); },
          [](TimeController* time_controller) {
            return time_controller->GetClock()->CurrentTime();
          },
          [](auto* clock) {
            return Timestamp::Micros(clock->TimeNanos() / 1000);
          },
      },
      clock);
}

void AdvanceTimeOnClockVariant(ClockVariant& clock, TimeDelta delta) {
  std::visit(absl::Overload{
                 [&](const std::monostate&) {
                   Thread::Current()->ProcessMessages(0);
                   Thread::Current()->SleepMs(delta.ms());
                 },
                 [&](auto* clock) { clock->AdvanceTime(delta); },
             },
             clock);
}

}  // namespace wait_until_internal
}  // namespace webrtc
