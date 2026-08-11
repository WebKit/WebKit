/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/scoped_operations_batcher.h"

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/rtc_error.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {

ScopedOperationsBatcher::ScopedOperationsBatcher(Thread* target_thread)
    : target_thread_(target_thread) {
  RTC_DCHECK(target_thread_);
}

ScopedOperationsBatcher::~ScopedOperationsBatcher() {
  RTCError error = Run();
  if (!error.ok()) {
    RTC_LOG(LS_ERROR) << "Batcher failed: " << error.message();
  }
}

RTCError ScopedOperationsBatcher::Run() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  const bool target_thread_is_current = target_thread_->IsCurrent();

#if RTC_DCHECK_IS_ON
  RTC_LOG_THREAD_BLOCK_COUNT();
  int expected_block_count = tasks_.empty() ? 0 : 1;
  if (target_thread_is_current) {
    // Many tests in peerconnection_unittests run in single threaded mode where
    // the operations below will not be accurately measured. Yielding is not
    // supported for single threaded mode and tasks in those tests may
    // internally run blocking tasks themselves, which affects the count but is
    // not useful for the purposes of measuring the multithreaded behavior.
    RTC_IGNORE_THREAD_BLOCK_COUNT();
  }
#endif

  std::vector<FinalizerTask> return_tasks;
  size_t task_idx = 0;

  RTCError error = RTCError::OK();
  while (task_idx < tasks_.size()) {
    target_thread_->BlockingCall([&] {
      while (task_idx < tasks_.size()) {
        if (auto* void_task = std::get_if<SimpleBatchTask>(&tasks_[task_idx])) {
          std::move (*void_task)();
        } else {
          auto* returning_task =
              std::get_if<BatchTaskWithFinalizer>(&tasks_[task_idx]);
          RTC_DCHECK(returning_task);
          auto ret = std::move(*returning_task)();
          if (ret.ok()) {
            if (ret.value()) {
              return_tasks.push_back(std::move(ret.value()));
            }
          } else {
            error = ret.MoveError();
          }
        }
        ++task_idx;
        if (!error.ok()) {
          return;
        }
        if (!target_thread_is_current && target_thread_->HasPendingTasks()) {
#if RTC_DCHECK_IS_ON
          ++expected_block_count;
#endif
          return;
        }
      }
    });
    if (!error.ok()) {
      break;
    }
  }

  tasks_.clear();

  for (auto& task : return_tasks) {
    std::move(task)();
  }

#if RTC_DCHECK_IS_ON
  // If this triggers, then likely one of the `return_tasks` has issued a
  // blocking call.
  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(expected_block_count);
#endif
  return error;
}

void ScopedOperationsBatcher::Add(SimpleBatchTask task) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task) {
    tasks_.emplace_back(std::move(task));
  }
}

void ScopedOperationsBatcher::AddWithFinalizer(BatchTaskWithFinalizer task) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (task) {
    tasks_.emplace_back(std::move(task));
  }
}

}  // namespace webrtc
