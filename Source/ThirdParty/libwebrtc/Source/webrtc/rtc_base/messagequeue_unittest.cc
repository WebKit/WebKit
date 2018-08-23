/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/messagequeue.h"

#include <functional>

#include "rtc_base/atomicops.h"
#include "rtc_base/bind.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/nullsocketserver.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"

using namespace rtc;

class MessageQueueTest : public testing::Test, public MessageQueue {
 public:
  MessageQueueTest() : MessageQueue(SocketServer::CreateDefault(), true) {}
  bool IsLocked_Worker() {
    if (!crit_.TryEnter()) {
      return true;
    }
    crit_.Leave();
    return false;
  }
  bool IsLocked() {
    // We have to do this on a worker thread, or else the TryEnter will
    // succeed, since our critical sections are reentrant.
    std::unique_ptr<Thread> worker(Thread::CreateWithSocketServer());
    worker->Start();
    return worker->Invoke<bool>(
        RTC_FROM_HERE, rtc::Bind(&MessageQueueTest::IsLocked_Worker, this));
  }
};

struct DeletedLockChecker {
  DeletedLockChecker(MessageQueueTest* test, bool* was_locked, bool* deleted)
      : test(test), was_locked(was_locked), deleted(deleted) {}
  ~DeletedLockChecker() {
    *deleted = true;
    *was_locked = test->IsLocked();
  }
  MessageQueueTest* test;
  bool* was_locked;
  bool* deleted;
};

static void DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(
    MessageQueue* q) {
  EXPECT_TRUE(q != nullptr);
  int64_t now = TimeMillis();
  q->PostAt(RTC_FROM_HERE, now, nullptr, 3);
  q->PostAt(RTC_FROM_HERE, now - 2, nullptr, 0);
  q->PostAt(RTC_FROM_HERE, now - 1, nullptr, 1);
  q->PostAt(RTC_FROM_HERE, now, nullptr, 4);
  q->PostAt(RTC_FROM_HERE, now - 1, nullptr, 2);

  Message msg;
  for (size_t i = 0; i < 5; ++i) {
    memset(&msg, 0, sizeof(msg));
    EXPECT_TRUE(q->Get(&msg, 0));
    EXPECT_EQ(i, msg.message_id);
  }

  EXPECT_FALSE(q->Get(&msg, 0));  // No more messages
}

TEST_F(MessageQueueTest,
       DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder) {
  MessageQueue q(SocketServer::CreateDefault(), true);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q);

  NullSocketServer nullss;
  MessageQueue q_nullss(&nullss, true);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q_nullss);
}

TEST_F(MessageQueueTest, DisposeNotLocked) {
  bool was_locked = true;
  bool deleted = false;
  DeletedLockChecker* d = new DeletedLockChecker(this, &was_locked, &deleted);
  Dispose(d);
  Message msg;
  EXPECT_FALSE(Get(&msg, 0));
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(was_locked);
}

class DeletedMessageHandler : public MessageHandler {
 public:
  explicit DeletedMessageHandler(bool* deleted) : deleted_(deleted) {}
  ~DeletedMessageHandler() override { *deleted_ = true; }
  void OnMessage(Message* msg) override {}

 private:
  bool* deleted_;
};

TEST_F(MessageQueueTest, DiposeHandlerWithPostedMessagePending) {
  bool deleted = false;
  DeletedMessageHandler* handler = new DeletedMessageHandler(&deleted);
  // First, post a dispose.
  Dispose(handler);
  // Now, post a message, which should *not* be returned by Get().
  Post(RTC_FROM_HERE, handler, 1);
  Message msg;
  EXPECT_FALSE(Get(&msg, 0));
  EXPECT_TRUE(deleted);
}

struct UnwrapMainThreadScope {
  UnwrapMainThreadScope() : rewrap_(Thread::Current() != nullptr) {
    if (rewrap_)
      ThreadManager::Instance()->UnwrapCurrentThread();
  }
  ~UnwrapMainThreadScope() {
    if (rewrap_)
      ThreadManager::Instance()->WrapCurrentThread();
  }

 private:
  bool rewrap_;
};

TEST(MessageQueueManager, Clear) {
  UnwrapMainThreadScope s;
  if (MessageQueueManager::IsInitialized()) {
    RTC_LOG(LS_INFO)
        << "Unable to run MessageQueueManager::Clear test, since the "
        << "MessageQueueManager was already initialized by some "
        << "other test in this run.";
    return;
  }
  bool deleted = false;
  DeletedMessageHandler* handler = new DeletedMessageHandler(&deleted);
  delete handler;
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(MessageQueueManager::IsInitialized());
}

// Ensure that ProcessAllMessageQueues does its essential function; process
// all messages (both delayed and non delayed) up until the current time, on
// all registered message queues.
TEST(MessageQueueManager, ProcessAllMessageQueues) {
  Event entered_process_all_message_queues(true, false);
  auto a = Thread::CreateWithSocketServer();
  auto b = Thread::CreateWithSocketServer();
  a->Start();
  b->Start();

  volatile int messages_processed = 0;
  FunctorMessageHandler<void, std::function<void()>> incrementer(
      [&messages_processed, &entered_process_all_message_queues] {
        // Wait for event as a means to ensure Increment doesn't occur outside
        // of ProcessAllMessageQueues. The event is set by a message posted to
        // the main thread, which is guaranteed to be handled inside
        // ProcessAllMessageQueues.
        entered_process_all_message_queues.Wait(Event::kForever);
        AtomicOps::Increment(&messages_processed);
      });
  FunctorMessageHandler<void, std::function<void()>> event_signaler(
      [&entered_process_all_message_queues] {
        entered_process_all_message_queues.Set();
      });

  // Post messages (both delayed and non delayed) to both threads.
  a->Post(RTC_FROM_HERE, &incrementer);
  b->Post(RTC_FROM_HERE, &incrementer);
  a->PostDelayed(RTC_FROM_HERE, 0, &incrementer);
  b->PostDelayed(RTC_FROM_HERE, 0, &incrementer);
  rtc::Thread::Current()->Post(RTC_FROM_HERE, &event_signaler);

  MessageQueueManager::ProcessAllMessageQueues();
  EXPECT_EQ(4, AtomicOps::AcquireLoad(&messages_processed));
}

// Test that ProcessAllMessageQueues doesn't hang if a thread is quitting.
TEST(MessageQueueManager, ProcessAllMessageQueuesWithQuittingThread) {
  auto t = Thread::CreateWithSocketServer();
  t->Start();
  t->Quit();
  MessageQueueManager::ProcessAllMessageQueues();
}

// Test that ProcessAllMessageQueues doesn't hang if a queue clears its
// messages.
TEST(MessageQueueManager, ProcessAllMessageQueuesWithClearedQueue) {
  Event entered_process_all_message_queues(true, false);
  auto t = Thread::CreateWithSocketServer();
  t->Start();

  FunctorMessageHandler<void, std::function<void()>> clearer(
      [&entered_process_all_message_queues] {
        // Wait for event as a means to ensure Clear doesn't occur outside of
        // ProcessAllMessageQueues. The event is set by a message posted to the
        // main thread, which is guaranteed to be handled inside
        // ProcessAllMessageQueues.
        entered_process_all_message_queues.Wait(Event::kForever);
        rtc::Thread::Current()->Clear(nullptr);
      });
  FunctorMessageHandler<void, std::function<void()>> event_signaler(
      [&entered_process_all_message_queues] {
        entered_process_all_message_queues.Set();
      });

  // Post messages (both delayed and non delayed) to both threads.
  t->Post(RTC_FROM_HERE, &clearer);
  rtc::Thread::Current()->Post(RTC_FROM_HERE, &event_signaler);
  MessageQueueManager::ProcessAllMessageQueues();
}

class RefCountedHandler : public MessageHandler, public rtc::RefCountInterface {
 public:
  void OnMessage(Message* msg) override {}
};

class EmptyHandler : public MessageHandler {
 public:
  void OnMessage(Message* msg) override {}
};

TEST(MessageQueueManager, ClearReentrant) {
  std::unique_ptr<Thread> t(Thread::Create());
  EmptyHandler handler;
  RefCountedHandler* inner_handler(
      new rtc::RefCountedObject<RefCountedHandler>());
  // When the empty handler is destroyed, it will clear messages queued for
  // itself. The message to be cleared itself wraps a MessageHandler object
  // (RefCountedHandler) so this will cause the message queue to be cleared
  // again in a re-entrant fashion, which previously triggered a DCHECK.
  // The inner handler will be removed in a re-entrant fashion from the
  // message queue of the thread while the outer handler is removed, verifying
  // that the iterator is not invalidated in "MessageQueue::Clear".
  t->Post(RTC_FROM_HERE, inner_handler, 0);
  t->Post(RTC_FROM_HERE, &handler, 0,
          new ScopedRefMessageData<RefCountedHandler>(inner_handler));
}
