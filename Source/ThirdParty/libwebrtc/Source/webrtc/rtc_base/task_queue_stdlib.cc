/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue.h"

#include <string.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <queue>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/timeutils.h"

namespace rtc {
namespace {

using Priority = TaskQueue::Priority;

ThreadPriority TaskQueuePriorityToThreadPriority(Priority priority) {
  switch (priority) {
    case Priority::HIGH:
      return kRealtimePriority;
    case Priority::LOW:
      return kLowPriority;
    case Priority::NORMAL:
      return kNormalPriority;
    default:
      RTC_NOTREACHED();
      return kNormalPriority;
  }
  return kNormalPriority;
}

}  // namespace

class TaskQueue::Impl : public RefCountInterface {
 public:
  Impl(const char* queue_name, TaskQueue* queue, Priority priority);
  ~Impl() override;

  static TaskQueue::Impl* Current();
  static TaskQueue* CurrentQueue();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostTask(Closure&& closure) {
    PostTask(NewClosure(std::forward<Closure>(closure)));
  }

  void PostTask(std::unique_ptr<QueuedTask> task);
  void PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                        std::unique_ptr<QueuedTask> reply,
                        TaskQueue::Impl* reply_queue);

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

  class WorkerThread : public PlatformThread {
   public:
    WorkerThread(ThreadRunFunction func,
                 void* obj,
                 const char* thread_name,
                 ThreadPriority priority)
        : PlatformThread(func, obj, thread_name, priority) {}
  };

  using OrderId = uint64_t;

  struct DelayedEntryTimeout {
    int64_t next_fire_at_ms_{};
    OrderId order_{};

    bool operator<(const DelayedEntryTimeout& o) const {
      return std::tie(next_fire_at_ms_, order_) <
             std::tie(o.next_fire_at_ms_, o.order_);
    }
  };

  struct NextTask {
    bool final_task_{false};
    std::unique_ptr<QueuedTask> run_task_;
    int64_t sleep_time_ms_{};
  };

 protected:
  NextTask GetNextTask();

 private:
  // The ThreadQueue::Current() method requires that the current thread
  // returns the task queue if the current thread is the active task
  // queue and this variable holds the value needed in thread_local to
  // on the initialized worker thread holding the queue.
  static thread_local TaskQueue::Impl* thread_context_;

  static void ThreadMain(void* context);

  void ProcessTasks();

  void NotifyWake();

  // The back pointer from the owner task queue object
  // from this implementation detail.
  TaskQueue* const queue_;

  // Indicates if the thread has started.
  Event started_;

  // Indicates if the thread has stopped.
  Event stopped_;

  // Signaled whenever a new task is pending.
  Event flag_notify_;

  // Contains the active worker thread assigned to processing
  // tasks (including delayed tasks).
  WorkerThread thread_;

  rtc::CriticalSection pending_lock_;

  // Indicates if the worker thread needs to shutdown now.
  bool thread_should_quit_ RTC_GUARDED_BY(pending_lock_){false};

  // Holds the next order to use for the next task to be
  // put into one of the pending queues.
  OrderId thread_posting_order_ RTC_GUARDED_BY(pending_lock_){};

  // The list of all pending tasks that need to be processed in the
  // FIFO queue ordering on the worker thread.
  std::queue<std::pair<OrderId, std::unique_ptr<QueuedTask>>> pending_queue_
      RTC_GUARDED_BY(pending_lock_);

  // The list of all pending tasks that need to be processed at a future
  // time based upon a delay. On the off change the delayed task should
  // happen at exactly the same time interval as another task then the
  // task is processed based on FIFO ordering. std::priority_queue was
  // considered but rejected due to its inability to extract the
  // std::unique_ptr out of the queue without the presence of a hack.
  std::map<DelayedEntryTimeout, std::unique_ptr<QueuedTask>> delayed_queue_
      RTC_GUARDED_BY(pending_lock_);
};

// static
thread_local TaskQueue::Impl* TaskQueue::Impl::thread_context_ = nullptr;

TaskQueue::Impl::Impl(const char* queue_name,
                      TaskQueue* queue,
                      Priority priority)
    : queue_(queue),
      started_(/*manual_reset=*/false, /*initially_signaled=*/false),
      stopped_(/*manual_reset=*/false, /*initially_signaled=*/false),
      flag_notify_(/*manual_reset=*/false, /*initially_signaled=*/false),
      thread_(&TaskQueue::Impl::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  thread_.Start();
  started_.Wait(Event::kForever);
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());

  {
    CritScope lock(&pending_lock_);
    thread_should_quit_ = true;
  }

  NotifyWake();

  stopped_.Wait(Event::kForever);
  thread_.Stop();
}

// static
TaskQueue::Impl* TaskQueue::Impl::Current() {
  return thread_context_;
}

// static
TaskQueue* TaskQueue::Impl::CurrentQueue() {
  TaskQueue::Impl* current = Current();
  return current ? current->queue_ : nullptr;
}

bool TaskQueue::Impl::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::Impl::PostTask(std::unique_ptr<QueuedTask> task) {
  {
    CritScope lock(&pending_lock_);
    OrderId order = thread_posting_order_++;

    pending_queue_.push(std::pair<OrderId, std::unique_ptr<QueuedTask>>(
        order, std::move(task)));
  }

  NotifyWake();
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  auto fire_at = rtc::TimeMillis() + milliseconds;

  DelayedEntryTimeout delay;
  delay.next_fire_at_ms_ = fire_at;

  {
    CritScope lock(&pending_lock_);
    delay.order_ = ++thread_posting_order_;
    delayed_queue_[delay] = std::move(task);
  }

  NotifyWake();
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();
  PostTask([task_ptr, reply_task_ptr, reply_queue]() {
    if (task_ptr->Run())
      delete task_ptr;

    reply_queue->PostTask(std::unique_ptr<QueuedTask>(reply_task_ptr));
  });
}

TaskQueue::Impl::NextTask TaskQueue::Impl::GetNextTask() {
  NextTask result{};

  auto tick = rtc::TimeMillis();

  CritScope lock(&pending_lock_);

  if (thread_should_quit_) {
    result.final_task_ = true;
    return result;
  }

  if (delayed_queue_.size() > 0) {
    auto delayed_entry = delayed_queue_.begin();
    const auto& delay_info = delayed_entry->first;
    auto& delay_run = delayed_entry->second;
    if (tick >= delay_info.next_fire_at_ms_) {
      if (pending_queue_.size() > 0) {
        auto& entry = pending_queue_.front();
        auto& entry_order = entry.first;
        auto& entry_run = entry.second;
        if (entry_order < delay_info.order_) {
          result.run_task_ = std::move(entry_run);
          pending_queue_.pop();
          return result;
        }
      }

      result.run_task_ = std::move(delay_run);
      delayed_queue_.erase(delayed_entry);
      return result;
    }

    result.sleep_time_ms_ = delay_info.next_fire_at_ms_ - tick;
  }

  if (pending_queue_.size() > 0) {
    auto& entry = pending_queue_.front();
    result.run_task_ = std::move(entry.second);
    pending_queue_.pop();
  }

  return result;
}

// static
void TaskQueue::Impl::ThreadMain(void* context) {
  TaskQueue::Impl* me = static_cast<TaskQueue::Impl*>(context);
  me->ProcessTasks();
}

void TaskQueue::Impl::ProcessTasks() {
  thread_context_ = this;
  started_.Set();

  while (true) {
    auto task = GetNextTask();

    if (task.final_task_)
      break;

    if (task.run_task_) {
      // process entry immediately then try again
      QueuedTask* release_ptr = task.run_task_.release();
      if (release_ptr->Run())
        delete release_ptr;

      // attempt to sleep again
      continue;
    }

    if (0 == task.sleep_time_ms_)
      flag_notify_.Wait(Event::kForever);
    else
      flag_notify_.Wait(task.sleep_time_ms_);
  }

  stopped_.Set();
}

void TaskQueue::Impl::NotifyWake() {
  // The queue holds pending tasks to complete. Either tasks are to be
  // executed immediately or tasks are to be run at some future delayed time.
  // For immediate tasks the task queue's thread is busy running the task and
  // the thread will not be waiting on the flag_notify_ event. If no immediate
  // tasks are available but a delayed task is pending then the thread will be
  // waiting on flag_notify_ with a delayed time-out of the nearest timed task
  // to run. If no immediate or pending tasks are available, the thread will
  // wait on flag_notify_ until signaled that a task has been added (or the
  // thread to be told to shutdown).

  // In all cases, when a new immediate task, delayed task, or request to
  // shutdown the thread is added the flag_notify_ is signaled after. If the
  // thread was waiting then the thread will wake up immediately and re-assess
  // what task needs to be run next (i.e. run a task now, wait for the nearest
  // timed delayed task, or shutdown the thread). If the thread was not waiting
  // then the thread will remained signaled to wake up the next time any
  // attempt to wait on the flag_notify_ event occurs.

  // Any immediate or delayed pending task (or request to shutdown the thread)
  // must always be added to the queue prior to signaling flag_notify_ to wake
  // up the possibly sleeping thread. This prevents a race condition where the
  // thread is notified to wake up but the task queue's thread finds nothing to
  // do so it waits once again to be signaled where such a signal may never
  // happen.
  flag_notify_.Set();
}

// Boilerplate for the PIMPL pattern.
TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(new RefCountedObject<TaskQueue::Impl>(queue_name, this, priority)) {
}

TaskQueue::~TaskQueue() {}

// static
TaskQueue* TaskQueue::Current() {
  return TaskQueue::Impl::CurrentQueue();
}

// Used for DCHECKing the current queue.
bool TaskQueue::IsCurrent() const {
  return impl_->IsCurrent();
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return TaskQueue::impl_->PostTask(std::move(task));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            reply_queue->impl_.get());
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            impl_.get());
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return TaskQueue::impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
