/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/asyncinvoker.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace rtc {

AsyncInvoker::AsyncInvoker() : invocation_complete_(false, false) {}

AsyncInvoker::~AsyncInvoker() {
  destroying_ = true;
  // Messages for this need to be cleared *before* our destructor is complete.
  MessageQueueManager::Clear(this);
  // And we need to wait for any invocations that are still in progress on
  // other threads.
  while (AtomicOps::AcquireLoad(&pending_invocations_)) {
    // If the destructor was called while AsyncInvoke was being called by
    // another thread, WITHIN an AsyncInvoked functor, it may do another
    // Thread::Post even after we called MessageQueueManager::Clear(this). So
    // we need to keep calling Clear to discard these posts.
    MessageQueueManager::Clear(this);
    invocation_complete_.Wait(Event::kForever);
  }
}

void AsyncInvoker::OnMessage(Message* msg) {
  // Get the AsyncClosure shared ptr from this message's data.
  ScopedMessageData<AsyncClosure>* data =
      static_cast<ScopedMessageData<AsyncClosure>*>(msg->pdata);
  // Execute the closure and trigger the return message if needed.
  data->inner_data().Execute();
  delete data;
}

void AsyncInvoker::Flush(Thread* thread, uint32_t id /*= MQID_ANY*/) {
  if (destroying_) return;

  // Run this on |thread| to reduce the number of context switches.
  if (Thread::Current() != thread) {
    thread->Invoke<void>(RTC_FROM_HERE,
                         Bind(&AsyncInvoker::Flush, this, thread, id));
    return;
  }

  MessageList removed;
  thread->Clear(this, id, &removed);
  for (MessageList::iterator it = removed.begin(); it != removed.end(); ++it) {
    // This message was pending on this thread, so run it now.
    thread->Send(it->posted_from, it->phandler, it->message_id, it->pdata);
  }
}

void AsyncInvoker::DoInvoke(const Location& posted_from,
                            Thread* thread,
                            std::unique_ptr<AsyncClosure> closure,
                            uint32_t id) {
  if (destroying_) {
    LOG(LS_WARNING) << "Tried to invoke while destroying the invoker.";
    return;
  }
  AtomicOps::Increment(&pending_invocations_);
  thread->Post(posted_from, this, id,
               new ScopedMessageData<AsyncClosure>(std::move(closure)));
}

void AsyncInvoker::DoInvokeDelayed(const Location& posted_from,
                                   Thread* thread,
                                   std::unique_ptr<AsyncClosure> closure,
                                   uint32_t delay_ms,
                                   uint32_t id) {
  if (destroying_) {
    LOG(LS_WARNING) << "Tried to invoke while destroying the invoker.";
    return;
  }
  AtomicOps::Increment(&pending_invocations_);
  thread->PostDelayed(posted_from, delay_ms, this, id,
                      new ScopedMessageData<AsyncClosure>(std::move(closure)));
}

GuardedAsyncInvoker::GuardedAsyncInvoker() : thread_(Thread::Current()) {
  thread_->SignalQueueDestroyed.connect(this,
                                        &GuardedAsyncInvoker::ThreadDestroyed);
}

GuardedAsyncInvoker::~GuardedAsyncInvoker() {
}

bool GuardedAsyncInvoker::Flush(uint32_t id) {
  rtc::CritScope cs(&crit_);
  if (thread_ == nullptr)
    return false;
  invoker_.Flush(thread_, id);
  return true;
}

void GuardedAsyncInvoker::ThreadDestroyed() {
  rtc::CritScope cs(&crit_);
  // We should never get more than one notification about the thread dying.
  RTC_DCHECK(thread_ != nullptr);
  thread_ = nullptr;
}

AsyncClosure::~AsyncClosure() {
  AtomicOps::Decrement(&invoker_->pending_invocations_);
  invoker_->invocation_complete_.Set();
}

}  // namespace rtc
