/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/simulated_process_thread.h"

#include <algorithm>
#include <utility>

namespace webrtc {
namespace {
// Helper function to remove from a std container by value.
template <class C>
bool RemoveByValue(C* vec, typename C::value_type val) {
  auto it = std::find(vec->begin(), vec->end(), val);
  if (it == vec->end())
    return false;
  vec->erase(it);
  return true;
}
}  // namespace
SimulatedProcessThread::SimulatedProcessThread(
    sim_time_impl::SimulatedTimeControllerImpl* handler,
    absl::string_view name)
    : handler_(handler), name_(new char[name.size()]) {
  std::copy_n(name.begin(), name.size(), name_);
}

SimulatedProcessThread::~SimulatedProcessThread() {
  handler_->Unregister(this);
  delete[] name_;
}

void SimulatedProcessThread::RunReady(Timestamp at_time) {
  CurrentTaskQueueSetter set_current(this);
  rtc::CritScope lock(&lock_);
  std::vector<Module*> ready_modules;
  for (auto it = delayed_modules_.begin();
       it != delayed_modules_.end() && it->first <= at_time;
       it = delayed_modules_.erase(it)) {
    for (auto module : it->second) {
      ready_modules.push_back(module);
    }
  }
  for (auto* module : ready_modules) {
    module->Process();
    delayed_modules_[GetNextTime(module, at_time)].push_back(module);
  }

  for (auto it = delayed_tasks_.begin();
       it != delayed_tasks_.end() && it->first <= at_time;
       it = delayed_tasks_.erase(it)) {
    for (auto& task : it->second) {
      queue_.push_back(std::move(task));
    }
  }
  while (!queue_.empty()) {
    std::unique_ptr<QueuedTask> task = std::move(queue_.front());
    queue_.pop_front();
    lock_.Leave();
    bool should_delete = task->Run();
    RTC_CHECK(should_delete);
    lock_.Enter();
  }
  RTC_DCHECK(queue_.empty());
  if (!delayed_modules_.empty()) {
    next_run_time_ = delayed_modules_.begin()->first;
  } else {
    next_run_time_ = Timestamp::PlusInfinity();
  }
  if (!delayed_tasks_.empty()) {
    next_run_time_ = std::min(next_run_time_, delayed_tasks_.begin()->first);
  }
}
void SimulatedProcessThread::Start() {
  std::vector<Module*> starting;
  {
    rtc::CritScope lock(&lock_);
    if (process_thread_running_)
      return;
    process_thread_running_ = true;
    starting.swap(stopped_modules_);
  }
  for (auto& module : starting)
    module->ProcessThreadAttached(this);

  Timestamp at_time = handler_->CurrentTime();
  rtc::CritScope lock(&lock_);
  for (auto& module : starting)
    delayed_modules_[GetNextTime(module, at_time)].push_back(module);

  if (!queue_.empty()) {
    next_run_time_ = Timestamp::MinusInfinity();
  } else if (!delayed_modules_.empty()) {
    next_run_time_ = delayed_modules_.begin()->first;
  } else {
    next_run_time_ = Timestamp::PlusInfinity();
  }
}

void SimulatedProcessThread::Stop() {
  std::vector<Module*> stopping;
  {
    rtc::CritScope lock(&lock_);
    process_thread_running_ = false;

    for (auto& delayed : delayed_modules_) {
      for (auto mod : delayed.second)
        stopped_modules_.push_back(mod);
    }
    delayed_modules_.clear();

    stopping = stopped_modules_;
  }
  for (auto& module : stopping)
    module->ProcessThreadAttached(nullptr);
}

void SimulatedProcessThread::WakeUp(Module* module) {
  rtc::CritScope lock(&lock_);
  for (auto it = delayed_modules_.begin(); it != delayed_modules_.end(); ++it) {
    if (RemoveByValue(&it->second, module))
      break;
  }
  Timestamp next_time = GetNextTime(module, handler_->CurrentTime());
  delayed_modules_[next_time].push_back(module);
  next_run_time_ = std::min(next_run_time_, next_time);
}

void SimulatedProcessThread::RegisterModule(Module* module,
                                            const rtc::Location& from) {
  module->ProcessThreadAttached(this);
  rtc::CritScope lock(&lock_);
  if (!process_thread_running_) {
    stopped_modules_.push_back(module);
  } else {
    Timestamp next_time = GetNextTime(module, handler_->CurrentTime());
    delayed_modules_[next_time].push_back(module);
    next_run_time_ = std::min(next_run_time_, next_time);
  }
}

void SimulatedProcessThread::DeRegisterModule(Module* module) {
  bool modules_running;
  {
    rtc::CritScope lock(&lock_);
    if (!process_thread_running_) {
      RemoveByValue(&stopped_modules_, module);
    } else {
      for (auto& pair : delayed_modules_) {
        if (RemoveByValue(&pair.second, module))
          break;
      }
    }
    modules_running = process_thread_running_;
  }
  if (modules_running)
    module->ProcessThreadAttached(nullptr);
}

void SimulatedProcessThread::PostTask(std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&lock_);
  queue_.emplace_back(std::move(task));
  next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedProcessThread::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                             uint32_t milliseconds) {
  rtc::CritScope lock(&lock_);
  Timestamp target_time =
      handler_->CurrentTime() + TimeDelta::Millis(milliseconds);
  delayed_tasks_[target_time].push_back(std::move(task));
  next_run_time_ = std::min(next_run_time_, target_time);
}

Timestamp SimulatedProcessThread::GetNextTime(Module* module,
                                              Timestamp at_time) {
  CurrentTaskQueueSetter set_current(this);
  return at_time + TimeDelta::Millis(module->TimeUntilNextProcess());
}

}  // namespace webrtc
