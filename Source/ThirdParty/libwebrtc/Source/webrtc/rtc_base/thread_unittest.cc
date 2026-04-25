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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/internal/default_socket_server.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"
#include "test/wait_until.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>  // NOLINT

#endif

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::NotNull;

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

// Receives messages and sends on a socket.
class MessageClient : public TestGenerator {
 public:
  MessageClient(Thread* pth, Socket* socket) : socket_(socket) {}

  ~MessageClient() { delete socket_; }

  void OnValue(int value) {
    int result = Next(value);
    EXPECT_GE(socket_->Send(&result, sizeof(result)), 0);
  }

 private:
  Socket* socket_;
};

// Receives on a socket and sends by posting messages.
class SocketClient : public TestGenerator {
 public:
  SocketClient(SocketFactory* socket_factory,
               const SocketAddress& addr,
               Thread* post_thread,
               MessageClient* phandler)
      : socket_(AsyncUDPSocket::Create(CreateTestEnvironment(),
                                       addr,
                                       *socket_factory)),
        post_thread_(post_thread),
        post_handler_(phandler) {
    socket_->RegisterReceivedPacketCallback(
        [&](AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
          OnPacket(socket, packet);
        });
  }

  ~SocketClient() = default;

  SocketAddress address() const { return socket_->GetLocalAddress(); }

  void OnPacket(AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
    EXPECT_EQ(packet.payload().size(), sizeof(uint32_t));
    uint32_t prev =
        reinterpret_cast<const uint32_t*>(packet.payload().data())[0];
    uint32_t result = Next(prev);

    post_thread_->PostDelayedTask([post_handler_ = post_handler_,
                                   result] { post_handler_->OnValue(result); },
                                  TimeDelta::Millis(200));
  }

 private:
  std::unique_ptr<AsyncUDPSocket> socket_;
  Thread* post_thread_;
  MessageClient* post_handler_;
};

class CustomThread : public Thread {
 public:
  CustomThread()
      : Thread(std::unique_ptr<SocketServer>(new NullSocketServer())) {}
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

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST(ThreadTest, DISABLED_Main) {
  AutoThread main_thread;
  const SocketAddress addr("127.0.0.1", 0);

  // Create the messaging client on its own thread.
  auto th1 = Thread::CreateWithSocketServer();
  Socket* socket = th1->socketserver()->CreateSocket(addr.family(), SOCK_DGRAM);
  MessageClient msg_client(th1.get(), socket);

  // Create the socket client on its own thread.
  auto th2 = Thread::CreateWithSocketServer();
  SocketClient sock_client(th2->socketserver(), addr, th1.get(), &msg_client);

  socket->Connect(sock_client.address());

  th1->Start();
  th2->Start();

  // Get the messages started.
  th1->PostDelayedTask([&msg_client] { msg_client.OnValue(1); },
                       TimeDelta::Millis(100));

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

// Tests that the implementation behind
// `RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS` doesn't cause problems (crash or
// DCHECK) when used on a thread that does not have an attached current
// `Thread*` instance.
TEST(ThreadTest, DisallowBlockingCallsNoThread) {
  ASSERT_THAT(Thread::Current(), IsNull());
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
}

TEST(ThreadTest, DisallowBlockingCallsWithThread) {
  AutoThread current;
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
}

TEST(ThreadTest, CountBlockingCalls) {
  AutoThread current;

  // When the test runs, this will print out:
  //   (thread_unittest.cc:262): Blocking TestBody: total=2 (actual=1, could=1)
  RTC_LOG_THREAD_BLOCK_COUNT();
#if RTC_DCHECK_IS_ON
  Thread::ScopedCountBlockingCalls blocked_calls(
      [&](uint32_t actual_block, uint32_t could_block, TimeDelta duration) {
        EXPECT_EQ(1u, actual_block);
        EXPECT_EQ(1u, could_block);
      });

  EXPECT_EQ(0u, blocked_calls.GetBlockingCallCount());
  EXPECT_EQ(0u, blocked_calls.GetCouldBeBlockingCallCount());
  EXPECT_EQ(0u, blocked_calls.GetTotalBlockedCallCount());

  // Test invoking on the current thread. This should not count as an 'actual'
  // invoke, but should still count as an invoke that could block since we
  // that the call to `BlockingCall` serves a purpose in some configurations
  // (and should not be used a general way to call methods on the same thread).
  current.BlockingCall([]() {});
  EXPECT_EQ(0u, blocked_calls.GetBlockingCallCount());
  EXPECT_EQ(1u, blocked_calls.GetCouldBeBlockingCallCount());
  EXPECT_EQ(1u, blocked_calls.GetTotalBlockedCallCount());

  // Create a new thread to invoke on.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  EXPECT_EQ(42, thread->BlockingCall([]() { return 42; }));
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
  AutoThread current;
  bool was_called_back = false;
  {
    Thread::ScopedCountBlockingCalls blocked_calls(
        [&](uint32_t actual_block, uint32_t could_block, TimeDelta duration) {
          was_called_back = true;
        });
    current.BlockingCall([]() {});
  }
  EXPECT_TRUE(was_called_back);
}

TEST(ThreadTest, CountBlockingCallsSkipCallback) {
  AutoThread current;
  bool was_called_back = false;
  {
    Thread::ScopedCountBlockingCalls blocked_calls(
        [&](uint32_t actual_block, uint32_t could_block, TimeDelta duration) {
          was_called_back = true;
        });
    // Changed `blocked_calls` to not issue the callback if there are 1 or
    // fewer blocking calls (i.e. we set the minimum required number to 2).
    blocked_calls.set_minimum_call_count_for_callback(2);
    current.BlockingCall([]() {});
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
  AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();

  thread1->PostTask(
      [&]() { EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread2.get())); });
  main_thread.ProcessMessages(100);
}

TEST(ThreadTest, InvokeAllowedWhenThreadsAdded) {
  AutoThread main_thread;
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
  AutoThread main_thread;
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
  AutoThread main_thread;
  // Create and start the thread.
  auto thread1 = Thread::CreateWithSocketServer();
  auto thread2 = Thread::CreateWithSocketServer();

  thread1->PostTask(
      [&]() { EXPECT_TRUE(thread1->IsInvokeToThreadAllowed(thread2.get())); });
  main_thread.ProcessMessages(100);
}

TEST(ThreadTest, BlockingCall) {
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functors.
  EXPECT_EQ(42, thread->BlockingCall([] { return 42; }));
  bool called = false;
  thread->BlockingCall([&] { called = true; });
  EXPECT_TRUE(called);

  // Try calling bare functions.
  struct LocalFuncs {
    static int Func1() { return 999; }
    static void Func2() {}
  };
  EXPECT_EQ(999, thread->BlockingCall(&LocalFuncs::Func1));
  thread->BlockingCall(&LocalFuncs::Func2);
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
  other_thread->BlockingCall([main_thread] {
    RTC_EXPECT_DEATH(main_thread->BlockingCall([] {}), "loop");
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

  second->BlockingCall([&] {
    third->BlockingCall(
        [&] { RTC_EXPECT_DEATH(first->BlockingCall([] {}), "loop"); });
  });
}

TEST(ThreadTest, DisallowBlockingCallDeathTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  AutoThread thread;
  ASSERT_THAT(Thread::Current(), NotNull());
  auto other_thread = Thread::CreateWithSocketServer();
  other_thread->Start();
  {
    RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
    RTC_EXPECT_DEATH(other_thread->BlockingCall([] {}),
                     "blocking_calls_allowed_");
  }
}
#endif

// Verifies that if thread A invokes a call on thread B and thread C is trying
// to invoke A at the same time, thread A does not handle C's invoke while
// invoking B.
TEST(ThreadTest, ThreeThreadsBlockingCall) {
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
      MutexLock lock(&mutex_);
      value_ = value;
    }

    bool Get() {
      MutexLock lock(&mutex_);
      return value_;
    }

   private:
    Mutex mutex_;
    bool value_ RTC_GUARDED_BY(mutex_);
  };

  struct LocalFuncs {
    static void Set(LockedBool* out) { out->Set(true); }
    static void InvokeSet(Thread* thread, LockedBool* out) {
      thread->BlockingCall([out] { Set(out); });
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
    static void AsyncInvokeSetAndWait(Thread* thread1,
                                      Thread* thread2,
                                      LockedBool* out) {
      LockedBool async_invoked(false);

      thread1->PostTask([&async_invoked, thread2, out] {
        SetAndInvokeSet(&async_invoked, thread2, out);
      });

      EXPECT_THAT(
          WaitUntil([&] { return async_invoked.Get(); }, ::testing::IsTrue()),
          IsRtcOk());
    }
  };

  LockedBool thread_a_called(false);

  // Start the sequence A --(invoke)--> B --(async invoke)--> C --(invoke)--> A.
  // Thread B returns when C receives the call and C should be blocked until A
  // starts to process messages.
  Thread* thread_c_ptr = thread_c.get();
  thread_b->BlockingCall([thread_c_ptr, thread_a, &thread_a_called] {
    LocalFuncs::AsyncInvokeSetAndWait(thread_c_ptr, thread_a, &thread_a_called);
  });
  EXPECT_FALSE(thread_a_called.Get());

  EXPECT_THAT(
      WaitUntil([&] { return thread_a_called.Get(); }, ::testing::IsTrue()),
      IsRtcOk());
}

void DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(FakeClock& clock,
                                                           Thread& q) {
  std::vector<int> run_order;

  Event done;
  int64_t now = TimeMillis();
  q.PostDelayedTask([&] { run_order.push_back(3); }, TimeDelta::Millis(3));
  q.PostDelayedTask([&] { run_order.push_back(0); }, TimeDelta::Millis(1));
  q.PostDelayedTask([&] { run_order.push_back(1); }, TimeDelta::Millis(2));
  q.PostDelayedTask([&] { run_order.push_back(4); }, TimeDelta::Millis(3));
  q.PostDelayedTask([&] { run_order.push_back(2); }, TimeDelta::Millis(2));
  q.PostDelayedTask([&] { done.Set(); }, TimeDelta::Millis(4));
  // Validate time was frozen while tasks were posted.
  RTC_DCHECK_EQ(TimeMillis(), now);

  // Change time to make all tasks ready to run and wait for them.
  clock.AdvanceTime(TimeDelta::Millis(4));
  ASSERT_TRUE(done.Wait(TimeDelta::Seconds(1)));

  EXPECT_THAT(run_order, ElementsAre(0, 1, 2, 3, 4));
}

TEST(ThreadTest, DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder) {
  ScopedBaseFakeClock clock;
  Thread q(CreateDefaultSocketServer(), true);
  q.Start();
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(clock, q);

  NullSocketServer nullss;
  Thread q_nullss(&nullss, true);
  q_nullss.Start();
  DelayedPostsWithIdenticalTimesAreProcessedInFifoOrder(clock, q_nullss);
}

// Ensure that ProcessAllMessageQueues does its essential function; process
// all messages (both delayed and non delayed) up until the current time, on
// all registered message queues.
TEST(ThreadManager, ProcessAllMessageQueues) {
  AutoThread main_thread;
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
  background_thread->Start();

  Event event;
  background_thread->PostTask([&event] { event.Set(); });
  event.Wait(Event::kForever);
}

TEST(ThreadPostTaskTest, InvokesWithCopiedFunctor) {
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
  background_thread->Start();

  // The first event ensures that SendSingleMessage() is not blocking this
  // thread. The second event ensures that the message is processed.
  Event event_set_by_test_thread;
  Event event_set_by_background_thread;
  background_thread->PostTask([&event_set_by_test_thread,
                               &event_set_by_background_thread] {
    WaitAndSetEvent(&event_set_by_test_thread, &event_set_by_background_thread);
  });
  event_set_by_test_thread.Set();
  event_set_by_background_thread.Wait(Event::kForever);
}

TEST(ThreadPostTaskTest, InvokesInPostedOrder) {
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  std::unique_ptr<Thread> background_thread(Thread::Create());
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
  EXPECT_TRUE(fourth.Wait(TimeDelta::Zero()));
}

TEST(ThreadPostDelayedTaskTest, IsCurrentTaskQueue) {
  auto current_tq = TaskQueueBase::Current();
  {
    std::unique_ptr<Thread> thread(Thread::Create());
    thread->WrapCurrent();
    EXPECT_EQ(TaskQueueBase::Current(),
              static_cast<TaskQueueBase*>(thread.get()));
    thread->UnwrapCurrent();
  }
  EXPECT_EQ(TaskQueueBase::Current(), current_tq);
}

class ThreadFactory : public TaskQueueFactory {
 public:
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view /* name */,
      Priority /*priority*/) const override {
    std::unique_ptr<Thread> thread = Thread::Create();
    thread->Start();
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(thread.release());
  }
};

std::unique_ptr<TaskQueueFactory> CreateDefaultThreadFactory(
    const FieldTrialsView*) {
  return std::make_unique<ThreadFactory>();
}


INSTANTIATE_TEST_SUITE_P(RtcThread,
                         TaskQueueTest,
                         ::testing::Values(CreateDefaultThreadFactory));

}  // namespace
}  // namespace webrtc
