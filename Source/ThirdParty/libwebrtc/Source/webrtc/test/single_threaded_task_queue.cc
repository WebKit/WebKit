/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/single_threaded_task_queue.h"

#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace test {

SingleThreadedTaskQueueForTesting::QueuedTask::QueuedTask(
    SingleThreadedTaskQueueForTesting::TaskId task_id,
    int64_t earliest_execution_time,
    SingleThreadedTaskQueueForTesting::Task task)
    : task_id(task_id),
      earliest_execution_time(earliest_execution_time),
      task(task) {}

SingleThreadedTaskQueueForTesting::QueuedTask::~QueuedTask() = default;

SingleThreadedTaskQueueForTesting::SingleThreadedTaskQueueForTesting(
    const char* name)
    : thread_(Run, this, name), running_(true), next_task_id_(0) {
  thread_.Start();
}

SingleThreadedTaskQueueForTesting::~SingleThreadedTaskQueueForTesting() {
  RTC_DCHECK_RUN_ON(&owner_thread_checker_);
  {
    rtc::CritScope lock(&cs_);
    running_ = false;
  }
  wake_up_.Set();
  thread_.Stop();
}

SingleThreadedTaskQueueForTesting::TaskId
SingleThreadedTaskQueueForTesting::PostTask(Task task) {
  return PostDelayedTask(task, 0);
}

SingleThreadedTaskQueueForTesting::TaskId
SingleThreadedTaskQueueForTesting::PostDelayedTask(Task task,
                                                   int64_t delay_ms) {
  int64_t earliest_exec_time = rtc::TimeAfter(delay_ms);

  rtc::CritScope lock(&cs_);

  TaskId id = next_task_id_++;

  // Insert after any other tasks with an earlier-or-equal target time.
  auto it = tasks_.begin();
  for (; it != tasks_.end(); it++) {
    if (earliest_exec_time < (*it)->earliest_execution_time) {
      break;
    }
  }
  tasks_.insert(it,
                absl::make_unique<QueuedTask>(id, earliest_exec_time, task));

  // This class is optimized for simplicty, not for performance. This will wake
  // the thread up even if the next task in the queue is only scheduled for
  // quite some time from now. In that case, the thread will just send itself
  // back to sleep.
  wake_up_.Set();

  return id;
}

void SingleThreadedTaskQueueForTesting::SendTask(Task task) {
  rtc::Event done;
  PostTask([&task, &done]() {
    task();
    done.Set();
  });
  done.Wait(rtc::Event::kForever);
}

bool SingleThreadedTaskQueueForTesting::CancelTask(TaskId task_id) {
  rtc::CritScope lock(&cs_);
  for (auto it = tasks_.begin(); it != tasks_.end(); it++) {
    if ((*it)->task_id == task_id) {
      tasks_.erase(it);
      return true;
    }
  }
  return false;
}

void SingleThreadedTaskQueueForTesting::Run(void* obj) {
  static_cast<SingleThreadedTaskQueueForTesting*>(obj)->RunLoop();
}

void SingleThreadedTaskQueueForTesting::RunLoop() {
  while (true) {
    std::unique_ptr<QueuedTask> queued_task;

    // An empty queue would lead to sleeping until the queue becoems non-empty.
    // A queue where the earliest task is scheduled for later than now, will
    // lead to sleeping until the time of the next scheduled task (or until
    // more tasks are scheduled).
    int wait_time = rtc::Event::kForever;

    {
      rtc::CritScope lock(&cs_);
      if (!running_) {
        return;
      }
      if (!tasks_.empty()) {
        int64_t remaining_delay_ms = rtc::TimeDiff(
            tasks_.front()->earliest_execution_time, rtc::TimeMillis());
        if (remaining_delay_ms <= 0) {
          queued_task = std::move(tasks_.front());
          tasks_.pop_front();
        } else {
          wait_time = rtc::saturated_cast<int>(remaining_delay_ms);
        }
      }
    }

    if (queued_task) {
      queued_task->task();
    } else {
      wake_up_.Wait(wait_time);
    }
  }
}

}  // namespace test
}  // namespace webrtc
