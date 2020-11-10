/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/run_loop.h"

#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gtest.h"

namespace webrtc {

TEST(RunLoopTest, TaskQueueOnThread) {
  test::RunLoop loop;
  EXPECT_EQ(TaskQueueBase::Current(), loop.task_queue());
  EXPECT_TRUE(loop.task_queue()->IsCurrent());
}

TEST(RunLoopTest, Flush) {
  test::RunLoop loop;
  int counter = 0;
  loop.PostTask([&counter]() { ++counter; });
  EXPECT_EQ(counter, 0);
  loop.Flush();
  EXPECT_EQ(counter, 1);
}

TEST(RunLoopTest, Delayed) {
  test::RunLoop loop;
  bool ran = false;
  loop.PostDelayedTask(
      [&ran, &loop]() {
        ran = true;
        loop.Quit();
      },
      100);
  loop.Flush();
  EXPECT_FALSE(ran);
  loop.Run();
  EXPECT_TRUE(ran);
}

TEST(RunLoopTest, PostAndQuit) {
  test::RunLoop loop;
  bool ran = false;
  loop.PostTask([&ran, &loop]() {
    ran = true;
    loop.Quit();
  });
  loop.Run();
  EXPECT_TRUE(ran);
}

}  // namespace webrtc
