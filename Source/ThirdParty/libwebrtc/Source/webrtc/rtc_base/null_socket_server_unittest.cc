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

#include <memory>

#include "api/environment/environment.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {

TEST(NullSocketServerTest, WaitAndSet) {
  test::RunLoop run_loop;
  NullSocketServer ss;
  auto thread = Thread::Create();
  EXPECT_TRUE(thread->Start());
  thread->PostTask([&ss] { ss.WakeUp(); });
  // The process_io will be ignored.
  const bool process_io = true;
  bool wait_result = false;
  run_loop.PostTask([&] {
    wait_result = ss.Wait(SocketServer::kForever, process_io);
    run_loop.Quit();
  });
  run_loop.RunFor(TimeDelta::Seconds(5));
  EXPECT_TRUE(wait_result);
}

TEST(NullSocketServerTest, TestWait) {
  Environment env = CreateTestEnvironment();
  NullSocketServer ss;
  Timestamp start = env.clock().CurrentTime();
  ss.Wait(TimeDelta::Millis(200), true);
  // The actual wait time is dependent on the resolution of the timer used by
  // the Event class. Allow for the event to signal ~20ms early.
  EXPECT_GE(env.clock().CurrentTime() - start, TimeDelta::Millis(180));
}

}  // namespace webrtc
