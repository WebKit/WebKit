/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/simulated_time_controller.h"

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace webrtc {
namespace {
// Helper function to remove from a std container by value.
template <class C>
bool RemoveByValue(C& vec, typename C::value_type val) {
  auto it = std::find(vec.begin(), vec.end(), val);
  if (it == vec.end())
    return false;
  vec.erase(it);
  return true;
}
}  // namespace

namespace sim_time_impl {
class SimulatedSequenceRunner : public ProcessThread, public TaskQueueBase {
 public:
  SimulatedSequenceRunner(SimulatedTimeControllerImpl* handler,
                          absl::string_view queue_name)
      : handler_(handler), name_(queue_name) {}
  ~SimulatedSequenceRunner() override { handler_->Unregister(this); }

  // Provides next run time.
  Timestamp GetNextRunTime() const;

  // Iterates through delayed tasks and modules and moves them to the ready set
  // if they are supposed to execute by |at time|.
  void UpdateReady(Timestamp at_time);
  // Runs all ready tasks and modules and updates next run time.
  void Run(Timestamp at_time);

  // TaskQueueBase interface
  void Delete() override;
  // Note: PostTask is also in ProcessThread interface.
  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

  // ProcessThread interface
  void Start() override;
  void Stop() override;
  void WakeUp(Module* module) override;
  void RegisterModule(Module* module, const rtc::Location& from) override;
  void DeRegisterModule(Module* module) override;
  // Promoted to public for use in SimulatedTimeControllerImpl::YieldExecution.
  using CurrentTaskQueueSetter = TaskQueueBase::CurrentTaskQueueSetter;

 private:
  Timestamp GetCurrentTime() const { return handler_->CurrentTime(); }
  void RunReadyTasks(Timestamp at_time) RTC_LOCKS_EXCLUDED(lock_);
  void RunReadyModules(Timestamp at_time) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UpdateNextRunTime() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  Timestamp GetNextTime(Module* module, Timestamp at_time);

  SimulatedTimeControllerImpl* const handler_;
  const std::string name_;

  rtc::CriticalSection lock_;

  std::deque<std::unique_ptr<QueuedTask>> ready_tasks_ RTC_GUARDED_BY(lock_);
  std::map<Timestamp, std::vector<std::unique_ptr<QueuedTask>>> delayed_tasks_
      RTC_GUARDED_BY(lock_);

  bool process_thread_running_ RTC_GUARDED_BY(lock_) = false;
  std::vector<Module*> stopped_modules_ RTC_GUARDED_BY(lock_);
  std::vector<Module*> ready_modules_ RTC_GUARDED_BY(lock_);
  std::map<Timestamp, std::list<Module*>> delayed_modules_
      RTC_GUARDED_BY(lock_);

  Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();
};

Timestamp SimulatedSequenceRunner::GetNextRunTime() const {
  rtc::CritScope lock(&lock_);
  return next_run_time_;
}

void SimulatedSequenceRunner::UpdateReady(Timestamp at_time) {
  rtc::CritScope lock(&lock_);
  for (auto it = delayed_tasks_.begin();
       it != delayed_tasks_.end() && it->first <= at_time;
       it = delayed_tasks_.erase(it)) {
    for (auto& task : it->second) {
      ready_tasks_.emplace_back(std::move(task));
    }
  }
  for (auto it = delayed_modules_.begin();
       it != delayed_modules_.end() && it->first <= at_time;
       it = delayed_modules_.erase(it)) {
    for (auto module : it->second) {
      ready_modules_.push_back(module);
    }
  }
}

void SimulatedSequenceRunner::Run(Timestamp at_time) {
  RunReadyTasks(at_time);
  rtc::CritScope lock(&lock_);
  RunReadyModules(at_time);
  UpdateNextRunTime();
}

void SimulatedSequenceRunner::Delete() {
  {
    rtc::CritScope lock(&lock_);
    ready_tasks_.clear();
    delayed_tasks_.clear();
  }
  delete this;
}

void SimulatedSequenceRunner::RunReadyTasks(Timestamp at_time) {
  std::deque<std::unique_ptr<QueuedTask>> ready_tasks;
  {
    rtc::CritScope lock(&lock_);
    ready_tasks.swap(ready_tasks_);
  }
  if (!ready_tasks.empty()) {
    CurrentTaskQueueSetter set_current(this);
    for (auto& ready : ready_tasks) {
      bool delete_task = ready->Run();
      if (delete_task) {
        ready.reset();
      } else {
        ready.release();
      }
    }
  }
}

void SimulatedSequenceRunner::RunReadyModules(Timestamp at_time) {
  if (!ready_modules_.empty()) {
    CurrentTaskQueueSetter set_current(this);
    for (auto* module : ready_modules_) {
      module->Process();
      delayed_modules_[GetNextTime(module, at_time)].push_back(module);
    }
  }
  ready_modules_.clear();
}

void SimulatedSequenceRunner::UpdateNextRunTime() {
  if (!ready_tasks_.empty() || !ready_modules_.empty()) {
    next_run_time_ = Timestamp::MinusInfinity();
  } else {
    next_run_time_ = Timestamp::PlusInfinity();
    if (!delayed_tasks_.empty())
      next_run_time_ = std::min(next_run_time_, delayed_tasks_.begin()->first);
    if (!delayed_modules_.empty())
      next_run_time_ =
          std::min(next_run_time_, delayed_modules_.begin()->first);
  }
}

void SimulatedSequenceRunner::PostTask(std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&lock_);
  ready_tasks_.emplace_back(std::move(task));
  next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedSequenceRunner::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                              uint32_t milliseconds) {
  rtc::CritScope lock(&lock_);
  Timestamp target_time = GetCurrentTime() + TimeDelta::ms(milliseconds);
  delayed_tasks_[target_time].push_back(std::move(task));
  next_run_time_ = std::min(next_run_time_, target_time);
}

void SimulatedSequenceRunner::Start() {
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

  Timestamp at_time = GetCurrentTime();
  rtc::CritScope lock(&lock_);
  for (auto& module : starting)
    delayed_modules_[GetNextTime(module, at_time)].push_back(module);
  UpdateNextRunTime();
}

void SimulatedSequenceRunner::Stop() {
  std::vector<Module*> stopping;
  {
    rtc::CritScope lock(&lock_);
    process_thread_running_ = false;

    for (auto* ready : ready_modules_)
      stopped_modules_.push_back(ready);
    ready_modules_.clear();

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

void SimulatedSequenceRunner::WakeUp(Module* module) {
  rtc::CritScope lock(&lock_);
  // If we already are planning to run this module as soon as possible, we don't
  // need to do anything.
  for (auto mod : ready_modules_)
    if (mod == module)
      return;

  for (auto it = delayed_modules_.begin(); it != delayed_modules_.end(); ++it) {
    if (RemoveByValue(it->second, module))
      break;
  }
  Timestamp next_time = GetNextTime(module, GetCurrentTime());
  delayed_modules_[next_time].push_back(module);
  next_run_time_ = std::min(next_run_time_, next_time);
}

void SimulatedSequenceRunner::RegisterModule(Module* module,
                                             const rtc::Location& from) {
  module->ProcessThreadAttached(this);
  rtc::CritScope lock(&lock_);
  if (!process_thread_running_) {
    stopped_modules_.push_back(module);
  } else {
    Timestamp next_time = GetNextTime(module, GetCurrentTime());
    delayed_modules_[next_time].push_back(module);
    next_run_time_ = std::min(next_run_time_, next_time);
  }
}

void SimulatedSequenceRunner::DeRegisterModule(Module* module) {
  bool modules_running;
  {
    rtc::CritScope lock(&lock_);
    if (!process_thread_running_) {
      RemoveByValue(stopped_modules_, module);
    } else {
      bool removed = RemoveByValue(ready_modules_, module);
      if (!removed) {
        for (auto& pair : delayed_modules_) {
          if (RemoveByValue(pair.second, module))
            break;
        }
      }
    }
    modules_running = process_thread_running_;
  }
  if (modules_running)
    module->ProcessThreadAttached(nullptr);
}

Timestamp SimulatedSequenceRunner::GetNextTime(Module* module,
                                               Timestamp at_time) {
  CurrentTaskQueueSetter set_current(this);
  return at_time + TimeDelta::ms(module->TimeUntilNextProcess());
}

SimulatedTimeControllerImpl::SimulatedTimeControllerImpl(Timestamp start_time)
    : thread_id_(rtc::CurrentThreadId()), current_time_(start_time) {}

SimulatedTimeControllerImpl::~SimulatedTimeControllerImpl() = default;

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
SimulatedTimeControllerImpl::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  // TODO(srte): Remove the const cast when the interface is made mutable.
  auto mutable_this = const_cast<SimulatedTimeControllerImpl*>(this);
  auto task_queue = std::unique_ptr<SimulatedSequenceRunner, TaskQueueDeleter>(
      new SimulatedSequenceRunner(mutable_this, name));
  rtc::CritScope lock(&mutable_this->lock_);
  mutable_this->runners_.push_back(task_queue.get());
  return task_queue;
}

std::unique_ptr<ProcessThread> SimulatedTimeControllerImpl::CreateProcessThread(
    const char* thread_name) {
  rtc::CritScope lock(&lock_);
  auto process_thread =
      absl::make_unique<SimulatedSequenceRunner>(this, thread_name);
  runners_.push_back(process_thread.get());
  return process_thread;
}

void SimulatedTimeControllerImpl::YieldExecution() {
  if (rtc::CurrentThreadId() == thread_id_) {
    TaskQueueBase* yielding_from = TaskQueueBase::Current();
    // Since we might continue execution on a process thread, we should reset
    // the thread local task queue reference. This ensures that thread checkers
    // won't think we are executing on the yielding task queue. It also ensure
    // that TaskQueueBase::Current() won't return the yielding task queue.
    SimulatedSequenceRunner::CurrentTaskQueueSetter reset_queue(nullptr);
    RTC_DCHECK_RUN_ON(&thread_checker_);
    // When we yield, we don't want to risk executing further tasks on the
    // currently executing task queue. If there's a ready task that also yields,
    // it's added to this set as well and only tasks on the remaining task
    // queues are executed.
    auto inserted = yielded_.insert(yielding_from);
    RTC_DCHECK(inserted.second);
    RunReadyRunners();
    yielded_.erase(inserted.first);
  }
}

void SimulatedTimeControllerImpl::RunReadyRunners() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  rtc::CritScope lock(&lock_);
  RTC_DCHECK_EQ(rtc::CurrentThreadId(), thread_id_);
  Timestamp current_time = CurrentTime();
  // Clearing |ready_runners_| in case this is a recursive call:
  // RunReadyRunners -> Run -> Event::Wait -> Yield ->RunReadyRunners
  ready_runners_.clear();

  // We repeat until we have no ready left to handle tasks posted by ready
  // runners.
  while (true) {
    for (auto* runner : runners_) {
      if (yielded_.find(runner) == yielded_.end() &&
          runner->GetNextRunTime() <= current_time) {
        ready_runners_.push_back(runner);
      }
    }
    if (ready_runners_.empty())
      return;
    while (!ready_runners_.empty()) {
      auto* runner = ready_runners_.front();
      ready_runners_.pop_front();
      runner->UpdateReady(current_time);
      // Note that the Run function might indirectly cause a call to
      // Unregister() which will recursively grab |lock_| again to remove items
      // from |ready_runners_|.
      runner->Run(current_time);
    }
  }
}

Timestamp SimulatedTimeControllerImpl::CurrentTime() const {
  rtc::CritScope lock(&time_lock_);
  return current_time_;
}

Timestamp SimulatedTimeControllerImpl::NextRunTime() const {
  Timestamp current_time = CurrentTime();
  Timestamp next_time = Timestamp::PlusInfinity();
  rtc::CritScope lock(&lock_);
  for (auto* runner : runners_) {
    Timestamp next_run_time = runner->GetNextRunTime();
    if (next_run_time <= current_time)
      return current_time;
    next_time = std::min(next_time, next_run_time);
  }
  return next_time;
}

void SimulatedTimeControllerImpl::AdvanceTime(Timestamp target_time) {
  rtc::CritScope time_lock(&time_lock_);
  RTC_DCHECK_GE(target_time, current_time_);
  current_time_ = target_time;
}

void SimulatedTimeControllerImpl::Unregister(SimulatedSequenceRunner* runner) {
  rtc::CritScope lock(&lock_);
  bool removed = RemoveByValue(runners_, runner);
  RTC_CHECK(removed);
  RemoveByValue(ready_runners_, runner);
}

}  // namespace sim_time_impl

GlobalSimulatedTimeController::GlobalSimulatedTimeController(
    Timestamp start_time)
    : sim_clock_(start_time.us()), impl_(start_time) {
  global_clock_.SetTime(start_time);
}

GlobalSimulatedTimeController::~GlobalSimulatedTimeController() = default;

Clock* GlobalSimulatedTimeController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* GlobalSimulatedTimeController::GetTaskQueueFactory() {
  return &impl_;
}

std::unique_ptr<ProcessThread>
GlobalSimulatedTimeController::CreateProcessThread(const char* thread_name) {
  return impl_.CreateProcessThread(thread_name);
}

void GlobalSimulatedTimeController::Sleep(TimeDelta duration) {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  Timestamp current_time = impl_.CurrentTime();
  Timestamp target_time = current_time + duration;
  RTC_DCHECK_EQ(current_time.us(), rtc::TimeMicros());
  while (current_time < target_time) {
    impl_.RunReadyRunners();
    Timestamp next_time = std::min(impl_.NextRunTime(), target_time);
    impl_.AdvanceTime(next_time);
    auto delta = next_time - current_time;
    current_time = next_time;
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
    global_clock_.AdvanceTime(delta);
  }
}

void GlobalSimulatedTimeController::InvokeWithControlledYield(
    std::function<void()> closure) {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  closure();
}

// namespace sim_time_impl

}  // namespace webrtc
