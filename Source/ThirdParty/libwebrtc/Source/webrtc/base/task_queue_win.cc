/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/task_queue.h"

#include <mmsystem.h>
#include <string.h>

#include <algorithm>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

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

#if defined(_WIN64)
DWORD GetTick() {
  static const UINT kPeriod = 1;
  bool high_res = (timeBeginPeriod(kPeriod) == TIMERR_NOERROR);
  DWORD ret = timeGetTime();
  if (high_res)
    timeEndPeriod(kPeriod);
  return ret;
}
#endif
}  // namespace

class TaskQueue::MultimediaTimer {
 public:
  // kMaxTimers defines the limit of how many MultimediaTimer instances should
  // be created.
  // Background: The maximum number of supported handles for Wait functions, is
  // MAXIMUM_WAIT_OBJECTS - 1 (63).
  // There are some ways to work around the limitation but as it turns out, the
  // limit of concurrently active multimedia timers per process, is much lower,
  // or 16. So there isn't much value in going to the lenghts required to
  // overcome the Wait limitations.
  // kMaxTimers is larger than 16 though since it is possible that 'complete' or
  // signaled timers that haven't been handled, are counted as part of
  // kMaxTimers and thus a multimedia timer can actually be queued even though
  // as far as we're concerned, there are more than 16 that are pending.
  static const int kMaxTimers = MAXIMUM_WAIT_OBJECTS - 1;

  // Controls how many MultimediaTimer instances a queue can hold before
  // attempting to garbage collect (GC) timers that aren't in use.
  static const int kInstanceThresholdGC = 8;

  MultimediaTimer() : event_(::CreateEvent(nullptr, false, false, nullptr)) {}

  MultimediaTimer(MultimediaTimer&& timer)
      : event_(timer.event_),
        timer_id_(timer.timer_id_),
        task_(std::move(timer.task_)) {
    RTC_DCHECK(event_);
    timer.event_ = nullptr;
    timer.timer_id_ = 0;
  }

  ~MultimediaTimer() { Close(); }

  // Implementing this operator is required because of the way
  // some stl algorithms work, such as std::rotate().
  MultimediaTimer& operator=(MultimediaTimer&& timer) {
    if (this != &timer) {
      Close();
      event_ = timer.event_;
      timer.event_ = nullptr;
      task_ = std::move(timer.task_);
      timer_id_ = timer.timer_id_;
      timer.timer_id_ = 0;
    }
    return *this;
  }

  bool StartOneShotTimer(std::unique_ptr<QueuedTask> task, UINT delay_ms) {
    RTC_DCHECK_EQ(0, timer_id_);
    RTC_DCHECK(event_ != nullptr);
    RTC_DCHECK(!task_.get());
    RTC_DCHECK(task.get());
    task_ = std::move(task);
    timer_id_ =
        ::timeSetEvent(delay_ms, 0, reinterpret_cast<LPTIMECALLBACK>(event_), 0,
                       TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
    return timer_id_ != 0;
  }

  std::unique_ptr<QueuedTask> Cancel() {
    if (timer_id_) {
      ::timeKillEvent(timer_id_);
      timer_id_ = 0;
    }
    return std::move(task_);
  }

  void OnEventSignaled() {
    RTC_DCHECK_NE(0, timer_id_);
    timer_id_ = 0;
    task_->Run() ? task_.reset() : static_cast<void>(task_.release());
  }

  HANDLE event() const { return event_; }

  bool is_active() const { return timer_id_ != 0; }

 private:
  void Close() {
    Cancel();

    if (event_) {
      ::CloseHandle(event_);
      event_ = nullptr;
    }
  }

  HANDLE event_ = nullptr;
  MMRESULT timer_id_ = 0;
  std::unique_ptr<QueuedTask> task_;

  RTC_DISALLOW_COPY_AND_ASSIGN(MultimediaTimer);
};

TaskQueue::TaskQueue(const char* queue_name, Priority priority /*= NORMAL*/)
    : thread_(&TaskQueue::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  thread_.Start();
  Event event(false, false);
  ThreadStartupData startup = {&event, this};
  RTC_CHECK(thread_.QueueAPC(&InitializeQueueThread,
                             reinterpret_cast<ULONG_PTR>(&startup)));
  event.Wait(Event::kForever);
}

TaskQueue::~TaskQueue() {
  RTC_DCHECK(!IsCurrent());
  while (!::PostThreadMessage(thread_.GetThreadRef(), WM_QUIT, 0, 0)) {
    RTC_CHECK_EQ(ERROR_NOT_ENOUGH_QUOTA, ::GetLastError());
    Sleep(1);
  }
  thread_.Stop();
}

// static
TaskQueue* TaskQueue::Current() {
  return static_cast<TaskQueue*>(::TlsGetValue(GetQueuePtrTls()));
}

// static
bool TaskQueue::IsCurrent(const char* queue_name) {
  TaskQueue* current = Current();
  return current && current->thread_.name().compare(queue_name) == 0;
}

bool TaskQueue::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  if (::PostThreadMessage(thread_.GetThreadRef(), WM_RUN_TASK, 0,
                          reinterpret_cast<LPARAM>(task.get()))) {
    task.release();
  }
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  WPARAM wparam;
#if defined(_WIN64)
  // GetTickCount() returns a fairly coarse tick count (resolution or about 8ms)
  // so this compensation isn't that accurate, but since we have unused 32 bits
  // on Win64, we might as well use them.
  wparam = (static_cast<WPARAM>(GetTick()) << 32) | milliseconds;
#else
  wparam = milliseconds;
#endif
  if (::PostThreadMessage(thread_.GetThreadRef(), WM_QUEUE_DELAYED_TASK, wparam,
                          reinterpret_cast<LPARAM>(task.get()))) {
    task.release();
  }
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
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

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return PostTaskAndReply(std::move(task), std::move(reply), Current());
}

// static
void TaskQueue::ThreadMain(void* context) {
  HANDLE timer_handles[MultimediaTimer::kMaxTimers];
  // Active multimedia timers.
  std::vector<MultimediaTimer> mm_timers;
  // Tasks that have been queued by using SetTimer/WM_TIMER.
  DelayedTasks delayed_tasks;

  while (true) {
    RTC_DCHECK(mm_timers.size() <= arraysize(timer_handles));
    DWORD count = 0;
    for (const auto& t : mm_timers) {
      if (!t.is_active())
        break;
      timer_handles[count++] = t.event();
    }
    // Make sure we do an alertable wait as that's required to allow APCs to run
    // (e.g. required for InitializeQueueThread and stopping the thread in
    // PlatformThread).
    DWORD result = ::MsgWaitForMultipleObjectsEx(count, timer_handles, INFINITE,
                                                 QS_ALLEVENTS, MWMO_ALERTABLE);
    RTC_CHECK_NE(WAIT_FAILED, result);
    // If we're not waiting for any timers, then count will be equal to
    // WAIT_OBJECT_0.  If we're waiting for timers, then |count| represents
    // "One more than the number of timers", which means that there's a
    // message in the queue that needs to be handled.
    // If |result| is less than |count|, then its value will be the index of the
    // timer that has been signaled.
    if (result == (WAIT_OBJECT_0 + count)) {
      if (!ProcessQueuedMessages(&delayed_tasks, &mm_timers))
        break;
    } else if (result < (WAIT_OBJECT_0 + count)) {
      mm_timers[result].OnEventSignaled();
      RTC_DCHECK(!mm_timers[result].is_active());
      // Reuse timer events by moving inactive timers to the back of the vector.
      // When new delayed tasks are queued, they'll get reused.
      if (mm_timers.size() > 1) {
        auto it = mm_timers.begin() + result;
        std::rotate(it, it + 1, mm_timers.end());
      }

      // Collect some garbage.
      if (mm_timers.size() > MultimediaTimer::kInstanceThresholdGC) {
        const auto inactive = std::find_if(
            mm_timers.begin(), mm_timers.end(),
            [](const MultimediaTimer& t) { return !t.is_active(); });
        if (inactive != mm_timers.end()) {
          // Since inactive timers are always moved to the back, we can
          // safely delete all timers following the first inactive one.
          mm_timers.erase(inactive, mm_timers.end());
        }
      }
    } else {
      RTC_DCHECK_EQ(WAIT_IO_COMPLETION, result);
    }
  }
}

// static
bool TaskQueue::ProcessQueuedMessages(DelayedTasks* delayed_tasks,
                                      std::vector<MultimediaTimer>* timers) {
  MSG msg = {};
  while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) &&
         msg.message != WM_QUIT) {
    if (!msg.hwnd) {
      switch (msg.message) {
        case WM_RUN_TASK: {
          QueuedTask* task = reinterpret_cast<QueuedTask*>(msg.lParam);
          if (task->Run())
            delete task;
          break;
        }
        case WM_QUEUE_DELAYED_TASK: {
          std::unique_ptr<QueuedTask> task(
              reinterpret_cast<QueuedTask*>(msg.lParam));
          uint32_t milliseconds = msg.wParam & 0xFFFFFFFF;
#if defined(_WIN64)
          // Subtract the time it took to queue the timer.
          const DWORD now = GetTick();
          DWORD post_time = now - (msg.wParam >> 32);
          milliseconds =
              post_time > milliseconds ? 0 : milliseconds - post_time;
#endif
          bool timer_queued = false;
          if (timers->size() < MultimediaTimer::kMaxTimers) {
            MultimediaTimer* timer = nullptr;
            auto available = std::find_if(
                timers->begin(), timers->end(),
                [](const MultimediaTimer& t) { return !t.is_active(); });
            if (available != timers->end()) {
              timer = &(*available);
            } else {
              timers->emplace_back();
              timer = &timers->back();
            }

            timer_queued =
                timer->StartOneShotTimer(std::move(task), milliseconds);
            if (!timer_queued) {
              // No more multimedia timers can be queued.
              // Detach the task and fall back on SetTimer.
              task = timer->Cancel();
            }
          }

          // When we fail to use multimedia timers, we fall back on the more
          // coarse SetTimer/WM_TIMER approach.
          if (!timer_queued) {
            UINT_PTR timer_id = ::SetTimer(nullptr, 0, milliseconds, nullptr);
            delayed_tasks->insert(std::make_pair(timer_id, task.release()));
          }
          break;
        }
        case WM_TIMER: {
          ::KillTimer(nullptr, msg.wParam);
          auto found = delayed_tasks->find(msg.wParam);
          RTC_DCHECK(found != delayed_tasks->end());
          if (!found->second->Run())
            found->second.release();
          delayed_tasks->erase(found);
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
  }
  return msg.message != WM_QUIT;
}

}  // namespace rtc
