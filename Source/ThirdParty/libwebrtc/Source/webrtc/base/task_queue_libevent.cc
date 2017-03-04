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

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "base/third_party/libevent/event.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/task_queue_posix.h"
#include "webrtc/base/timeutils.h"

namespace rtc {
using internal::GetQueuePtrTls;
using internal::AutoSetCurrentQueuePtr;

namespace {
static const char kQuit = 1;
static const char kRunTask = 2;
static const char kRunReplyTask = 3;

using Priority = TaskQueue::Priority;

// This ignores the SIGPIPE signal on the calling thread.
// This signal can be fired when trying to write() to a pipe that's being
// closed or while closing a pipe that's being written to.
// We can run into that situation (e.g. reply tasks that don't get a chance to
// run because the task queue is being deleted) so we ignore this signal and
// continue as normal.
// As a side note for this implementation, it would be great if we could safely
// restore the sigmask, but unfortunately the operation of restoring it, can
// itself actually cause SIGPIPE to be signaled :-| (e.g. on MacOS)
// The SIGPIPE signal by default causes the process to be terminated, so we
// don't want to risk that.
// An alternative to this approach is to ignore the signal for the whole
// process:
//   signal(SIGPIPE, SIG_IGN);
void IgnoreSigPipeSignalOnCurrentThread() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &sigpipe_mask, nullptr);
}

struct TimerEvent {
  explicit TimerEvent(std::unique_ptr<QueuedTask> task)
      : task(std::move(task)) {}
  ~TimerEvent() { event_del(&ev); }
  event ev;
  std::unique_ptr<QueuedTask> task;
};

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  RTC_CHECK(flags != -1);
  return (flags & O_NONBLOCK) || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// TODO(tommi): This is a hack to support two versions of libevent that we're
// compatible with.  The method we really want to call is event_assign(),
// since event_set() has been marked as deprecated (and doesn't accept
// passing event_base__ as a parameter).  However, the version of libevent
// that we have in Chromium, doesn't have event_assign(), so we need to call
// event_set() there.
void EventAssign(struct event* ev,
                 struct event_base* base,
                 int fd,
                 short events,
                 void (*callback)(int, short, void*),
                 void* arg) {
#if defined(_EVENT2_EVENT_H_)
  RTC_CHECK_EQ(0, event_assign(ev, base, fd, events, callback, arg));
#else
  event_set(ev, fd, events, callback, arg);
  RTC_CHECK_EQ(0, event_base_set(base, ev));
#endif
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
}  // namespace

struct TaskQueue::QueueContext {
  explicit QueueContext(TaskQueue* q) : queue(q), is_active(true) {}
  TaskQueue* queue;
  bool is_active;
  // Holds a list of events pending timers for cleanup when the loop exits.
  std::list<TimerEvent*> pending_timers_;
};

// Posting a reply task is tricky business. This class owns the reply task
// and a reference to it is held by both the reply queue and the first task.
// Here's an outline of what happens when dealing with a reply task.
// * The ReplyTaskOwner owns the |reply_| task.
// * One ref owned by PostAndReplyTask
// * One ref owned by the reply TaskQueue
// * ReplyTaskOwner has a flag |run_task_| initially set to false.
// * ReplyTaskOwner has a method: HasOneRef() (provided by RefCountedObject).
// * After successfully running the original |task_|, PostAndReplyTask() calls
//   set_should_run_task(). This sets |run_task_| to true.
// * In PostAndReplyTask's dtor:
//   * It releases its reference to ReplyTaskOwner (important to do this first).
//   * Sends (write()) a kRunReplyTask message to the reply queue's pipe.
// * PostAndReplyTask doesn't care if write() fails, but when it does:
//   * The reply queue is gone.
//   * ReplyTaskOwner has already been deleted and the reply task too.
// * If write() succeeds:
//   * ReplyQueue receives the kRunReplyTask message
//   * Goes through all pending tasks, finding the first that HasOneRef()
//   * Calls ReplyTaskOwner::Run()
//     * if set_should_run_task() was called, the reply task will be run
//   * Release the reference to ReplyTaskOwner
//   * ReplyTaskOwner and associated |reply_| are deleted.
class TaskQueue::ReplyTaskOwner {
 public:
  ReplyTaskOwner(std::unique_ptr<QueuedTask> reply)
      : reply_(std::move(reply)) {}

  void Run() {
    RTC_DCHECK(reply_);
    if (run_task_) {
      if (!reply_->Run())
        reply_.release();
    }
    reply_.reset();
  }

  void set_should_run_task() {
    RTC_DCHECK(!run_task_);
    run_task_ = true;
  }

 private:
  std::unique_ptr<QueuedTask> reply_;
  bool run_task_ = false;
};

class TaskQueue::PostAndReplyTask : public QueuedTask {
 public:
  PostAndReplyTask(std::unique_ptr<QueuedTask> task,
                   std::unique_ptr<QueuedTask> reply,
                   TaskQueue* reply_queue,
                   int reply_pipe)
      : task_(std::move(task)),
        reply_pipe_(reply_pipe),
        reply_task_owner_(
            new RefCountedObject<ReplyTaskOwner>(std::move(reply))) {
    reply_queue->PrepareReplyTask(reply_task_owner_);
  }

  ~PostAndReplyTask() override {
    reply_task_owner_ = nullptr;
    IgnoreSigPipeSignalOnCurrentThread();
    // Send a signal to the reply queue that the reply task can run now.
    // Depending on whether |set_should_run_task()| was called by the
    // PostAndReplyTask(), the reply task may or may not actually run.
    // In either case, it will be deleted.
    char message = kRunReplyTask;
    write(reply_pipe_, &message, sizeof(message));
  }

 private:
  bool Run() override {
    if (!task_->Run())
      task_.release();
    reply_task_owner_->set_should_run_task();
    return true;
  }

  std::unique_ptr<QueuedTask> task_;
  int reply_pipe_;
  scoped_refptr<RefCountedObject<ReplyTaskOwner>> reply_task_owner_;
};

class TaskQueue::SetTimerTask : public QueuedTask {
 public:
  SetTimerTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds)
      : task_(std::move(task)),
        milliseconds_(milliseconds),
        posted_(Time32()) {}

 private:
  bool Run() override {
    // Compensate for the time that has passed since construction
    // and until we got here.
    uint32_t post_time = Time32() - posted_;
    TaskQueue::Current()->PostDelayedTask(
        std::move(task_),
        post_time > milliseconds_ ? 0 : milliseconds_ - post_time);
    return true;
  }

  std::unique_ptr<QueuedTask> task_;
  const uint32_t milliseconds_;
  const uint32_t posted_;
};

TaskQueue::TaskQueue(const char* queue_name, Priority priority /*= NORMAL*/)
    : event_base_(event_base_new()),
      wakeup_event_(new event()),
      thread_(&TaskQueue::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  int fds[2];
  RTC_CHECK(pipe(fds) == 0);
  SetNonBlocking(fds[0]);
  SetNonBlocking(fds[1]);
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];

  EventAssign(wakeup_event_.get(), event_base_, wakeup_pipe_out_,
              EV_READ | EV_PERSIST, OnWakeup, this);
  event_add(wakeup_event_.get(), 0);
  thread_.Start();
}

TaskQueue::~TaskQueue() {
  RTC_DCHECK(!IsCurrent());
  struct timespec ts;
  char message = kQuit;
  while (write(wakeup_pipe_in_, &message, sizeof(message)) != sizeof(message)) {
    // The queue is full, so we have no choice but to wait and retry.
    RTC_CHECK_EQ(EAGAIN, errno);
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    nanosleep(&ts, nullptr);
  }

  thread_.Stop();

  event_del(wakeup_event_.get());

  IgnoreSigPipeSignalOnCurrentThread();

  close(wakeup_pipe_in_);
  close(wakeup_pipe_out_);
  wakeup_pipe_in_ = -1;
  wakeup_pipe_out_ = -1;

  event_base_free(event_base_);
}

// static
TaskQueue* TaskQueue::Current() {
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
  return ctx ? ctx->queue : nullptr;
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
  RTC_DCHECK(task.get());
  // libevent isn't thread safe.  This means that we can't use methods such
  // as event_base_once to post tasks to the worker thread from a different
  // thread.  However, we can use it when posting from the worker thread itself.
  if (IsCurrent()) {
    if (event_base_once(event_base_, -1, EV_TIMEOUT, &TaskQueue::RunTask,
                        task.get(), nullptr) == 0) {
      task.release();
    }
  } else {
    QueuedTask* task_id = task.get();  // Only used for comparison.
    {
      CritScope lock(&pending_lock_);
      pending_.push_back(std::move(task));
    }
    char message = kRunTask;
    if (write(wakeup_pipe_in_, &message, sizeof(message)) != sizeof(message)) {
      LOG(WARNING) << "Failed to queue task.";
      CritScope lock(&pending_lock_);
      pending_.remove_if([task_id](std::unique_ptr<QueuedTask>& t) {
        return t.get() == task_id;
      });
    }
  }
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  if (IsCurrent()) {
    TimerEvent* timer = new TimerEvent(std::move(task));
    EventAssign(&timer->ev, event_base_, -1, 0, &TaskQueue::RunTimer, timer);
    QueueContext* ctx =
        static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
    ctx->pending_timers_.push_back(timer);
    timeval tv = {milliseconds / 1000, (milliseconds % 1000) * 1000};
    event_add(&timer->ev, &tv);
  } else {
    PostTask(std::unique_ptr<QueuedTask>(
        new SetTimerTask(std::move(task), milliseconds)));
  }
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  std::unique_ptr<QueuedTask> wrapper_task(
      new PostAndReplyTask(std::move(task), std::move(reply), reply_queue,
                           reply_queue->wakeup_pipe_in_));
  PostTask(std::move(wrapper_task));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return PostTaskAndReply(std::move(task), std::move(reply), Current());
}

// static
void TaskQueue::ThreadMain(void* context) {
  TaskQueue* me = static_cast<TaskQueue*>(context);

  QueueContext queue_context(me);
  pthread_setspecific(GetQueuePtrTls(), &queue_context);

  while (queue_context.is_active)
    event_base_loop(me->event_base_, 0);

  pthread_setspecific(GetQueuePtrTls(), nullptr);

  for (TimerEvent* timer : queue_context.pending_timers_)
    delete timer;
}

// static
void TaskQueue::OnWakeup(int socket, short flags, void* context) {  // NOLINT
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
  RTC_DCHECK(ctx->queue->wakeup_pipe_out_ == socket);
  char buf;
  RTC_CHECK(sizeof(buf) == read(socket, &buf, sizeof(buf)));
  switch (buf) {
    case kQuit:
      ctx->is_active = false;
      event_base_loopbreak(ctx->queue->event_base_);
      break;
    case kRunTask: {
      std::unique_ptr<QueuedTask> task;
      {
        CritScope lock(&ctx->queue->pending_lock_);
        RTC_DCHECK(!ctx->queue->pending_.empty());
        task = std::move(ctx->queue->pending_.front());
        ctx->queue->pending_.pop_front();
        RTC_DCHECK(task.get());
      }
      if (!task->Run())
        task.release();
      break;
    }
    case kRunReplyTask: {
      scoped_refptr<ReplyTaskOwnerRef> reply_task;
      {
        CritScope lock(&ctx->queue->pending_lock_);
        for (auto it = ctx->queue->pending_replies_.begin();
             it != ctx->queue->pending_replies_.end(); ++it) {
          if ((*it)->HasOneRef()) {
            reply_task = std::move(*it);
            ctx->queue->pending_replies_.erase(it);
            break;
          }
        }
      }
      reply_task->Run();
      break;
    }
    default:
      RTC_NOTREACHED();
      break;
  }
}

// static
void TaskQueue::RunTask(int fd, short flags, void* context) {  // NOLINT
  auto* task = static_cast<QueuedTask*>(context);
  if (task->Run())
    delete task;
}

// static
void TaskQueue::RunTimer(int fd, short flags, void* context) {  // NOLINT
  TimerEvent* timer = static_cast<TimerEvent*>(context);
  if (!timer->task->Run())
    timer->task.release();
  QueueContext* ctx =
      static_cast<QueueContext*>(pthread_getspecific(GetQueuePtrTls()));
  ctx->pending_timers_.remove(timer);
  delete timer;
}

void TaskQueue::PrepareReplyTask(scoped_refptr<ReplyTaskOwnerRef> reply_task) {
  RTC_DCHECK(reply_task);
  CritScope lock(&pending_lock_);
  pending_replies_.push_back(std::move(reply_task));
}

}  // namespace rtc
