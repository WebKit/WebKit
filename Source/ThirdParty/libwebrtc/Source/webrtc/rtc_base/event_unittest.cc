/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/platform_thread.h"

namespace rtc {

TEST(EventTest, InitiallySignaled) {
  Event event(false, true);
  ASSERT_TRUE(event.Wait(0));
}

TEST(EventTest, ManualReset) {
  Event event(true, false);
  ASSERT_FALSE(event.Wait(0));

  event.Set();
  ASSERT_TRUE(event.Wait(0));
  ASSERT_TRUE(event.Wait(0));

  event.Reset();
  ASSERT_FALSE(event.Wait(0));
}

TEST(EventTest, AutoReset) {
  Event event(false, false);
  ASSERT_FALSE(event.Wait(0));

  event.Set();
  ASSERT_TRUE(event.Wait(0));
  ASSERT_FALSE(event.Wait(0));
}

class SignalerThread {
public:
  SignalerThread() : thread_(&ThreadFn, this, "EventPerf") {}
  void Start(Event* writer, Event* reader) {
    writer_ = writer;
    reader_ = reader;
    thread_.Start();
  }
  void Stop() {
    stop_event_.Set();
    thread_.Stop();
  }
  static void ThreadFn(void *param) {
    auto* me = static_cast<SignalerThread*>(param);
    while(!me->stop_event_.Wait(0)) {
      me->writer_->Set();
      me->reader_->Wait(Event::kForever);
    }
  }
  Event stop_event_{false, false};
  Event* writer_;
  Event* reader_;
  PlatformThread thread_;
};

// These tests are disabled by default and only intended to be run manually.
TEST(EventTest, DISABLED_PerformanceSingleThread) {
  static const int kNumIterations = 10000000;
  Event event(false, false);
  for (int i = 0; i < kNumIterations; ++i) {
    event.Set();
    event.Wait(0);
  }
}

TEST(EventTest, DISABLED_PerformanceMultiThread) {
  static const int kNumIterations = 10000;
  Event read(false, false);
  Event write(false, false);
  SignalerThread thread;
  thread.Start(&read, &write);

  for (int i = 0; i < kNumIterations; ++i) {
    write.Set();
    read.Wait(Event::kForever);
  }
  write.Set();

  thread.Stop();
}

}  // namespace rtc
