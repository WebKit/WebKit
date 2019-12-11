/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/yield_policy.h"

#include <thread>  // Not allowed in production per Chromium style guide.

#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {
namespace {
class MockYieldHandler : public YieldInterface {
 public:
  MOCK_METHOD0(YieldExecution, void());
};
}  // namespace
TEST(YieldPolicyTest, HandlerReceivesYieldSignalWhenSet) {
  ::testing::StrictMock<MockYieldHandler> handler;
  {
    Event event;
    EXPECT_CALL(handler, YieldExecution()).Times(1);
    ScopedYieldPolicy policy(&handler);
    event.Set();
    event.Wait(Event::kForever);
  }
  {
    Event event;
    EXPECT_CALL(handler, YieldExecution()).Times(0);
    event.Set();
    event.Wait(Event::kForever);
  }
}

TEST(YieldPolicyTest, IsThreadLocal) {
  Event events[3];
  std::thread other_thread([&]() {
    ::testing::StrictMock<MockYieldHandler> local_handler;
    // The local handler is never called as we never Wait on this thread.
    EXPECT_CALL(local_handler, YieldExecution()).Times(0);
    ScopedYieldPolicy policy(&local_handler);
    events[0].Set();
    events[1].Set();
    events[2].Set();
  });

  // Waiting until the other thread has entered the scoped policy.
  events[0].Wait(Event::kForever);
  // Wait on this thread should not trigger the handler of that policy as it's
  // thread local.
  events[1].Wait(Event::kForever);

  // We can set a policy that's active on this thread independently.
  ::testing::StrictMock<MockYieldHandler> main_handler;
  EXPECT_CALL(main_handler, YieldExecution()).Times(1);
  ScopedYieldPolicy policy(&main_handler);
  events[2].Wait(Event::kForever);
  other_thread.join();
}
}  // namespace rtc
