/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCELABLE_PERIODIC_TASK_H_
#define RTC_BASE_CANCELABLE_PERIODIC_TASK_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/cancelable_task_handle.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace rtc {
namespace cancelable_periodic_task_internal {
// CancelablePeriodicTask runs a closure multiple times with delay decided
// by the return value of the closure itself.
// The task can be canceled with the handle returned by GetCancelationHandle().
// Note that the task can only be canceled on the task queue where it runs.
template <typename Closure>
class CancelablePeriodicTask final : public BaseCancelableTask {
 public:
  // |closure| should return time in ms until next run.
  explicit CancelablePeriodicTask(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  CancelablePeriodicTask(const CancelablePeriodicTask&) = delete;
  CancelablePeriodicTask& operator=(const CancelablePeriodicTask&) = delete;
  ~CancelablePeriodicTask() override = default;

 private:
  bool Run() override {
    // See QueuedTask::Run documentaion for return values meaning.
    if (BaseCancelableTask::Canceled())
      return true;  // Caller retains ownership of `this`, and will destroy it.
    // Run the actual task.
    auto delay = closure_();
    // Convert closure_() return type into uint32_t.
    uint32_t delay_ms = 0;
    if (rtc::IsValueInRangeForNumericType<uint32_t>(delay)) {
      delay_ms = static_cast<uint32_t>(delay);
    } else {
      // Log and recover in production.
      RTC_LOG(LS_ERROR) << "Invalid delay until next run: " << delay;
      delay_ms = rtc::saturated_cast<uint32_t>(delay);
      // But crash in debug.
      RTC_DCHECK(false);
    }
    // Reschedule.
    auto owned_task = absl::WrapUnique(this);
    if (delay_ms == 0)
      TaskQueue::Current()->PostTask(std::move(owned_task));
    else
      TaskQueue::Current()->PostDelayedTask(std::move(owned_task), delay_ms);
    return false;  // Caller will release ownership of `this`.
  }

  Closure closure_;
};
}  // namespace cancelable_periodic_task_internal

template <typename Closure>
std::unique_ptr<BaseCancelableTask> CreateCancelablePeriodicTask(
    Closure&& closure) {
  using CleanedClosure = typename std::remove_cv<
      typename std::remove_reference<Closure>::type>::type;
  return absl::make_unique<cancelable_periodic_task_internal::
                               CancelablePeriodicTask<CleanedClosure>>(
      std::forward<CleanedClosure>(closure));
}

}  // namespace rtc

#endif  // RTC_BASE_CANCELABLE_PERIODIC_TASK_H_
