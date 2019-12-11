/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_QUEUE_FOR_TEST_H_
#define RTC_BASE_TASK_QUEUE_FOR_TEST_H_

#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE TaskQueueForTest : public rtc::TaskQueue {
 public:
  using rtc::TaskQueue::TaskQueue;
  explicit TaskQueueForTest(absl::string_view name = "TestQueue",
                            Priority priority = Priority::NORMAL);
  TaskQueueForTest(const TaskQueueForTest&) = delete;
  TaskQueueForTest& operator=(const TaskQueueForTest&) = delete;
  ~TaskQueueForTest() = default;

  // A convenience, test-only method that blocks the current thread while
  // a task executes on the task queue.
  // This variant is specifically for posting custom QueuedTask derived
  // implementations that tests do not want to pass ownership of over to the
  // task queue (i.e. the Run() method always returns |false|.).
  template <class Closure>
  void SendTask(Closure* task) {
    RTC_DCHECK(!IsCurrent());
    rtc::Event event;
    PostTask(ToQueuedTask(
        [&task] { RTC_CHECK_EQ(false, static_cast<QueuedTask*>(task)->Run()); },
        [&event] { event.Set(); }));
    event.Wait(rtc::Event::kForever);
  }

  // A convenience, test-only method that blocks the current thread while
  // a task executes on the task queue.
  template <class Closure>
  void SendTask(Closure&& task) {
    RTC_DCHECK(!IsCurrent());
    rtc::Event event;
    PostTask(
        ToQueuedTask(std::forward<Closure>(task), [&event] { event.Set(); }));
    event.Wait(rtc::Event::kForever);
  }
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_QUEUE_FOR_TEST_H_
