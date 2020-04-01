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

#include <stdint.h>

#include <memory>

#include "rtc_base/gunit.h"
#include "rtc_base/location.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace rtc {

static const uint32_t kTimeout = 5000U;

class NullSocketServerTest : public ::testing::Test, public MessageHandler {
 protected:
  void OnMessage(Message* message) override { ss_.WakeUp(); }

  NullSocketServer ss_;
};

TEST_F(NullSocketServerTest, WaitAndSet) {
  auto thread = Thread::Create();
  EXPECT_TRUE(thread->Start());
  thread->Post(RTC_FROM_HERE, this, 0);
  // The process_io will be ignored.
  const bool process_io = true;
  EXPECT_TRUE_WAIT(ss_.Wait(SocketServer::kForever, process_io), kTimeout);
}

TEST_F(NullSocketServerTest, TestWait) {
  int64_t start = TimeMillis();
  ss_.Wait(200, true);
  // The actual wait time is dependent on the resolution of the timer used by
  // the Event class. Allow for the event to signal ~20ms early.
  EXPECT_GE(TimeSince(start), 180);
}

}  // namespace rtc
