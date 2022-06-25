/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/test_activities_executor.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void TestActivitiesExecutor::Start(TaskQueueForTest* task_queue) {
  RTC_DCHECK(task_queue);
  task_queue_ = task_queue;
  MutexLock lock(&lock_);
  start_time_ = Now();
  while (!scheduled_activities_.empty()) {
    PostActivity(std::move(scheduled_activities_.front()));
    scheduled_activities_.pop();
  }
}

void TestActivitiesExecutor::Stop() {
  if (task_queue_ == nullptr) {
    // Already stopped or not started.
    return;
  }
  task_queue_->SendTask(
      [this]() {
        MutexLock lock(&lock_);
        for (auto& handle : repeating_task_handles_) {
          handle.Stop();
        }
      },
      RTC_FROM_HERE);
  task_queue_ = nullptr;
}

void TestActivitiesExecutor::ScheduleActivity(
    TimeDelta initial_delay_since_start,
    absl::optional<TimeDelta> interval,
    std::function<void(TimeDelta)> func) {
  RTC_CHECK(initial_delay_since_start.IsFinite() &&
            initial_delay_since_start >= TimeDelta::Zero());
  RTC_CHECK(!interval ||
            (interval->IsFinite() && *interval > TimeDelta::Zero()));
  MutexLock lock(&lock_);
  ScheduledActivity activity(initial_delay_since_start, interval, func);
  if (start_time_.IsInfinite()) {
    scheduled_activities_.push(std::move(activity));
  } else {
    PostActivity(std::move(activity));
  }
}

void TestActivitiesExecutor::PostActivity(ScheduledActivity activity) {
  // Because start_time_ will never change at this point copy it to local
  // variable to capture in in lambda without requirement to hold a lock.
  Timestamp start_time = start_time_;

  TimeDelta remaining_delay =
      activity.initial_delay_since_start == TimeDelta::Zero()
          ? TimeDelta::Zero()
          : activity.initial_delay_since_start - (Now() - start_time);
  if (remaining_delay < TimeDelta::Zero()) {
    RTC_LOG(WARNING) << "Executing late task immediately, late by="
                     << ToString(remaining_delay.Abs());
    remaining_delay = TimeDelta::Zero();
  }

  if (activity.interval) {
    if (remaining_delay == TimeDelta::Zero()) {
      repeating_task_handles_.push_back(RepeatingTaskHandle::Start(
          task_queue_->Get(), [activity, start_time, this]() {
            activity.func(Now() - start_time);
            return *activity.interval;
          }));
      return;
    }
    repeating_task_handles_.push_back(RepeatingTaskHandle::DelayedStart(
        task_queue_->Get(), remaining_delay, [activity, start_time, this]() {
          activity.func(Now() - start_time);
          return *activity.interval;
        }));
    return;
  }

  if (remaining_delay == TimeDelta::Zero()) {
    task_queue_->PostTask(
        [activity, start_time, this]() { activity.func(Now() - start_time); });
    return;
  }

  task_queue_->PostDelayedTask(
      [activity, start_time, this]() { activity.func(Now() - start_time); },
      remaining_delay.ms());
}

Timestamp TestActivitiesExecutor::Now() const {
  return clock_->CurrentTime();
}

TestActivitiesExecutor::ScheduledActivity::ScheduledActivity(
    TimeDelta initial_delay_since_start,
    absl::optional<TimeDelta> interval,
    std::function<void(TimeDelta)> func)
    : initial_delay_since_start(initial_delay_since_start),
      interval(interval),
      func(std::move(func)) {}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
