/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/null_socket_server.h"

#include <cstdint>
#include <memory>

#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

TEST(NullSocketServerTest, WaitAndSet) {
  AutoThread main_thread;
  NullSocketServer ss;
  auto thread = Thread::Create();
  EXPECT_TRUE(thread->Start());
  thread->PostTask([&ss] { ss.WakeUp(); });
  // The process_io will be ignored.
  const bool process_io = true;
  EXPECT_THAT(
      WaitUntil([&] { return ss.Wait(SocketServer::kForever, process_io); },
                ::testing::IsTrue(), {.timeout = TimeDelta::Millis(5'000)}),
      IsRtcOk());
}

TEST(NullSocketServerTest, TestWait) {
  NullSocketServer ss;
  int64_t start = TimeMillis();
  ss.Wait(TimeDelta::Millis(200), true);
  // The actual wait time is dependent on the resolution of the timer used by
  // the Event class. Allow for the event to signal ~20ms early.
  EXPECT_GE(TimeSince(start), 180);
}

}  // namespace webrtc
