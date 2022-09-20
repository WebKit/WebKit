/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/thread.h"

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "api/units/time_delta.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/internal/default_socket_server.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "test/testsupport/rtc_expect_death.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>  // NOLINT

#endif

namespace rtc {
namespace {

using ::webrtc::TimeDelta;

// Generates a sequence of numbers (collaboratively).
class TestGenerator {
 public:
  TestGenerator() : last(0), count(0) {}

  int Next(int prev) {
    int result = prev + last;
    last = result;
    count += 1;
    return result;
  }

  int last;
  int count;
};

struct TestMessage : public MessageData {
  explicit TestMessage(int v) : value(v) {}

  int value;
};

// Receives on a socket and sends by posting messages.
class SocketClient : public TestGenerator, public sigslot::has_slots<> {
 public:
  SocketClient(Socket* socket,
               const SocketAddress& addr,
               Thread* post_thread,
               MessageHandler* phandler)
      : socket_(AsyncUDPSocket::Create(socket, addr)),
        post_thread_(post_thread),
        post_handler_(phandler) {
    socket_->SignalReadPacket.connect(this, &SocketClient::OnPacket);
  }

  ~SocketClient() override { delete socket_; }

  SocketAddress address() const { return socket_->GetLocalAddress(); }

  void OnPacket(AsyncPacketSocket* socket,
                const char* buf,
                size_t size,
                const SocketAddress& remote_addr,
                const int64_t& packet_time_us) {
    EXPECT_EQ(size, sizeof(uint32_t));
    uint32_t prev = reinterpret_cast<const uint32_t*>(buf)[0];
    uint32_t result = Next(prev);

    post_thread_->PostDelayed(RTC_FROM_HERE, 200, post_handler_, 0,
                              new TestMessage(result));
  }

 private:
  AsyncUDPSocket* socket_;
  Thread* post_thread_;
  MessageHandler* post_handler_;
};

// Receives messages and sends on a socket.
class MessageClient : public MessageHandlerAutoCleanup, public TestGenerator {
 public:
  MessageClient(Thread* pth, Socket* socket) : socket_(socket) {}

  ~MessageClient() override { delete socket_; }

  void OnMessage(Message* pmsg) override {
    TestMessage* msg = static_cast<TestMessage*>(pmsg->pdata);
    int result = Next(msg->value);
    EXPECT_GE(socket_->Send(&result, sizeof(result)), 0);
    delete msg;
  }

 private:
  Socket* socket_;
};

class CustomThread : public rtc::Thread {
 public:
  CustomThread()
      : Thread(std::unique_ptr<SocketServer>(new rtc::NullSocketServer())) {}
  ~CustomThread() override { Stop(); }
  bool Start() { return false; }

  bool WrapCurrent() { return Thread::WrapCurrent(); }
  void UnwrapCurrent() { Thread::UnwrapCurrent(); }
};

// A thread that does nothing when it runs and signals an event
// when it is destroyed.
class SignalWhenDestroyedThread : public Thread {
 public:
  SignalWhenDestroyedThread(Event* event)
      : Thread(std::unique_ptr<SocketServer>(new NullSocketServer())),
        event_(event) {}

  ~SignalWhenDestroyedThread() override {
    Stop();
    event_->Set();
  }

  void Run() override {
    // Do nothing.
  }

 private:
  Event* event_;
};

// A bool wrapped in a mutex, to avoid data races. Using a volatile
// bool should be sufficient for correct code ("eventual consistency"
// between caches is sufficient), but we can't tell the compiler about
// that, and then tsan complains about a data race.

// See also discussion at
// http://stackoverflow.com/questions/7223164/is-mutex-needed-to-synchronize-a-simple-flag-between-pthreads

// Using std::atomic<bool> or std::atomic_flag in C++11 is probably
// the right thing to do, but those features are not yet allowed. Or
// rtc::AtomicInt, if/when that is added. Since the use isn't
// performance critical, use a plain critical section for the time
// being.

class AtomicBool {
 public:
  explicit AtomicBool(bool value = false) : flag_(value) {}
  AtomicBool& operator=(bool value) {
    webrtc::MutexLock scoped_lock(&mutex_);
    flag_ = value;
    return *this;
  }
  bool get() const {
    webrtc::MutexLock scoped_lock(&mutex_);
    return flag_;
  }

 private:
  mutable webrtc::Mutex mutex_;
  bool flag_;
};

// Function objects to test Thread::Invoke.
struct FunctorA {
  int operator()() { return 42; }
};
class FunctorB {
 public:
  explicit FunctorB(AtomicBool* flag) : flag_(flag) {}
  void operator()() {
    if (flag_)
      *flag_ = true;
  }

 private:
  AtomicBool* flag_;
};
struct FunctorC {
  int operator()() {
    Thread::Current()->ProcessMessages(50);
    return 24;
  }
};
struct FunctorD {
 public:
  explicit FunctorD(AtomicBool* flag) : flag_(flag) {}
  FunctorD(FunctorD&&) = default;

  FunctorD(const FunctorD&) = delete;
  FunctorD& operator=(const FunctorD&) = delete;

  FunctorD& operator=(FunctorD&&) = default;
  void operator()() {
    if (flag_)
      *flag_ = true;
  }

 private:
  AtomicBool* flag_;
};

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST(ThreadTest, DISABLED_Main) {
  const SocketAddress addr("127.0.0.1", 0);

  // Create the messaging client on its own thread.
  auto th1 = Thread::CreateWithSocketServer();
  Socket* socket = th1->socketserver()->CreateSocket(addr.family(), SOCK_DGRAM);
  MessageClient msg_client(th1.get(), socket);

  // Create the socket client on its own thread.
  auto th2 = Thread::CreateWithSocketServer();
  Socket* asocket =
      th2->socketserver()->CreateSocket(addr.family(), SOCK_DGRAM);
  SocketClient sock_client(asocket, addr, th1.get(), &msg_client);

  socket->Connect(sock_client.address());

  th1->Start();
  th2->Start();

  // Get the messages started.
  th1->PostDelayed(RTC_FROM_HERE, 100, &msg_client, 0, new TestMessage(1));

  // Give the clients a little while to run.
  // Messages will be processed at 100, 300, 500, 700, 900.
  Thread* th_main = Thread::Current();
  th_main->ProcessMessages(1000);

  // Stop the sending client. Give the receiver a bit longer to run, in case
  // it is running on a machine that is under load (e.g. the build machine).
  th1->Stop();
  th_main->ProcessMessages(200);
  th2->Stop();

  // Make sure the results were correct
  EXPECT_EQ(5, msg_client.count);
  EXPECT_EQ(34, msg_client.last);
  EXPECT_EQ(5, sock_client.count);
  EXPECT_EQ(55, sock_client.last);
}

TEST(ThreadTest, CountBlockingCalls) {
  rtc::AutoThread current;

  // When the test runs, this will print out:
  //   (thread_unittest.cc:262): Blocking TestBody: total=2 (actual=1, could=1)
  RTC_LOG_THREAD_BLOCK_COUNT();
#if RTC_DCHECK_IS_ON
  rtc::Thread::ScopedCountBlockingCalls blocked_calls(
      [&](uint32_t actual_block, uint32_t could_block) {
        EXPECT_EQ(1u, actual_block);
        EXPECT_EQ(1u, could_block);
      });

  EXPECT_EQ(0u, blocked_calls.GetBlockingCallCount());
  EXPECT_EQ(0u, blocked_calls.GetCouldBeBlockingCallCount());
  EXPECT_EQ(0u, blocked_calls.GetTotalBlockedCallCount());

  // Test invoking on the current thread. This should not count as an 'actual'
  // invoke, but should still count as an invoke that could block since we
  // that the call to Invoke serves a purpose in some configurations (and should
  // not be used a general way to call methods on the same thread).
  current.Invoke<void>(RTC_FROM_HERE, []() {});
  EXPECT_EQ(0u, blocked_calls.GetBlockingCallCount());
  EXPECT_EQ(1u, blocked_calls.GetCouldBeBlockingCallCount());
  EXPECT_EQ(1u, blocked_calls.GetTotalBlockedCallCount());

  // Create a new thread to invoke on.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  EXPECT_EQ(42, thread->Invoke<int>(RTC_FROM_HERE, []() { return 42; }));
  EXPECT_EQ(1u, blocked_calls.GetBlockingCallCount());
  EXPECT_EQ(1u, blocked_calls.GetCouldBeBlockingCallCount());
  EXPECT_EQ(2u, blocked_calls.GetTotalBlockedCallCount());
  thread->Stop();
  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(2);
#else
  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(0);
  RTC_LOG(LS_INFO) << "Test not active in this config";
#endif
}

#if RTC_DCHECK_IS_ON
TEST(ThreadTest, CountBlockingCallsOneCallback) {
  rtc::AutoThread current;
  bool was_called_back = false;
  {
    rtc::Thread::ScopedCountBlockingCalls blocked_calls(
        [&](uint32_t actual_block, uint32_t could_block) {
          was_called_back = true;
        });
    current.Invoke<void>(RTC_FROM_HERE, []() {});
  }
  EXPECT_TRUE(was_called_back);
}

TEST(ThreadTest, CountBlockingCallsSkipCallback) {
  rtc::AutoThread current;
  bool was_called_back = false;
  {
    rtc::Thread::ScopedCountBlockingCalls blocked_calls(
        [&](uint32_t actual_block, uint32_t could_block) {
          was_called_back = true;
        });
    // Changed `blocked_calls` to not issue the callback if there are 1 or
    // fewer blocking calls (i.e. we set the minimum required number to 2).
    blocked_calls.set_minimum_call_count_for_callback(2);
    current.Invoke<void>(RTC_FROM_HERE, []() {});
  }
  // We should not have gotten a call back.
  EXPECT_FALSE(was_called_back);
}
#endif

// Test that setting thread names doesn't cause a malfunction.
// There's no easy way to verify the name was set properly at this time.
TEST(ThreadTest, Names) {
  // Default name
  auto thread = Thread::CreateWithSocketServer();
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  // Name with no object parameter
  thread = Thread::CreateWithSocketServer();
  EXPECT_TRUE(thread->SetName("No object", nullptr));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  // Really long name
  thread = Thread::CreateWithSocketServer();
  EXPECT_TRUE(thread->SetName("Abcdefghijklmnopqrstuvwxyz1234567890", this));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
}

TEST(ThreadTest, Wrap) {
  Thread* current_thread = Thread::Current();
  ThreadManager::Instance()->SetCurrentThread(nullptr);

  {
    CustomThread cthread;
    EXPECT_TRUE(cthread.WrapCurrent());
    EXPECT_EQ(&cthread, Thread::Current());
    EXPECT_TRUE(cthread.RunningForTest());
    EXPECT_FALSE(cthread.IsOwned());
    cthread.UnwrapCurrent();
    EXPECT_FALSE(cthread.RunningForTest());
  }
  ThreadManager::Instance()->SetCurrentThread(current_thread);
}

#if (!defined(NDEBUG) || RTC_DCHECK_IS_ON)
TEST(ThreadTest, InvokeToThreadAllowedReturnsTrueWithoutPolicies) {
  rtc::AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();

  thread1->PostTask(
      [&]() { EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread2.get())); });
  main_thread.ProcessMessages(100);
}

TEST(ThreadTest, InvokeAllowedWhenThreadsAdded) {
  rtc::AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();
  auto thread3 = Thread::CreateWithSocketServer();
  auto thread4 = Thread::CreateWithSocketServer();

  thread1->AllowInvokesToThread(thread2.get());
  thread1->AllowInvokesToThread(thread3.get());

  thread1->PostTask([&]() {
    EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread2.get()));
    EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread3.get()));
    EXPECT_FALSE(thread1->IsInvokeToThreadAllowed(thread4.get()));
  });
  main_thread.ProcessMessages(100);
}

TEST(ThreadTest, InvokesDisallowedWhenDisallowAllInvokes) {
  rtc::AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();

  thread1->DisallowAllInvokes();

  thread1->PostTask(
      [&]() { EXPECT_FALSE(thread1->IsInvokeToThreadAllowed(thread2.get())); });
  main_thread.ProcessMessages(100);
}
#endif  // (!defined(NDEBUG) || RTC_DCHECK_IS_ON)

TEST(ThreadTest, InvokesAllowedByDefault) {
  rtc::AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();

  thread1->PostTask(
      [&]() { EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread2.get())); });
  main_thread.ProcessMessages(100);
}

TEST(ThreadTest, Invoke) {
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functors.
  EXPECT_EQ(42, thread->Invoke<int>(RTC_FROM_HERE, FunctorA()));
  AtomicBool called;
  FunctorB f2(&called);
  thread->Invoke<void>(RTC_FROM_HERE, f2);
  EXPECT_TRUE(called.get());
  // Try calling bare functions.
  struct LocalFuncs {
    static int Func1() { return 999; }
    static void Func2() {}
  };
  EXPECT_EQ(999, thread->Invoke<int>(RTC_FROM_HERE, &LocalFuncs::Func1));
  thread->Invoke<void>(RTC_FROM_HERE, &LocalFuncs::Func2);
}

// Verifies that two threads calling Invoke on each other at the same time does
// not deadlock but crash.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(ThreadTest, TwoThreadsInvokeDeathTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  AutoThread thread;
  Thread* main_thread = Thread::Current();
  auto other_thread = Thread::CreateWithSocketServer();
  other_thread->Start();
  other_thread->Invoke<void>(RTC_FROM_HERE, [main_thread] {
    RTC_EXPECT_DEATH(main_thread->Invoke<void>(RTC_FROM_HERE, [] {}), "loop");
  });
}

TEST(ThreadTest, ThreeThreadsInvokeDeathTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  AutoThread thread;
  Thread* first = Thread::Current();

  auto second = Thread::Create();
  second->Start();
  auto third = Thread::Create();
  third->Start();

  second->Invoke<void>(RTC_FROM_HERE, [&] {
    third->Invoke<void>(RTC_FROM_HERE, [&] {
      RTC_EXPECT_DEATH(first->Invoke<void>(RTC_FROM_HERE, [] {}), "loop");
    });
  });
}

#endif

// Verifies that if thread A invokes a call on thread B and thread C is trying
// to invoke A at the same time, thread A does not handle C's invoke while
// invoking B.
TEST(ThreadTest, ThreeThreadsInvoke) {
  AutoThread thread;
  Thread* thread_a = Thread::Current();
  auto thread_b = Thread::CreateWithSocketServer();
  auto thread_c = Thread::CreateWithSocketServer();
  thread_b->Start();
  thread_c->Start();

  class LockedBool {
   public:
    explicit LockedBool(bool value) : value_(value) {}

    void Set(bool value) {
      webrtc::MutexLock lock(&mutex_);
      value_ = value;
    }

    bool Get() {
      webrtc::MutexLock lock(&mutex_);
      return value_;
    }

   private:
    webrtc::Mutex mutex_;
    bool value_ RTC_GUARDED_BY(mutex_);
  };

  struct LocalFuncs {
    static void Set(LockedBool* out) { out->Set(true); }
    static void InvokeSet(Thread* thread, LockedBool* out) {
      thread->Invoke<void>(RTC_FROM_HERE, [out] { Set(out); });
    }

    // Set `out` true and call InvokeSet on `thread`.
    static void SetAndInvokeSet(LockedBool* out,
                                Thread* thread,
                                LockedBool* out_inner) {
      out->Set(true);
      InvokeSet(thread, out_inner);
    }

    // Asynchronously invoke SetAndInvokeSet on `thread1` and wait until
    // `thread1` starts the call.
    static void AsyncInvokeSetAndWait(DEPRECATED_AsyncInvoker* invoker,
                                      Thread* thread1,
                                      Thread* thread2,
                                      LockedBool* out) {
      LockedBool async_invoked(false);

      invoker->AsyncInvoke<void>(
          RTC_FROM_HERE, thread1, [&async_invoked, thread2, out] {
            SetAndInvokeSet(&async_invoked, thread2, out);
          });

      EXPECT_TRUE_WAIT(async_invoked.Get(), 2000);
    }
  };

  DEPRECATED_AsyncInvoker invoker;
  LockedBool thread_a_called(false);

  // Start the sequence A --(invoke)--> B --(async invoke)--> C --(invoke)--> A.
  // Thread B returns when C receives the call and C should be blocked until A
  // starts to process messages.
  Thread* thread_c_ptr = thread_c.get();
  thread_b->Invoke<void>(
      RTC_FROM_HERE, [&invoker, thread_c_ptr, thread_a, &thread_a_called] {
        LocalFuncs::AsyncInvokeSetAndWait(&invoker, thread_c_ptr, thread_a,
                                          &thread_a_called);
      });
  EXPECT_FALSE(thread_a_called.Get());

  EXPECT_TRUE_WAIT(thread_a_called.Get(), 2000);
}

class ThreadQueueTest : public ::testing::Test, public Thread {
 public:
  ThreadQueueTest() : Thread(CreateDefaultSocketServer(), true) {}
  bool IsLocked_Worker() {
    if (!CritForTest()->TryEnter()) {
      return true;
    }
    CritForTest()->Leave();
    return false;
  }
  bool IsLocked() {
    // We have to do this on a worker thread, or else the TryEnter will
    // succeed, since our critical sections are reentrant.
    std::unique_ptr<Thread> worker(Thread::CreateWithSocketServer());
    worker->Start();
    return worker->Invoke<bool>(RTC_FROM_HERE,
                                [this] { return IsLocked_Worker(); });
  }
};

struct DeletedLockChecker {
  DeletedLockChecker(ThreadQueueTest* test, bool* was_locked, bool* deleted)
      : test(test), was_locked(was_locked), deleted(deleted) {}
  ~DeletedLockChecker() {
    *deleted = true;
    *was_locked = test->IsLocked();
  }
  ThreadQueueTest* test;
  bool* was_locked;
  bool* deleted;
};

static void DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(Thread* q) {
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

TEST_F(ThreadQueueTest, DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder) {
  Thread q(CreateDefaultSocketServer(), true);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q);

  NullSocketServer nullss;
  Thread q_nullss(&nullss, true);
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(&q_nullss);
}

TEST_F(ThreadQueueTest, DisposeNotLocked) {
  bool was_locked = true;
  bool deleted = false;
  DeletedLockChecker* d = new DeletedLockChecker(this, &was_locked, &deleted);
  Dispose(d);
  Message msg;
  EXPECT_FALSE(Get(&msg, 0));
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(was_locked);
}

class DeletedMessageHandler : public MessageHandlerAutoCleanup {
 public:
  explicit DeletedMessageHandler(bool* deleted) : deleted_(deleted) {}
  ~DeletedMessageHandler() override { *deleted_ = true; }
  void OnMessage(Message* msg) override {}

 private:
  bool* deleted_;
};

TEST_F(ThreadQueueTest, DiposeHandlerWithPostedMessagePending) {
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

// Ensure that ProcessAllMessageQueues does its essential function; process
// all messages (both delayed and non delayed) up until the current time, on
// all registered message queues.
TEST(ThreadManager, ProcessAllMessageQueues) {
  rtc::AutoThread main_thread;
  Event entered_process_all_message_queues(true, false);
  auto a = Thread::CreateWithSocketServer();
  auto b = Thread::CreateWithSocketServer();
  a->Start();
  b->Start();

  std::atomic<int> messages_processed(0);
  auto incrementer = [&messages_processed,
                      &entered_process_all_message_queues] {
    // Wait for event as a means to ensure Increment doesn't occur outside
    // of ProcessAllMessageQueues. The event is set by a message posted to
    // the main thread, which is guaranteed to be handled inside
    // ProcessAllMessageQueues.
    entered_process_all_message_queues.Wait(Event::kForever);
    messages_processed.fetch_add(1);
  };
  auto event_signaler = [&entered_process_all_message_queues] {
    entered_process_all_message_queues.Set();
  };

  // Post messages (both delayed and non delayed) to both threads.
  a->PostTask(incrementer);
  b->PostTask(incrementer);
  a->PostDelayedTask(incrementer, TimeDelta::Zero());
  b->PostDelayedTask(incrementer, TimeDelta::Zero());
  main_thread.PostTask(event_signaler);

  ThreadManager::ProcessAllMessageQueuesForTesting();
  EXPECT_EQ(4, messages_processed.load(std::memory_order_acquire));
}

// Test that ProcessAllMessageQueues doesn't hang if a thread is quitting.
TEST(ThreadManager, ProcessAllMessageQueuesWithQuittingThread) {
  auto t = Thread::CreateWithSocketServer();
  t->Start();
  t->Quit();
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

// Test that ProcessAllMessageQueues doesn't hang if a queue clears its
// messages.
TEST(ThreadManager, ProcessAllMessageQueuesWithClearedQueue) {
  rtc::AutoThread main_thread;
  Event entered_process_all_message_queues(true, false);
  auto t = Thread::CreateWithSocketServer();
  t->Start();

  auto clearer = [&entered_process_all_message_queues] {
    // Wait for event as a means to ensure Clear doesn't occur outside of
    // ProcessAllMessageQueues. The event is set by a message posted to the
    // main thread, which is guaranteed to be handled inside
    // ProcessAllMessageQueues.
    entered_process_all_message_queues.Wait(Event::kForever);
    rtc::Thread::Current()->Clear(nullptr);
  };
  auto event_signaler = [&entered_process_all_message_queues] {
    entered_process_all_message_queues.Set();
  };

  // Post messages (both delayed and non delayed) to both threads.
  t->PostTask(clearer);
  main_thread.PostTask(event_signaler);
  ThreadManager::ProcessAllMessageQueuesForTesting();
}

class RefCountedHandler : public MessageHandlerAutoCleanup,
                          public rtc::RefCountInterface {
 public:
  void OnMessage(Message* msg) override {}
};

class EmptyHandler : public MessageHandlerAutoCleanup {
 public:
  void OnMessage(Message* msg) override {}
};

TEST(ThreadManager, ClearReentrant) {
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

class DEPRECATED_AsyncInvokeTest : public ::testing::Test {
 public:
  void IntCallback(int value) {
    EXPECT_EQ(expected_thread_, Thread::Current());
    int_value_ = value;
  }
  void SetExpectedThreadForIntCallback(Thread* thread) {
    expected_thread_ = thread;
  }

 protected:
  enum { kWaitTimeout = 1000 };
  DEPRECATED_AsyncInvokeTest() : int_value_(0), expected_thread_(nullptr) {}

  rtc::AutoThread main_thread_;
  int int_value_;
  Thread* expected_thread_;
};

TEST_F(DEPRECATED_AsyncInvokeTest, FireAndForget) {
  DEPRECATED_AsyncInvoker invoker;
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functor.
  AtomicBool called;
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, thread.get(), FunctorB(&called));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
  thread->Stop();
}

TEST_F(DEPRECATED_AsyncInvokeTest, NonCopyableFunctor) {
  DEPRECATED_AsyncInvoker invoker;
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functor.
  AtomicBool called;
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, thread.get(), FunctorD(&called));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
  thread->Stop();
}

TEST_F(DEPRECATED_AsyncInvokeTest, KillInvokerDuringExecute) {
  // Use these events to get in a state where the functor is in the middle of
  // executing, and then to wait for it to finish, ensuring the "EXPECT_FALSE"
  // is run.
  Event functor_started;
  Event functor_continue;
  Event functor_finished;

  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  volatile bool invoker_destroyed = false;
  {
    auto functor = [&functor_started, &functor_continue, &functor_finished,
                    &invoker_destroyed] {
      functor_started.Set();
      functor_continue.Wait(Event::kForever);
      rtc::Thread::Current()->SleepMs(kWaitTimeout);
      EXPECT_FALSE(invoker_destroyed);
      functor_finished.Set();
    };
    DEPRECATED_AsyncInvoker invoker;
    invoker.AsyncInvoke<void>(RTC_FROM_HERE, thread.get(), functor);
    functor_started.Wait(Event::kForever);

    // Destroy the invoker while the functor is still executing (doing
    // SleepMs).
    functor_continue.Set();
  }

  // If the destructor DIDN'T wait for the functor to finish executing, it will
  // hit the EXPECT_FALSE(invoker_destroyed) after it finishes sleeping for a
  // second.
  invoker_destroyed = true;
  functor_finished.Wait(Event::kForever);
}

// Variant of the above test where the async-invoked task calls AsyncInvoke
// *again*, for the thread on which the invoker is currently being destroyed.
// This shouldn't deadlock or crash. The second invocation should be ignored.
TEST_F(DEPRECATED_AsyncInvokeTest,
       KillInvokerDuringExecuteWithReentrantInvoke) {
  Event functor_started;
  // Flag used to verify that the recursively invoked task never actually runs.
  bool reentrant_functor_run = false;

  Thread* main = Thread::Current();
  Thread thread(std::make_unique<NullSocketServer>());
  thread.Start();
  {
    DEPRECATED_AsyncInvoker invoker;
    auto reentrant_functor = [&reentrant_functor_run] {
      reentrant_functor_run = true;
    };
    auto functor = [&functor_started, &invoker, main, reentrant_functor] {
      functor_started.Set();
      Thread::Current()->SleepMs(kWaitTimeout);
      invoker.AsyncInvoke<void>(RTC_FROM_HERE, main, reentrant_functor);
    };
    // This queues a task on `thread` to sleep for `kWaitTimeout` then queue a
    // task on `main`. But this second queued task should never run, since the
    // destructor will be entered before it's even invoked.
    invoker.AsyncInvoke<void>(RTC_FROM_HERE, &thread, functor);
    functor_started.Wait(Event::kForever);
  }
  EXPECT_FALSE(reentrant_functor_run);
}

void WaitAndSetEvent(Event* wait_event, Event* set_event) {
  wait_event->Wait(Event::kForever);
  set_event->Set();
}

// A functor that keeps track of the number of copies and moves.
class LifeCycleFunctor {
 public:
  struct Stats {
    size_t copy_count = 0;
    size_t move_count = 0;
  };

  LifeCycleFunctor(Stats* stats, Event* event) : stats_(stats), event_(event) {}
  LifeCycleFunctor(const LifeCycleFunctor& other) { *this = other; }
  LifeCycleFunctor(LifeCycleFunctor&& other) { *this = std::move(other); }

  LifeCycleFunctor& operator=(const LifeCycleFunctor& other) {
    stats_ = other.stats_;
    event_ = other.event_;
    ++stats_->copy_count;
    return *this;
  }

  LifeCycleFunctor& operator=(LifeCycleFunctor&& other) {
    stats_ = other.stats_;
    event_ = other.event_;
    ++stats_->move_count;
    return *this;
  }

  void operator()() { event_->Set(); }

 private:
  Stats* stats_;
  Event* event_;
};

// A functor that verifies the thread it was destroyed on.
class DestructionFunctor {
 public:
  DestructionFunctor(Thread* thread, bool* thread_was_current, Event* event)
      : thread_(thread),
        thread_was_current_(thread_was_current),
        event_(event) {}
  ~DestructionFunctor() {
    // Only signal the event if this was the functor that was invoked to avoid
    // the event being signaled due to the destruction of temporary/moved
    // versions of this object.
    if (was_invoked_) {
      *thread_was_current_ = thread_->IsCurrent();
      event_->Set();
    }
  }

  void operator()() { was_invoked_ = true; }

 private:
  Thread* thread_;
  bool* thread_was_current_;
  Event* event_;
  bool was_invoked_ = false;
};

TEST(ThreadPostTaskTest, InvokesWithLambda) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  background_thread->PostTask([&event] { event.Set(); });
  event.Wait(Event::kForever);
}

TEST(ThreadPostTaskTest, InvokesWithCopiedFunctor) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  LifeCycleFunctor::Stats stats;
  Event event;
  LifeCycleFunctor functor(&stats, &event);
  background_thread->PostTask(functor);
  event.Wait(Event::kForever);

  EXPECT_EQ(1u, stats.copy_count);
  EXPECT_EQ(0u, stats.move_count);
}

TEST(ThreadPostTaskTest, InvokesWithMovedFunctor) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  LifeCycleFunctor::Stats stats;
  Event event;
  LifeCycleFunctor functor(&stats, &event);
  background_thread->PostTask(std::move(functor));
  event.Wait(Event::kForever);

  EXPECT_EQ(0u, stats.copy_count);
  EXPECT_EQ(1u, stats.move_count);
}

TEST(ThreadPostTaskTest, InvokesWithReferencedFunctorShouldCopy) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  LifeCycleFunctor::Stats stats;
  Event event;
  LifeCycleFunctor functor(&stats, &event);
  LifeCycleFunctor& functor_ref = functor;
  background_thread->PostTask(functor_ref);
  event.Wait(Event::kForever);

  EXPECT_EQ(1u, stats.copy_count);
  EXPECT_EQ(0u, stats.move_count);
}

TEST(ThreadPostTaskTest, InvokesWithCopiedFunctorDestroyedOnTargetThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  DestructionFunctor functor(background_thread.get(),
                             &was_invoked_on_background_thread, &event);
  background_thread->PostTask(functor);
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(ThreadPostTaskTest, InvokesWithMovedFunctorDestroyedOnTargetThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  DestructionFunctor functor(background_thread.get(),
                             &was_invoked_on_background_thread, &event);
  background_thread->PostTask(std::move(functor));
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(ThreadPostTaskTest,
     InvokesWithReferencedFunctorShouldCopyAndDestroyedOnTargetThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  DestructionFunctor functor(background_thread.get(),
                             &was_invoked_on_background_thread, &event);
  DestructionFunctor& functor_ref = functor;
  background_thread->PostTask(functor_ref);
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(ThreadPostTaskTest, InvokesOnBackgroundThread) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event event;
  bool was_invoked_on_background_thread = false;
  Thread* background_thread_ptr = background_thread.get();
  background_thread->PostTask(
      [background_thread_ptr, &was_invoked_on_background_thread, &event] {
        was_invoked_on_background_thread = background_thread_ptr->IsCurrent();
        event.Set();
      });
  event.Wait(Event::kForever);

  EXPECT_TRUE(was_invoked_on_background_thread);
}

TEST(ThreadPostTaskTest, InvokesAsynchronously) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  // The first event ensures that SendSingleMessage() is not blocking this
  // thread. The second event ensures that the message is processed.
  Event event_set_by_test_thread;
  Event event_set_by_background_thread;
  background_thread->PostTask(
      [&event_set_by_test_thread, &event_set_by_background_thread] {
        WaitAndSetEvent(&event_set_by_test_thread,
                        &event_set_by_background_thread);
      });
  event_set_by_test_thread.Set();
  event_set_by_background_thread.Wait(Event::kForever);
}

TEST(ThreadPostTaskTest, InvokesInPostedOrder) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event first;
  Event second;
  Event third;
  Event fourth;

  background_thread->PostTask(
      [&first, &second] { WaitAndSetEvent(&first, &second); });
  background_thread->PostTask(
      [&second, &third] { WaitAndSetEvent(&second, &third); });
  background_thread->PostTask(
      [&third, &fourth] { WaitAndSetEvent(&third, &fourth); });

  // All tasks have been posted before the first one is unblocked.
  first.Set();
  // Only if the chain is invoked in posted order will the last event be set.
  fourth.Wait(Event::kForever);
}

TEST(ThreadPostDelayedTaskTest, InvokesAsynchronously) {
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  // The first event ensures that SendSingleMessage() is not blocking this
  // thread. The second event ensures that the message is processed.
  Event event_set_by_test_thread;
  Event event_set_by_background_thread;
  background_thread->PostDelayedTask(
      [&event_set_by_test_thread, &event_set_by_background_thread] {
        WaitAndSetEvent(&event_set_by_test_thread,
                        &event_set_by_background_thread);
      },
      TimeDelta::Millis(10));
  event_set_by_test_thread.Set();
  event_set_by_background_thread.Wait(Event::kForever);
}

TEST(ThreadPostDelayedTaskTest, InvokesInDelayOrder) {
  ScopedFakeClock clock;
  std::unique_ptr<rtc::Thread> background_thread(rtc::Thread::Create());
  background_thread->Start();

  Event first;
  Event second;
  Event third;
  Event fourth;

  background_thread->PostDelayedTask(
      [&third, &fourth] { WaitAndSetEvent(&third, &fourth); },
      TimeDelta::Millis(11));
  background_thread->PostDelayedTask(
      [&first, &second] { WaitAndSetEvent(&first, &second); },
      TimeDelta::Millis(9));
  background_thread->PostDelayedTask(
      [&second, &third] { WaitAndSetEvent(&second, &third); },
      TimeDelta::Millis(10));

  // All tasks have been posted before the first one is unblocked.
  first.Set();
  // Only if the chain is invoked in delay order will the last event be set.
  clock.AdvanceTime(TimeDelta::Millis(11));
  EXPECT_TRUE(fourth.Wait(0));
}

TEST(ThreadPostDelayedTaskTest, IsCurrentTaskQueue) {
  auto current_tq = webrtc::TaskQueueBase::Current();
  {
    std::unique_ptr<rtc::Thread> thread(rtc::Thread::Create());
    thread->WrapCurrent();
    EXPECT_EQ(webrtc::TaskQueueBase::Current(),
              static_cast<webrtc::TaskQueueBase*>(thread.get()));
    thread->UnwrapCurrent();
  }
  EXPECT_EQ(webrtc::TaskQueueBase::Current(), current_tq);
}

class ThreadFactory : public webrtc::TaskQueueFactory {
 public:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view /* name */,
                  Priority /*priority*/) const override {
    std::unique_ptr<Thread> thread = Thread::Create();
    thread->Start();
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        thread.release());
  }
};

using ::webrtc::TaskQueueTest;

INSTANTIATE_TEST_SUITE_P(RtcThread,
                         TaskQueueTest,
                         ::testing::Values(std::make_unique<ThreadFactory>));

}  // namespace
}  // namespace rtc
