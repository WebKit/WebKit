/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/real_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;

enum class TimeMode { kRealTime, kSimulated };

std::unique_ptr<TimeController> CreateTimeController(TimeMode mode) {
  switch (mode) {
    case TimeMode::kRealTime:
      return std::make_unique<RealTimeController>();
    case TimeMode::kSimulated:
      // Using an offset of 100000 to get nice fixed width and readable
      // timestamps in typical test scenarios.
      constexpr Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
      return std::make_unique<GlobalSimulatedTimeController>(
          kSimulatedStartTime);
  }
}

std::string ParamsToString(const TestParamInfo<webrtc::TimeMode>& param) {
  switch (param.param) {
    case webrtc::TimeMode::kRealTime:
      return "RealTime";
    case webrtc::TimeMode::kSimulated:
      return "SimulatedTime";
    default:
      RTC_DCHECK_NOTREACHED() << "Time mode not supported";
  }
}

// Keeps order of executions. May be called from different threads.
class ExecutionOrderKeeper {
 public:
  void Executed(int execution_id) {
    MutexLock lock(&mutex_);
    order_.push_back(execution_id);
  }

  std::vector<int> order() const {
    MutexLock lock(&mutex_);
    return order_;
  }

 private:
  mutable Mutex mutex_;
  std::vector<int> order_ RTC_GUARDED_BY(mutex_);
};

// Tests conformance between real time and simulated time time controller.
class SimulatedRealTimeControllerConformanceTest
    : public TestWithParam<webrtc::TimeMode> {};

TEST_P(SimulatedRealTimeControllerConformanceTest, ThreadPostOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // Tasks on thread have to be executed in order in which they were
  // posted.
  ExecutionOrderKeeper execution_order;
  thread->PostTask([&]() { execution_order.Executed(1); });
  thread->PostTask([&]() { execution_order.Executed(2); });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(SimulatedRealTimeControllerConformanceTest, ThreadPostDelayedOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  ExecutionOrderKeeper execution_order;
  thread->PostDelayedTask([&]() { execution_order.Executed(2); },
                          TimeDelta::Millis(500));
  thread->PostTask([&]() { execution_order.Executed(1); });
  time_controller->AdvanceTime(TimeDelta::Millis(600));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(SimulatedRealTimeControllerConformanceTest, ThreadPostInvokeOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // Tasks on thread have to be executed in order in which they were
  // posted/invoked.
  ExecutionOrderKeeper execution_order;
  thread->PostTask([&]() { execution_order.Executed(1); });
  thread->Invoke<void>(RTC_FROM_HERE, [&]() { execution_order.Executed(2); });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(SimulatedRealTimeControllerConformanceTest,
       ThreadPostInvokeFromThreadOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // If task is invoked from thread X on thread X it has to be executed
  // immediately.
  ExecutionOrderKeeper execution_order;
  thread->PostTask([&]() {
    thread->PostTask([&]() { execution_order.Executed(2); });
    thread->Invoke<void>(RTC_FROM_HERE, [&]() { execution_order.Executed(1); });
  });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(SimulatedRealTimeControllerConformanceTest,
       TaskQueuePostEventWaitOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  auto task_queue = time_controller->GetTaskQueueFactory()->CreateTaskQueue(
      "task_queue", webrtc::TaskQueueFactory::Priority::NORMAL);

  // Tasks on thread have to be executed in order in which they were
  // posted/invoked.
  ExecutionOrderKeeper execution_order;
  rtc::Event event;
  task_queue->PostTask([&]() { execution_order.Executed(1); });
  task_queue->PostTask([&]() {
    execution_order.Executed(2);
    event.Set();
  });
  EXPECT_TRUE(event.Wait(/*give_up_after_ms=*/100,
                         /*warn_after_ms=*/10'000));
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

INSTANTIATE_TEST_SUITE_P(ConformanceTest,
                         SimulatedRealTimeControllerConformanceTest,
                         Values(TimeMode::kRealTime, TimeMode::kSimulated),
                         ParamsToString);

}  // namespace
}  // namespace webrtc
