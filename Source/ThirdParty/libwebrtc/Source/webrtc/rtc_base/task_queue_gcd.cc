/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the implementation of TaskQueue for Mac and iOS.
// The implementation uses Grand Central Dispatch queues (GCD) to
// do the actual task queuing.

#include "rtc_base/task_queue.h"

#include <string.h>

#include <dispatch/dispatch.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/task_queue_posix.h"

namespace rtc {
namespace {

using Priority = TaskQueue::Priority;

int TaskQueuePriorityToGCD(Priority priority) {
  switch (priority) {
    case Priority::NORMAL:
      return DISPATCH_QUEUE_PRIORITY_DEFAULT;
    case Priority::HIGH:
      return DISPATCH_QUEUE_PRIORITY_HIGH;
    case Priority::LOW:
      return DISPATCH_QUEUE_PRIORITY_LOW;
  }
}
}

using internal::GetQueuePtrTls;
using internal::AutoSetCurrentQueuePtr;

class TaskQueue::Impl : public RefCountInterface {
 public:
  Impl(const char* queue_name, TaskQueue* task_queue, Priority priority);
  ~Impl() override;

  static TaskQueue* Current();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  void PostTask(std::unique_ptr<QueuedTask> task);
  void PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                        std::unique_ptr<QueuedTask> reply,
                        TaskQueue::Impl* reply_queue);

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

 private:
  struct QueueContext {
    explicit QueueContext(TaskQueue* q) : queue(q), is_active(true) {}

    static void SetNotActive(void* context) {
      QueueContext* qc = static_cast<QueueContext*>(context);
      qc->is_active = false;
    }

    static void DeleteContext(void* context) {
      QueueContext* qc = static_cast<QueueContext*>(context);
      delete qc;
    }

    TaskQueue* const queue;
    bool is_active;
  };

  struct TaskContext {
    TaskContext(QueueContext* queue_ctx, std::unique_ptr<QueuedTask> task)
        : queue_ctx(queue_ctx), task(std::move(task)) {}
    virtual ~TaskContext() {}

    static void RunTask(void* context) {
      std::unique_ptr<TaskContext> tc(static_cast<TaskContext*>(context));
      if (tc->queue_ctx->is_active) {
        AutoSetCurrentQueuePtr set_current(tc->queue_ctx->queue);
        if (!tc->task->Run())
          tc->task.release();
      }
    }

    QueueContext* const queue_ctx;
    std::unique_ptr<QueuedTask> task;
  };

  // Special case context for holding two tasks, a |first_task| + the task
  // that's owned by the parent struct, TaskContext, that then becomes the
  // second (i.e. 'reply') task.
  struct PostTaskAndReplyContext : public TaskContext {
    explicit PostTaskAndReplyContext(QueueContext* first_queue_ctx,
                                     std::unique_ptr<QueuedTask> first_task,
                                     QueueContext* second_queue_ctx,
                                     std::unique_ptr<QueuedTask> second_task)
        : TaskContext(second_queue_ctx, std::move(second_task)),
          first_queue_ctx(first_queue_ctx),
          first_task(std::move(first_task)),
          reply_queue_(second_queue_ctx->queue->impl_->queue_) {
      // Retain the reply queue for as long as this object lives.
      // If we don't, we may have memory leaks and/or failures.
      dispatch_retain(reply_queue_);
    }
    ~PostTaskAndReplyContext() override { dispatch_release(reply_queue_); }

    static void RunTask(void* context) {
      auto* rc = static_cast<PostTaskAndReplyContext*>(context);
      if (rc->first_queue_ctx->is_active) {
        AutoSetCurrentQueuePtr set_current(rc->first_queue_ctx->queue);
        if (!rc->first_task->Run())
          rc->first_task.release();
      }
      // Post the reply task.  This hands the work over to the parent struct.
      // This task will eventually delete |this|.
      dispatch_async_f(rc->reply_queue_, rc, &TaskContext::RunTask);
    }

    QueueContext* const first_queue_ctx;
    std::unique_ptr<QueuedTask> first_task;
    dispatch_queue_t reply_queue_;
  };

  dispatch_queue_t queue_;
  QueueContext* const context_;
};

TaskQueue::Impl::Impl(const char* queue_name,
                      TaskQueue* task_queue,
                      Priority priority)
    : queue_(dispatch_queue_create(queue_name, DISPATCH_QUEUE_SERIAL)),
      context_(new QueueContext(task_queue)) {
  RTC_DCHECK(queue_name);
  RTC_CHECK(queue_);
  dispatch_set_context(queue_, context_);
  // Assign a finalizer that will delete the context when the last reference
  // to the queue is released.  This may run after the TaskQueue object has
  // been deleted.
  dispatch_set_finalizer_f(queue_, &QueueContext::DeleteContext);

  dispatch_set_target_queue(
      queue_, dispatch_get_global_queue(TaskQueuePriorityToGCD(priority), 0));
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());
  // Implementation/behavioral note:
  // Dispatch queues are reference counted via calls to dispatch_retain and
  // dispatch_release. Pending blocks submitted to a queue also hold a
  // reference to the queue until they have finished. Once all references to a
  // queue have been released, the queue will be deallocated by the system.
  // This is why we check the context before running tasks.

  // Use dispatch_sync to set the context to null to guarantee that there's not
  // a race between checking the context and using it from a task.
  dispatch_sync_f(queue_, context_, &QueueContext::SetNotActive);
  dispatch_release(queue_);
}

// static
TaskQueue* TaskQueue::Impl::Current() {
  return static_cast<TaskQueue*>(pthread_getspecific(GetQueuePtrTls()));
}

bool TaskQueue::Impl::IsCurrent() const {
  RTC_DCHECK(queue_);
  const TaskQueue* current = Current();
  return current && this == current->impl_.get();
}

void TaskQueue::Impl::PostTask(std::unique_ptr<QueuedTask> task) {
  auto* context = new TaskContext(context_, std::move(task));
  dispatch_async_f(queue_, context, &TaskContext::RunTask);
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  auto* context = new TaskContext(context_, std::move(task));
  dispatch_after_f(
      dispatch_time(DISPATCH_TIME_NOW, milliseconds * NSEC_PER_MSEC), queue_,
      context, &TaskContext::RunTask);
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  auto* context = new PostTaskAndReplyContext(
      context_, std::move(task), reply_queue->context_, std::move(reply));
  dispatch_async_f(queue_, context, &PostTaskAndReplyContext::RunTask);
}

// Boilerplate for the PIMPL pattern.
TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(new RefCountedObject<TaskQueue::Impl>(queue_name, this, priority)) {
}

TaskQueue::~TaskQueue() {}

// static
TaskQueue* TaskQueue::Current() {
  return TaskQueue::Impl::Current();
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
