/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/signal_thread.h"

#include <memory>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/gunit.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/gtest.h"

namespace rtc {
namespace {

// 10 seconds.
static const int kTimeout = 10000;

class SignalThreadTest : public ::testing::Test, public sigslot::has_slots<> {
 public:
  class SlowSignalThread : public SignalThread {
   public:
    SlowSignalThread(SignalThreadTest* harness) : harness_(harness) {}

    ~SlowSignalThread() override {
      EXPECT_EQ(harness_->main_thread_, Thread::Current());
      ++harness_->thread_deleted_;
    }

    const SignalThreadTest* harness() { return harness_; }

   protected:
    void OnWorkStart() override {
      ASSERT_TRUE(harness_ != nullptr);
      ++harness_->thread_started_;
      EXPECT_EQ(harness_->main_thread_, Thread::Current());
      EXPECT_FALSE(worker()->RunningForTest());  // not started yet
    }

    void OnWorkStop() override {
      ++harness_->thread_stopped_;
      EXPECT_EQ(harness_->main_thread_, Thread::Current());
      EXPECT_TRUE(worker()->RunningForTest());  // not stopped yet
    }

    void OnWorkDone() override {
      ++harness_->thread_done_;
      EXPECT_EQ(harness_->main_thread_, Thread::Current());
      EXPECT_TRUE(worker()->RunningForTest());  // not stopped yet
    }

    void DoWork() override {
      EXPECT_NE(harness_->main_thread_, Thread::Current());
      EXPECT_EQ(worker(), Thread::Current());
      Thread::Current()->socketserver()->Wait(250, false);
    }

   private:
    SignalThreadTest* harness_;
    RTC_DISALLOW_COPY_AND_ASSIGN(SlowSignalThread);
  };

  void OnWorkComplete(rtc::SignalThread* thread) {
    SlowSignalThread* t = static_cast<SlowSignalThread*>(thread);
    EXPECT_EQ(t->harness(), this);
    EXPECT_EQ(main_thread_, Thread::Current());

    ++thread_completed_;
    if (!called_release_) {
      thread->Release();
    }
  }

  void SetUp() override {
    main_thread_ = Thread::Current();
    thread_ = new SlowSignalThread(this);
    thread_->SignalWorkDone.connect(this, &SignalThreadTest::OnWorkComplete);
    called_release_ = false;
    thread_started_ = 0;
    thread_done_ = 0;
    thread_completed_ = 0;
    thread_stopped_ = 0;
    thread_deleted_ = 0;
  }

  void ExpectState(int started,
                   int done,
                   int completed,
                   int stopped,
                   int deleted) {
    EXPECT_EQ(started, thread_started_);
    EXPECT_EQ(done, thread_done_);
    EXPECT_EQ(completed, thread_completed_);
    EXPECT_EQ(stopped, thread_stopped_);
    EXPECT_EQ(deleted, thread_deleted_);
  }

  void ExpectStateWait(int started,
                       int done,
                       int completed,
                       int stopped,
                       int deleted,
                       int timeout) {
    EXPECT_EQ_WAIT(started, thread_started_, timeout);
    EXPECT_EQ_WAIT(done, thread_done_, timeout);
    EXPECT_EQ_WAIT(completed, thread_completed_, timeout);
    EXPECT_EQ_WAIT(stopped, thread_stopped_, timeout);
    EXPECT_EQ_WAIT(deleted, thread_deleted_, timeout);
  }

  Thread* main_thread_;
  SlowSignalThread* thread_;
  bool called_release_;

  int thread_started_;
  int thread_done_;
  int thread_completed_;
  int thread_stopped_;
  int thread_deleted_;
};

class OwnerThread : public Thread, public sigslot::has_slots<> {
 public:
  explicit OwnerThread(SignalThreadTest* harness)
      : Thread(std::make_unique<NullSocketServer>()),
        harness_(harness),
        has_run_(false) {}

  ~OwnerThread() override { Stop(); }

  void Run() override {
    SignalThreadTest::SlowSignalThread* signal_thread =
        new SignalThreadTest::SlowSignalThread(harness_);
    signal_thread->SignalWorkDone.connect(this, &OwnerThread::OnWorkDone);
    signal_thread->Start();
    Thread::Current()->socketserver()->Wait(100, false);
    signal_thread->Release();
    // Delete |signal_thread|.
    signal_thread->Destroy(true);
    {
      rtc::CritScope cs(&crit_);
      has_run_ = true;
    }
  }

  bool has_run() {
    rtc::CritScope cs(&crit_);
    return has_run_;
  }
  void OnWorkDone(SignalThread* /*signal_thread*/) {
    FAIL() << " This shouldn't get called.";
  }

 private:
  rtc::CriticalSection crit_;
  SignalThreadTest* harness_;
  bool has_run_ RTC_GUARDED_BY(crit_);
  RTC_DISALLOW_COPY_AND_ASSIGN(OwnerThread);
};

// Test for when the main thread goes away while the
// signal thread is still working.  This may happen
// when shutting down the process.
TEST_F(SignalThreadTest, OwnerThreadGoesAway) {
  // We don't use |thread_| for this test, so destroy it.
  thread_->Destroy(true);

  {
    std::unique_ptr<OwnerThread> owner(new OwnerThread(this));
    main_thread_ = owner.get();
    owner->Start();
    while (!owner->has_run()) {
      Thread::Current()->socketserver()->Wait(10, false);
    }
  }
  // At this point the main thread has gone away.
  // Give the SignalThread a little time to do its callback,
  // which will crash if the signal thread doesn't handle
  // this situation well.
  Thread::Current()->socketserver()->Wait(500, false);
}

TEST_F(SignalThreadTest, ThreadFinishes) {
  thread_->Start();
  ExpectState(1, 0, 0, 0, 0);
  ExpectStateWait(1, 1, 1, 0, 1, kTimeout);
}

TEST_F(SignalThreadTest, ReleasedThreadFinishes) {
  thread_->Start();
  ExpectState(1, 0, 0, 0, 0);
  thread_->Release();
  called_release_ = true;
  ExpectState(1, 0, 0, 0, 0);
  ExpectStateWait(1, 1, 1, 0, 1, kTimeout);
}

TEST_F(SignalThreadTest, DestroyedThreadCleansUp) {
  thread_->Start();
  ExpectState(1, 0, 0, 0, 0);
  thread_->Destroy(true);
  ExpectState(1, 0, 0, 1, 1);
  Thread::Current()->ProcessMessages(0);
  ExpectState(1, 0, 0, 1, 1);
}

TEST_F(SignalThreadTest, DeferredDestroyedThreadCleansUp) {
  thread_->Start();
  ExpectState(1, 0, 0, 0, 0);
  thread_->Destroy(false);
  ExpectState(1, 0, 0, 1, 0);
  ExpectStateWait(1, 1, 0, 1, 1, kTimeout);
}

}  // namespace
}  // namespace rtc
