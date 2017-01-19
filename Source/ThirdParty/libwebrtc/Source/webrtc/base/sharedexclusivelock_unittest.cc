/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/event.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sharedexclusivelock.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

static const uint32_t kMsgRead = 0;
static const uint32_t kMsgWrite = 0;
static const int kNoWaitThresholdInMs = 10;
static const int kWaitThresholdInMs = 80;
static const int kProcessTimeInMs = 100;
static const int kProcessTimeoutInMs = 5000;

class SharedExclusiveTask : public MessageHandler {
 public:
  SharedExclusiveTask(SharedExclusiveLock* shared_exclusive_lock,
                      int* value,
                      Event* done)
      : shared_exclusive_lock_(shared_exclusive_lock),
        value_(value),
        done_(done) {
    worker_thread_.reset(new Thread());
    worker_thread_->Start();
  }

 protected:
  std::unique_ptr<Thread> worker_thread_;
  SharedExclusiveLock* shared_exclusive_lock_;
  int* value_;
  Event* done_;
};

class ReadTask : public SharedExclusiveTask {
 public:
  ReadTask(SharedExclusiveLock* shared_exclusive_lock, int* value, Event* done)
      : SharedExclusiveTask(shared_exclusive_lock, value, done) {
  }

  void PostRead(int* value) {
    worker_thread_->Post(RTC_FROM_HERE, this, kMsgRead,
                         new TypedMessageData<int*>(value));
  }

 private:
  virtual void OnMessage(Message* message) {
    ASSERT(rtc::Thread::Current() == worker_thread_.get());
    ASSERT(message != NULL);
    ASSERT(message->message_id == kMsgRead);

    TypedMessageData<int*>* message_data =
        static_cast<TypedMessageData<int*>*>(message->pdata);

    {
      SharedScope ss(shared_exclusive_lock_);
      *message_data->data() = *value_;
      done_->Set();
    }
    delete message->pdata;
    message->pdata = NULL;
  }
};

class WriteTask : public SharedExclusiveTask {
 public:
  WriteTask(SharedExclusiveLock* shared_exclusive_lock, int* value, Event* done)
      : SharedExclusiveTask(shared_exclusive_lock, value, done) {
  }

  void PostWrite(int value) {
    worker_thread_->Post(RTC_FROM_HERE, this, kMsgWrite,
                         new TypedMessageData<int>(value));
  }

 private:
  virtual void OnMessage(Message* message) {
    ASSERT(rtc::Thread::Current() == worker_thread_.get());
    ASSERT(message != NULL);
    ASSERT(message->message_id == kMsgWrite);

    TypedMessageData<int>* message_data =
        static_cast<TypedMessageData<int>*>(message->pdata);

    {
      ExclusiveScope es(shared_exclusive_lock_);
      *value_ = message_data->data();
      done_->Set();
    }
    delete message->pdata;
    message->pdata = NULL;
  }
};

// Unit test for SharedExclusiveLock.
class SharedExclusiveLockTest
    : public testing::Test {
 public:
  SharedExclusiveLockTest() : value_(0) {
  }

  virtual void SetUp() {
    shared_exclusive_lock_.reset(new SharedExclusiveLock());
  }

 protected:
  std::unique_ptr<SharedExclusiveLock> shared_exclusive_lock_;
  int value_;
};

TEST_F(SharedExclusiveLockTest, TestSharedShared) {
  int value0, value1;
  Event done0(false, false), done1(false, false);
  ReadTask reader0(shared_exclusive_lock_.get(), &value_, &done0);
  ReadTask reader1(shared_exclusive_lock_.get(), &value_, &done1);

  // Test shared locks can be shared without waiting.
  {
    SharedScope ss(shared_exclusive_lock_.get());
    value_ = 1;
    reader0.PostRead(&value0);
    reader1.PostRead(&value1);

    EXPECT_TRUE(done0.Wait(kProcessTimeoutInMs));
    EXPECT_TRUE(done1.Wait(kProcessTimeoutInMs));
    EXPECT_EQ(1, value0);
    EXPECT_EQ(1, value1);
  }
}

TEST_F(SharedExclusiveLockTest, TestSharedExclusive) {
  Event done(false, false);
  WriteTask writer(shared_exclusive_lock_.get(), &value_, &done);

  // Test exclusive lock needs to wait for shared lock.
  {
    SharedScope ss(shared_exclusive_lock_.get());
    value_ = 1;
    writer.PostWrite(2);
    EXPECT_FALSE(done.Wait(kProcessTimeInMs));
  }
  EXPECT_TRUE(done.Wait(kProcessTimeoutInMs));
  EXPECT_EQ(2, value_);
}

TEST_F(SharedExclusiveLockTest, TestExclusiveShared) {
  int value;
  Event done(false, false);
  ReadTask reader(shared_exclusive_lock_.get(), &value_, &done);

  // Test shared lock needs to wait for exclusive lock.
  {
    ExclusiveScope es(shared_exclusive_lock_.get());
    value_ = 1;
    reader.PostRead(&value);
    EXPECT_FALSE(done.Wait(kProcessTimeInMs));
    value_ = 2;
  }

  EXPECT_TRUE(done.Wait(kProcessTimeoutInMs));
  EXPECT_EQ(2, value);
}

TEST_F(SharedExclusiveLockTest, TestExclusiveExclusive) {
  Event done(false, false);
  WriteTask writer(shared_exclusive_lock_.get(), &value_, &done);

  // Test exclusive lock needs to wait for exclusive lock.
  {
    ExclusiveScope es(shared_exclusive_lock_.get());
    // Start the writer task only after holding the lock, to ensure it need
    value_ = 1;
    writer.PostWrite(2);
    EXPECT_FALSE(done.Wait(kProcessTimeInMs));
    EXPECT_EQ(1, value_);
  }

  EXPECT_TRUE(done.Wait(kProcessTimeoutInMs));
  EXPECT_EQ(2, value_);
}

}  // namespace rtc
