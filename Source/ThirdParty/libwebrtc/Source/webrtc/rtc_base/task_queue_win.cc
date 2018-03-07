/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue.h"

#include <mmsystem.h>
#include <string.h>

#include <algorithm>
#include <queue>
#include <utility>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/timeutils.h"

namespace rtc {
namespace {
#define WM_RUN_TASK WM_USER + 1
#define WM_QUEUE_DELAYED_TASK WM_USER + 2

using Priority = TaskQueue::Priority;

DWORD g_queue_ptr_tls = 0;

BOOL CALLBACK InitializeTls(PINIT_ONCE init_once, void* param, void** context) {
  g_queue_ptr_tls = TlsAlloc();
  return TRUE;
}

DWORD GetQueuePtrTls() {
  static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
  ::InitOnceExecuteOnce(&init_once, InitializeTls, nullptr, nullptr);
  return g_queue_ptr_tls;
}

struct ThreadStartupData {
  Event* started;
  void* thread_context;
};

void CALLBACK InitializeQueueThread(ULONG_PTR param) {
  MSG msg;
  ::PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  ThreadStartupData* data = reinterpret_cast<ThreadStartupData*>(param);
  ::TlsSetValue(GetQueuePtrTls(), data->thread_context);
  data->started->Set();
}

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
      break;
  }
  return kNormalPriority;
}

int64_t GetTick() {
  static const UINT kPeriod = 1;
  bool high_res = (timeBeginPeriod(kPeriod) == TIMERR_NOERROR);
  int64_t ret = TimeMillis();
  if (high_res)
    timeEndPeriod(kPeriod);
  return ret;
}

class DelayedTaskInfo {
 public:
  // Default ctor needed to support priority_queue::pop().
  DelayedTaskInfo() {}
  DelayedTaskInfo(uint32_t milliseconds, std::unique_ptr<QueuedTask> task)
      : due_time_(GetTick() + milliseconds), task_(std::move(task)) {}
  DelayedTaskInfo(DelayedTaskInfo&&) = default;

  // Implement for priority_queue.
  bool operator>(const DelayedTaskInfo& other) const {
    return due_time_ > other.due_time_;
  }

  // Required by priority_queue::pop().
  DelayedTaskInfo& operator=(DelayedTaskInfo&& other) = default;

  // See below for why this method is const.
  void Run() const {
    RTC_DCHECK(due_time_);
    task_->Run() ? task_.reset() : static_cast<void>(task_.release());
  }

  int64_t due_time() const { return due_time_; }

 private:
  int64_t due_time_ = 0;  // Absolute timestamp in milliseconds.

  // |task| needs to be mutable because std::priority_queue::top() returns
  // a const reference and a key in an ordered queue must not be changed.
  // There are two basic workarounds, one using const_cast, which would also
  // make the key (|due_time|), non-const and the other is to make the non-key
  // (|task|), mutable.
  // Because of this, the |task| variable is made private and can only be
  // mutated by calling the |Run()| method.
  mutable std::unique_ptr<QueuedTask> task_;
};

class MultimediaTimer {
 public:
  // Note: We create an event that requires manual reset.
  MultimediaTimer() : event_(::CreateEvent(nullptr, true, false, nullptr)) {}

  ~MultimediaTimer() {
    Cancel();
    ::CloseHandle(event_);
  }

  bool StartOneShotTimer(UINT delay_ms) {
    RTC_DCHECK_EQ(0, timer_id_);
    RTC_DCHECK(event_ != nullptr);
    timer_id_ =
        ::timeSetEvent(delay_ms, 0, reinterpret_cast<LPTIMECALLBACK>(event_), 0,
                       TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
    return timer_id_ != 0;
  }

  void Cancel() {
    ::ResetEvent(event_);
    if (timer_id_) {
      ::timeKillEvent(timer_id_);
      timer_id_ = 0;
    }
  }

  HANDLE* event_for_wait() { return &event_; }

 private:
  HANDLE event_ = nullptr;
  MMRESULT timer_id_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(MultimediaTimer);
};

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

  void RunPendingTasks();

 private:
  static void ThreadMain(void* context);

  class WorkerThread : public PlatformThread {
   public:
    WorkerThread(ThreadRunFunction func,
                 void* obj,
                 const char* thread_name,
                 ThreadPriority priority)
        : PlatformThread(func, obj, thread_name, priority) {}

    bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data) {
      return PlatformThread::QueueAPC(apc_function, data);
    }
  };

  class ThreadState {
   public:
    explicit ThreadState(HANDLE in_queue) : in_queue_(in_queue) {}
    ~ThreadState() {}

    void RunThreadMain();

   private:
    bool ProcessQueuedMessages();
    void RunDueTasks();
    void ScheduleNextTimer();
    void CancelTimers();

    // Since priority_queue<> by defult orders items in terms of
    // largest->smallest, using std::less<>, and we want smallest->largest,
    // we would like to use std::greater<> here. Alas it's only available in
    // C++14 and later, so we roll our own compare template that that relies on
    // operator<().
    template <typename T>
    struct greater {
      bool operator()(const T& l, const T& r) { return l > r; }
    };

    MultimediaTimer timer_;
    std::priority_queue<DelayedTaskInfo,
                        std::vector<DelayedTaskInfo>,
                        greater<DelayedTaskInfo>>
        timer_tasks_;
    UINT_PTR timer_id_ = 0;
    HANDLE in_queue_;
  };

  TaskQueue* const queue_;
  WorkerThread thread_;
  rtc::CriticalSection pending_lock_;
  std::queue<std::unique_ptr<QueuedTask>> pending_
      RTC_GUARDED_BY(pending_lock_);
  HANDLE in_queue_;
};

TaskQueue::Impl::Impl(const char* queue_name,
                      TaskQueue* queue,
                      Priority priority)
    : queue_(queue),
      thread_(&TaskQueue::Impl::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)),
      in_queue_(::CreateEvent(nullptr, true, false, nullptr)) {
  RTC_DCHECK(queue_name);
  RTC_DCHECK(in_queue_);
  thread_.Start();
  Event event(false, false);
  ThreadStartupData startup = {&event, this};
  RTC_CHECK(thread_.QueueAPC(&InitializeQueueThread,
                             reinterpret_cast<ULONG_PTR>(&startup)));
  event.Wait(Event::kForever);
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());
  while (!::PostThreadMessage(thread_.GetThreadRef(), WM_QUIT, 0, 0)) {
    RTC_CHECK_EQ(ERROR_NOT_ENOUGH_QUOTA, ::GetLastError());
    Sleep(1);
  }
  thread_.Stop();
  ::CloseHandle(in_queue_);
}

// static
TaskQueue::Impl* TaskQueue::Impl::Current() {
  return static_cast<TaskQueue::Impl*>(::TlsGetValue(GetQueuePtrTls()));
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
  rtc::CritScope lock(&pending_lock_);
  pending_.push(std::move(task));
  ::SetEvent(in_queue_);
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  if (!milliseconds) {
    PostTask(std::move(task));
    return;
  }

  // TODO(tommi): Avoid this allocation.  It is currently here since
  // the timestamp stored in the task info object, is a 64bit timestamp
  // and WPARAM is 32bits in 32bit builds.  Otherwise, we could pass the
  // task pointer and timestamp as LPARAM and WPARAM.
  auto* task_info = new DelayedTaskInfo(milliseconds, std::move(task));
  if (!::PostThreadMessage(thread_.GetThreadRef(), WM_QUEUE_DELAYED_TASK, 0,
                           reinterpret_cast<LPARAM>(task_info))) {
    delete task_info;
  }
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();
  DWORD reply_thread_id = reply_queue->thread_.GetThreadRef();
  PostTask([task_ptr, reply_task_ptr, reply_thread_id]() {
    if (task_ptr->Run())
      delete task_ptr;
    // If the thread's message queue is full, we can't queue the task and will
    // have to drop it (i.e. delete).
    if (!::PostThreadMessage(reply_thread_id, WM_RUN_TASK, 0,
                             reinterpret_cast<LPARAM>(reply_task_ptr))) {
      delete reply_task_ptr;
    }
  });
}

void TaskQueue::Impl::RunPendingTasks() {
  while (true) {
    std::unique_ptr<QueuedTask> task;
    {
      rtc::CritScope lock(&pending_lock_);
      if (pending_.empty())
        break;
      task = std::move(pending_.front());
      pending_.pop();
    }

    if (!task->Run())
      task.release();
  }
}

// static
void TaskQueue::Impl::ThreadMain(void* context) {
  ThreadState state(static_cast<TaskQueue::Impl*>(context)->in_queue_);
  state.RunThreadMain();
}

void TaskQueue::Impl::ThreadState::RunThreadMain() {
  HANDLE handles[2] = { *timer_.event_for_wait(), in_queue_ };
  while (true) {
    // Make sure we do an alertable wait as that's required to allow APCs to run
    // (e.g. required for InitializeQueueThread and stopping the thread in
    // PlatformThread).
    DWORD result = ::MsgWaitForMultipleObjectsEx(
        arraysize(handles), handles, INFINITE, QS_ALLEVENTS, MWMO_ALERTABLE);
    RTC_CHECK_NE(WAIT_FAILED, result);
    if (result == (WAIT_OBJECT_0 + 2)) {
      // There are messages in the message queue that need to be handled.
      if (!ProcessQueuedMessages())
        break;
    }

    if (result == WAIT_OBJECT_0 || (!timer_tasks_.empty() &&
        ::WaitForSingleObject(*timer_.event_for_wait(), 0) == WAIT_OBJECT_0)) {
      // The multimedia timer was signaled.
      timer_.Cancel();
      RunDueTasks();
      ScheduleNextTimer();
    }

    if (result == (WAIT_OBJECT_0 + 1)) {
      ::ResetEvent(in_queue_);
      TaskQueue::Impl::Current()->RunPendingTasks();
    }
  }
}

bool TaskQueue::Impl::ThreadState::ProcessQueuedMessages() {
  MSG msg = {};
  // To protect against overly busy message queues, we limit the time
  // we process tasks to a few milliseconds. If we don't do that, there's
  // a chance that timer tasks won't ever run.
  static const int kMaxTaskProcessingTimeMs = 500;
  auto start = GetTick();
  while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) &&
         msg.message != WM_QUIT) {
    if (!msg.hwnd) {
      switch (msg.message) {
        // TODO(tommi): Stop using this way of queueing tasks.
        case WM_RUN_TASK: {
          QueuedTask* task = reinterpret_cast<QueuedTask*>(msg.lParam);
          if (task->Run())
            delete task;
          break;
        }
        case WM_QUEUE_DELAYED_TASK: {
          std::unique_ptr<DelayedTaskInfo> info(
              reinterpret_cast<DelayedTaskInfo*>(msg.lParam));
          bool need_to_schedule_timers =
              timer_tasks_.empty() ||
              timer_tasks_.top().due_time() > info->due_time();
          timer_tasks_.emplace(std::move(*info.get()));
          if (need_to_schedule_timers) {
            CancelTimers();
            ScheduleNextTimer();
          }
          break;
        }
        case WM_TIMER: {
          RTC_DCHECK_EQ(timer_id_, msg.wParam);
          ::KillTimer(nullptr, msg.wParam);
          timer_id_ = 0;
          RunDueTasks();
          ScheduleNextTimer();
          break;
        }
        default:
          RTC_NOTREACHED();
          break;
      }
    } else {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    if (GetTick() > start + kMaxTaskProcessingTimeMs)
      break;
  }
  return msg.message != WM_QUIT;
}

void TaskQueue::Impl::ThreadState::RunDueTasks() {
  RTC_DCHECK(!timer_tasks_.empty());
  auto now = GetTick();
  do {
    const auto& top = timer_tasks_.top();
    if (top.due_time() > now)
      break;
    top.Run();
    timer_tasks_.pop();
  } while (!timer_tasks_.empty());
}

void TaskQueue::Impl::ThreadState::ScheduleNextTimer() {
  RTC_DCHECK_EQ(timer_id_, 0);
  if (timer_tasks_.empty())
    return;

  const auto& next_task = timer_tasks_.top();
  int64_t delay_ms = std::max(0ll, next_task.due_time() - GetTick());
  uint32_t milliseconds = rtc::dchecked_cast<uint32_t>(delay_ms);
  if (!timer_.StartOneShotTimer(milliseconds))
    timer_id_ = ::SetTimer(nullptr, 0, milliseconds, nullptr);
}

void TaskQueue::Impl::ThreadState::CancelTimers() {
  timer_.Cancel();
  if (timer_id_) {
    ::KillTimer(nullptr, timer_id_);
    timer_id_ = 0;
  }
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
