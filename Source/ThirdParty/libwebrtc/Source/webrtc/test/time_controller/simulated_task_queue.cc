/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/simulated_task_queue.h"

#include <algorithm>
#include <utility>

namespace webrtc {

SimulatedTaskQueue::SimulatedTaskQueue(
    sim_time_impl::SimulatedTimeControllerImpl* handler,
    absl::string_view name)
    : handler_(handler), name_(new char[name.size()]) {
  std::copy_n(name.begin(), name.size(), name_);
}

SimulatedTaskQueue::~SimulatedTaskQueue() {
  handler_->Unregister(this);
  delete[] name_;
}

void SimulatedTaskQueue::Delete() {
  {
    rtc::CritScope lock(&lock_);
    ready_tasks_.clear();
    delayed_tasks_.clear();
  }
  delete this;
}

void SimulatedTaskQueue::RunReady(Timestamp at_time) {
  rtc::CritScope lock(&lock_);
  for (auto it = delayed_tasks_.begin();
       it != delayed_tasks_.end() && it->first <= at_time;
       it = delayed_tasks_.erase(it)) {
    for (auto& task : it->second) {
      ready_tasks_.emplace_back(std::move(task));
    }
  }
  CurrentTaskQueueSetter set_current(this);
  while (!ready_tasks_.empty()) {
    std::unique_ptr<QueuedTask> ready = std::move(ready_tasks_.front());
    ready_tasks_.pop_front();
    lock_.Leave();
    bool delete_task = ready->Run();
    if (delete_task) {
      ready.reset();
    } else {
      ready.release();
    }
    lock_.Enter();
  }
  if (!delayed_tasks_.empty()) {
    next_run_time_ = delayed_tasks_.begin()->first;
  } else {
    next_run_time_ = Timestamp::PlusInfinity();
  }
}

void SimulatedTaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&lock_);
  ready_tasks_.emplace_back(std::move(task));
  next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedTaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                         uint32_t milliseconds) {
  rtc::CritScope lock(&lock_);
  Timestamp target_time =
      handler_->CurrentTime() + TimeDelta::Millis(milliseconds);
  delayed_tasks_[target_time].push_back(std::move(task));
  next_run_time_ = std::min(next_run_time_, target_time);
}

}  // namespace webrtc
