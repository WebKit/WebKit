/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/simulated_time_task_queue_controller.h"

#include <memory>

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(SimulatedTimeTaskQueueControllerTest, SetsCurrent) {
  SimulatedTimeTaskQueueController controller(Timestamp::Zero());
  auto task_queue = controller.GetTaskQueueFactory()->CreateTaskQueue(
      "TestQueue", TaskQueueFactory::Priority::kNormal);

  bool done = false;
  task_queue->PostTask([&] {
    EXPECT_EQ(TaskQueueBase::Current(), task_queue.get());
    done = true;
  });

  controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_TRUE(done);
}

TEST(SimulatedTimeTaskQueueControllerTest, TaskIsExecuted) {
  SimulatedTimeTaskQueueController controller(Timestamp::Zero());
  auto task_queue = controller.GetTaskQueueFactory()->CreateTaskQueue(
      "TestQueue", TaskQueueFactory::Priority::kNormal);

  bool done = false;
  task_queue->PostTask([&] { done = true; });

  EXPECT_FALSE(done);
  controller.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_TRUE(done);
}

TEST(SimulatedTimeTaskQueueControllerTest, DelayedTaskIsExecuted) {
  SimulatedTimeTaskQueueController controller(Timestamp::Zero());
  auto task_queue = controller.GetTaskQueueFactory()->CreateTaskQueue(
      "TestQueue", TaskQueueFactory::Priority::kNormal);

  bool done = false;
  task_queue->PostDelayedTask([&] { done = true; }, TimeDelta::Millis(10));

  EXPECT_FALSE(done);

  controller.AdvanceTime(TimeDelta::Millis(9));
  EXPECT_FALSE(done);

  controller.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_TRUE(done);
}

}  // namespace
}  // namespace webrtc
