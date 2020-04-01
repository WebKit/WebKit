/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/external_time_controller.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

// Wraps a ProcessThread so that it can reschedule the time controller whenever
// an external call changes the ProcessThread's state.  For example, when a new
// module is registered, the ProcessThread may need to be called sooner than the
// time controller's currently-scheduled deadline.
class ExternalTimeController::ProcessThreadWrapper : public ProcessThread {
 public:
  ProcessThreadWrapper(ExternalTimeController* parent,
                       std::unique_ptr<ProcessThread> thread)
      : parent_(parent), thread_(std::move(thread)) {}

  void Start() override {
    parent_->UpdateTime();
    thread_->Start();
    parent_->ScheduleNext();
  }

  void Stop() override {
    parent_->UpdateTime();
    thread_->Stop();
    parent_->ScheduleNext();
  }

  void WakeUp(Module* module) override {
    parent_->UpdateTime();
    thread_->WakeUp(GetWrapper(module));
    parent_->ScheduleNext();
  }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    parent_->UpdateTime();
    thread_->PostTask(std::move(task));
    parent_->ScheduleNext();
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override {
    parent_->UpdateTime();
    thread_->PostDelayedTask(std::move(task), milliseconds);
    parent_->ScheduleNext();
  }

  void RegisterModule(Module* module, const rtc::Location& from) override {
    parent_->UpdateTime();
    module_wrappers_.emplace(module, new ModuleWrapper(module, this));
    thread_->RegisterModule(GetWrapper(module), from);
    parent_->ScheduleNext();
  }

  void DeRegisterModule(Module* module) override {
    parent_->UpdateTime();
    thread_->DeRegisterModule(GetWrapper(module));
    parent_->ScheduleNext();
    module_wrappers_.erase(module);
  }

 private:
  class ModuleWrapper : public Module {
   public:
    ModuleWrapper(Module* module, ProcessThreadWrapper* thread)
        : module_(module), thread_(thread) {}

    int64_t TimeUntilNextProcess() override {
      return module_->TimeUntilNextProcess();
    }

    void Process() override { module_->Process(); }

    void ProcessThreadAttached(ProcessThread* process_thread) override {
      if (process_thread) {
        module_->ProcessThreadAttached(thread_);
      } else {
        module_->ProcessThreadAttached(nullptr);
      }
    }

   private:
    Module* module_;
    ProcessThreadWrapper* thread_;
  };

  void Delete() override {
    // ProcessThread shouldn't be deleted as a TaskQueue.
    RTC_NOTREACHED();
  }

  ModuleWrapper* GetWrapper(Module* module) {
    auto it = module_wrappers_.find(module);
    RTC_DCHECK(it != module_wrappers_.end());
    return it->second.get();
  }

  ExternalTimeController* const parent_;
  std::unique_ptr<ProcessThread> thread_;
  std::map<Module*, std::unique_ptr<ModuleWrapper>> module_wrappers_;
};

// Wraps a TaskQueue so that it can reschedule the time controller whenever
// an external call schedules a new task.
class ExternalTimeController::TaskQueueWrapper : public TaskQueueBase {
 public:
  TaskQueueWrapper(ExternalTimeController* parent,
                   std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base)
      : parent_(parent), base_(std::move(base)) {}

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    parent_->UpdateTime();
    base_->PostTask(std::make_unique<TaskWrapper>(std::move(task), this));
    parent_->ScheduleNext();
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t ms) override {
    parent_->UpdateTime();
    base_->PostDelayedTask(std::make_unique<TaskWrapper>(std::move(task), this),
                           ms);
    parent_->ScheduleNext();
  }

  void Delete() override { delete this; }

 private:
  class TaskWrapper : public QueuedTask {
   public:
    TaskWrapper(std::unique_ptr<QueuedTask> task, TaskQueueWrapper* queue)
        : task_(std::move(task)), queue_(queue) {}

    bool Run() override {
      CurrentTaskQueueSetter current(queue_);
      if (!task_->Run()) {
        task_.release();
      }
      // The wrapper should always be deleted, even if it releases the inner
      // task, in order to avoid leaking wrappers.
      return true;
    }

   private:
    std::unique_ptr<QueuedTask> task_;
    TaskQueueWrapper* queue_;
  };

  ExternalTimeController* const parent_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_;
};

ExternalTimeController::ExternalTimeController(ControlledAlarmClock* alarm)
    : alarm_(alarm),
      impl_(alarm_->GetClock()->CurrentTime()),
      yield_policy_(&impl_) {
  global_clock_.SetTime(alarm_->GetClock()->CurrentTime());
  alarm_->SetCallback([this] { Run(); });
}

Clock* ExternalTimeController::GetClock() {
  return alarm_->GetClock();
}

TaskQueueFactory* ExternalTimeController::GetTaskQueueFactory() {
  return this;
}

std::unique_ptr<ProcessThread> ExternalTimeController::CreateProcessThread(
    const char* thread_name) {
  return std::make_unique<ProcessThreadWrapper>(
      this, impl_.CreateProcessThread(thread_name));
}

void ExternalTimeController::AdvanceTime(TimeDelta duration) {
  alarm_->Sleep(duration);
}

std::unique_ptr<rtc::Thread> ExternalTimeController::CreateThread(
    const std::string& name,
    std::unique_ptr<rtc::SocketServer> socket_server) {
  RTC_NOTREACHED();
  return nullptr;
}

rtc::Thread* ExternalTimeController::GetMainThread() {
  RTC_NOTREACHED();
  return nullptr;
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
ExternalTimeController::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new TaskQueueWrapper(const_cast<ExternalTimeController*>(this),
                           impl_.CreateTaskQueue(name, priority)));
}

void ExternalTimeController::Run() {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  UpdateTime();
  impl_.RunReadyRunners();
  ScheduleNext();
}

void ExternalTimeController::UpdateTime() {
  Timestamp now = alarm_->GetClock()->CurrentTime();
  impl_.AdvanceTime(now);
  global_clock_.SetTime(now);
}

void ExternalTimeController::ScheduleNext() {
  RTC_DCHECK_EQ(impl_.CurrentTime(), alarm_->GetClock()->CurrentTime());
  TimeDelta delay =
      std::max(impl_.NextRunTime() - impl_.CurrentTime(), TimeDelta::Zero());
  if (delay.IsFinite()) {
    alarm_->ScheduleAlarmAt(alarm_->GetClock()->CurrentTime() + delay);
  }
}

}  // namespace webrtc
