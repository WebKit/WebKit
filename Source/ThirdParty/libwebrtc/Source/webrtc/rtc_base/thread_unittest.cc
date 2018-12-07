/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/asyncinvoker.h"
#include "rtc_base/asyncudpsocket.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/nullsocketserver.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>  // NOLINT
#endif

using namespace rtc;

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
  SocketClient(AsyncSocket* socket,
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
class MessageClient : public MessageHandler, public TestGenerator {
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
    CritScope scoped_lock(&cs_);
    flag_ = value;
    return *this;
  }
  bool get() const {
    CritScope scoped_lock(&cs_);
    return flag_;
  }

 private:
  CriticalSection cs_;
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
  FunctorD& operator=(FunctorD&&) = default;
  void operator()() {
    if (flag_)
      *flag_ = true;
  }

 private:
  AtomicBool* flag_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FunctorD);
};

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST(ThreadTest, DISABLED_Main) {
  const SocketAddress addr("127.0.0.1", 0);

  // Create the messaging client on its own thread.
  auto th1 = Thread::CreateWithSocketServer();
  Socket* socket =
      th1->socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
  MessageClient msg_client(th1.get(), socket);

  // Create the socket client on its own thread.
  auto th2 = Thread::CreateWithSocketServer();
  AsyncSocket* asocket =
      th2->socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
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
  current_thread->UnwrapCurrent();
  CustomThread* cthread = new CustomThread();
  EXPECT_TRUE(cthread->WrapCurrent());
  EXPECT_TRUE(cthread->RunningForTest());
  EXPECT_FALSE(cthread->IsOwned());
  cthread->UnwrapCurrent();
  EXPECT_FALSE(cthread->RunningForTest());
  delete cthread;
  current_thread->WrapCurrent();
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
// not deadlock.
TEST(ThreadTest, TwoThreadsInvokeNoDeadlock) {
  AutoThread thread;
  Thread* current_thread = Thread::Current();
  ASSERT_TRUE(current_thread != nullptr);

  auto other_thread = Thread::CreateWithSocketServer();
  other_thread->Start();

  struct LocalFuncs {
    static void Set(bool* out) { *out = true; }
    static void InvokeSet(Thread* thread, bool* out) {
      thread->Invoke<void>(RTC_FROM_HERE, Bind(&Set, out));
    }
  };

  bool called = false;
  other_thread->Invoke<void>(
      RTC_FROM_HERE, Bind(&LocalFuncs::InvokeSet, current_thread, &called));

  EXPECT_TRUE(called);
}

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
      CritScope lock(&crit_);
      value_ = value;
    }

    bool Get() {
      CritScope lock(&crit_);
      return value_;
    }

   private:
    CriticalSection crit_;
    bool value_ RTC_GUARDED_BY(crit_);
  };

  struct LocalFuncs {
    static void Set(LockedBool* out) { out->Set(true); }
    static void InvokeSet(Thread* thread, LockedBool* out) {
      thread->Invoke<void>(RTC_FROM_HERE, Bind(&Set, out));
    }

    // Set |out| true and call InvokeSet on |thread|.
    static void SetAndInvokeSet(LockedBool* out,
                                Thread* thread,
                                LockedBool* out_inner) {
      out->Set(true);
      InvokeSet(thread, out_inner);
    }

    // Asynchronously invoke SetAndInvokeSet on |thread1| and wait until
    // |thread1| starts the call.
    static void AsyncInvokeSetAndWait(AsyncInvoker* invoker,
                                      Thread* thread1,
                                      Thread* thread2,
                                      LockedBool* out) {
      CriticalSection crit;
      LockedBool async_invoked(false);

      invoker->AsyncInvoke<void>(
          RTC_FROM_HERE, thread1,
          Bind(&SetAndInvokeSet, &async_invoked, thread2, out));

      EXPECT_TRUE_WAIT(async_invoked.Get(), 2000);
    }
  };

  AsyncInvoker invoker;
  LockedBool thread_a_called(false);

  // Start the sequence A --(invoke)--> B --(async invoke)--> C --(invoke)--> A.
  // Thread B returns when C receives the call and C should be blocked until A
  // starts to process messages.
  thread_b->Invoke<void>(RTC_FROM_HERE,
                         Bind(&LocalFuncs::AsyncInvokeSetAndWait, &invoker,
                              thread_c.get(), thread_a, &thread_a_called));
  EXPECT_FALSE(thread_a_called.Get());

  EXPECT_TRUE_WAIT(thread_a_called.Get(), 2000);
}

// Set the name on a thread when the underlying QueueDestroyed signal is
// triggered. This causes an error if the object is already partially
// destroyed.
class SetNameOnSignalQueueDestroyedTester : public sigslot::has_slots<> {
 public:
  SetNameOnSignalQueueDestroyedTester(Thread* thread) : thread_(thread) {
    thread->SignalQueueDestroyed.connect(
        this, &SetNameOnSignalQueueDestroyedTester::OnQueueDestroyed);
  }

  void OnQueueDestroyed() {
    // Makes sure that if we access the Thread while it's being destroyed, that
    // it doesn't cause a problem because the vtable has been modified.
    thread_->SetName("foo", nullptr);
  }

 private:
  Thread* thread_;
};

TEST(ThreadTest, SetNameOnSignalQueueDestroyed) {
  auto thread1 = Thread::CreateWithSocketServer();
  SetNameOnSignalQueueDestroyedTester tester1(thread1.get());
  thread1.reset();

  Thread* thread2 = new AutoThread();
  SetNameOnSignalQueueDestroyedTester tester2(thread2);
  delete thread2;
}

class AsyncInvokeTest : public testing::Test {
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
  AsyncInvokeTest() : int_value_(0), expected_thread_(nullptr) {}

  int int_value_;
  Thread* expected_thread_;
};

TEST_F(AsyncInvokeTest, FireAndForget) {
  AsyncInvoker invoker;
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functor.
  AtomicBool called;
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, thread.get(), FunctorB(&called));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
  thread->Stop();
}

TEST_F(AsyncInvokeTest, NonCopyableFunctor) {
  AsyncInvoker invoker;
  // Create and start the thread.
  auto thread = Thread::CreateWithSocketServer();
  thread->Start();
  // Try calling functor.
  AtomicBool called;
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, thread.get(), FunctorD(&called));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
  thread->Stop();
}

TEST_F(AsyncInvokeTest, KillInvokerDuringExecute) {
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
    AsyncInvoker invoker;
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
// *again*, for the thread on which the AsyncInvoker is currently being
// destroyed. This shouldn't deadlock or crash; this second invocation should
// just be ignored.
TEST_F(AsyncInvokeTest, KillInvokerDuringExecuteWithReentrantInvoke) {
  Event functor_started;
  // Flag used to verify that the recursively invoked task never actually runs.
  bool reentrant_functor_run = false;

  Thread* main = Thread::Current();
  Thread thread;
  thread.Start();
  {
    AsyncInvoker invoker;
    auto reentrant_functor = [&reentrant_functor_run] {
      reentrant_functor_run = true;
    };
    auto functor = [&functor_started, &invoker, main, reentrant_functor] {
      functor_started.Set();
      Thread::Current()->SleepMs(kWaitTimeout);
      invoker.AsyncInvoke<void>(RTC_FROM_HERE, main, reentrant_functor);
    };
    // This queues a task on |thread| to sleep for |kWaitTimeout| then queue a
    // task on |main|. But this second queued task should never run, since the
    // destructor will be entered before it's even invoked.
    invoker.AsyncInvoke<void>(RTC_FROM_HERE, &thread, functor);
    functor_started.Wait(Event::kForever);
  }
  EXPECT_FALSE(reentrant_functor_run);
}

TEST_F(AsyncInvokeTest, Flush) {
  AsyncInvoker invoker;
  AtomicBool flag1;
  AtomicBool flag2;
  // Queue two async calls to the current thread.
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, Thread::Current(), FunctorB(&flag1));
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, Thread::Current(), FunctorB(&flag2));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1.get());
  EXPECT_FALSE(flag2.get());
  // Force them to run now.
  invoker.Flush(Thread::Current());
  EXPECT_TRUE(flag1.get());
  EXPECT_TRUE(flag2.get());
}

TEST_F(AsyncInvokeTest, FlushWithIds) {
  AsyncInvoker invoker;
  AtomicBool flag1;
  AtomicBool flag2;
  // Queue two async calls to the current thread, one with a message id.
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, Thread::Current(), FunctorB(&flag1),
                            5);
  invoker.AsyncInvoke<void>(RTC_FROM_HERE, Thread::Current(), FunctorB(&flag2));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1.get());
  EXPECT_FALSE(flag2.get());
  // Execute pending calls with id == 5.
  invoker.Flush(Thread::Current(), 5);
  EXPECT_TRUE(flag1.get());
  EXPECT_FALSE(flag2.get());
  flag1 = false;
  // Execute all pending calls. The id == 5 call should not execute again.
  invoker.Flush(Thread::Current());
  EXPECT_FALSE(flag1.get());
  EXPECT_TRUE(flag2.get());
}

class GuardedAsyncInvokeTest : public testing::Test {
 public:
  void IntCallback(int value) {
    EXPECT_EQ(expected_thread_, Thread::Current());
    int_value_ = value;
  }
  void SetExpectedThreadForIntCallback(Thread* thread) {
    expected_thread_ = thread;
  }

 protected:
  const static int kWaitTimeout = 1000;
  GuardedAsyncInvokeTest() : int_value_(0), expected_thread_(nullptr) {}

  int int_value_;
  Thread* expected_thread_;
};

// Functor for creating an invoker.
struct CreateInvoker {
  CreateInvoker(std::unique_ptr<GuardedAsyncInvoker>* invoker)
      : invoker_(invoker) {}
  void operator()() { invoker_->reset(new GuardedAsyncInvoker()); }
  std::unique_ptr<GuardedAsyncInvoker>* invoker_;
};

// Test that we can call AsyncInvoke<void>() after the thread died.
TEST_F(GuardedAsyncInvokeTest, KillThreadFireAndForget) {
  // Create and start the thread.
  std::unique_ptr<Thread> thread(Thread::Create());
  thread->Start();
  std::unique_ptr<GuardedAsyncInvoker> invoker;
  // Create the invoker on |thread|.
  thread->Invoke<void>(RTC_FROM_HERE, CreateInvoker(&invoker));
  // Kill |thread|.
  thread = nullptr;
  // Try calling functor.
  AtomicBool called;
  EXPECT_FALSE(invoker->AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&called)));
  // With thread gone, nothing should happen.
  WAIT(called.get(), kWaitTimeout);
  EXPECT_FALSE(called.get());
}

// The remaining tests check that GuardedAsyncInvoker behaves as AsyncInvoker
// when Thread is still alive.
TEST_F(GuardedAsyncInvokeTest, FireAndForget) {
  GuardedAsyncInvoker invoker;
  // Try calling functor.
  AtomicBool called;
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&called)));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
}

TEST_F(GuardedAsyncInvokeTest, NonCopyableFunctor) {
  GuardedAsyncInvoker invoker;
  // Try calling functor.
  AtomicBool called;
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorD(&called)));
  EXPECT_TRUE_WAIT(called.get(), kWaitTimeout);
}

TEST_F(GuardedAsyncInvokeTest, Flush) {
  GuardedAsyncInvoker invoker;
  AtomicBool flag1;
  AtomicBool flag2;
  // Queue two async calls to the current thread.
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&flag1)));
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&flag2)));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1.get());
  EXPECT_FALSE(flag2.get());
  // Force them to run now.
  EXPECT_TRUE(invoker.Flush());
  EXPECT_TRUE(flag1.get());
  EXPECT_TRUE(flag2.get());
}

TEST_F(GuardedAsyncInvokeTest, FlushWithIds) {
  GuardedAsyncInvoker invoker;
  AtomicBool flag1;
  AtomicBool flag2;
  // Queue two async calls to the current thread, one with a message id.
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&flag1), 5));
  EXPECT_TRUE(invoker.AsyncInvoke<void>(RTC_FROM_HERE, FunctorB(&flag2)));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1.get());
  EXPECT_FALSE(flag2.get());
  // Execute pending calls with id == 5.
  EXPECT_TRUE(invoker.Flush(5));
  EXPECT_TRUE(flag1.get());
  EXPECT_FALSE(flag2.get());
  flag1 = false;
  // Execute all pending calls. The id == 5 call should not execute again.
  EXPECT_TRUE(invoker.Flush());
  EXPECT_FALSE(flag1.get());
  EXPECT_TRUE(flag2.get());
}
