/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/nullsocketserver.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"
#include "test/testsupport/fileutils.h"

namespace rtc {

template <typename Base>
class LogSinkImpl
    : public LogSink,
      public Base {
 public:
  LogSinkImpl() {}

  template<typename P>
  explicit LogSinkImpl(P* p) : Base(p) {}

 private:
  void OnLogMessage(const std::string& message) override {
    static_cast<Base*>(this)->WriteAll(
        message.data(), message.size(), nullptr, nullptr);
  }
};

// Test basic logging operation. We should get the INFO log but not the VERBOSE.
// We should restore the correct global state at the end.
TEST(LogTest, SingleStream) {
  int sev = LogMessage::GetLogToStream(nullptr);

  std::string str;
  LogSinkImpl<StringStream> stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream));

  RTC_LOG(LS_INFO) << "INFO";
  RTC_LOG(LS_VERBOSE) << "VERBOSE";
  EXPECT_NE(std::string::npos, str.find("INFO"));
  EXPECT_EQ(std::string::npos, str.find("VERBOSE"));

  LogMessage::RemoveLogToStream(&stream);
  EXPECT_EQ(LS_NONE, LogMessage::GetLogToStream(&stream));

  EXPECT_EQ(sev, LogMessage::GetLogToStream(nullptr));
}

// Test using multiple log streams. The INFO stream should get the INFO message,
// the VERBOSE stream should get the INFO and the VERBOSE.
// We should restore the correct global state at the end.
TEST(LogTest, MultipleStreams) {
  int sev = LogMessage::GetLogToStream(nullptr);

  std::string str1, str2;
  LogSinkImpl<StringStream> stream1(&str1), stream2(&str2);
  LogMessage::AddLogToStream(&stream1, LS_INFO);
  LogMessage::AddLogToStream(&stream2, LS_VERBOSE);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream1));
  EXPECT_EQ(LS_VERBOSE, LogMessage::GetLogToStream(&stream2));

  RTC_LOG(LS_INFO) << "INFO";
  RTC_LOG(LS_VERBOSE) << "VERBOSE";

  EXPECT_NE(std::string::npos, str1.find("INFO"));
  EXPECT_EQ(std::string::npos, str1.find("VERBOSE"));
  EXPECT_NE(std::string::npos, str2.find("INFO"));
  EXPECT_NE(std::string::npos, str2.find("VERBOSE"));

  LogMessage::RemoveLogToStream(&stream2);
  LogMessage::RemoveLogToStream(&stream1);
  EXPECT_EQ(LS_NONE, LogMessage::GetLogToStream(&stream2));
  EXPECT_EQ(LS_NONE, LogMessage::GetLogToStream(&stream1));

  EXPECT_EQ(sev, LogMessage::GetLogToStream(nullptr));
}

// Ensure we don't crash when adding/removing streams while threads are going.
// We should restore the correct global state at the end.
class LogThread : public Thread {
 public:
  LogThread() : Thread(std::unique_ptr<SocketServer>(new NullSocketServer())) {}

  ~LogThread() override {
    Stop();
  }

 private:
  void Run() override {
    // LS_SENSITIVE to avoid cluttering up any real logging going on
    RTC_LOG(LS_SENSITIVE) << "RTC_LOG";
  }
};

TEST(LogTest, MultipleThreads) {
  int sev = LogMessage::GetLogToStream(nullptr);

  LogThread thread1, thread2, thread3;
  thread1.Start();
  thread2.Start();
  thread3.Start();

  LogSinkImpl<NullStream> stream1, stream2, stream3;
  for (int i = 0; i < 1000; ++i) {
    LogMessage::AddLogToStream(&stream1, LS_INFO);
    LogMessage::AddLogToStream(&stream2, LS_VERBOSE);
    LogMessage::AddLogToStream(&stream3, LS_SENSITIVE);
    LogMessage::RemoveLogToStream(&stream1);
    LogMessage::RemoveLogToStream(&stream2);
    LogMessage::RemoveLogToStream(&stream3);
  }

  EXPECT_EQ(sev, LogMessage::GetLogToStream(nullptr));
}


TEST(LogTest, WallClockStartTime) {
  uint32_t time = LogMessage::WallClockStartTime();
  // Expect the time to be in a sensible range, e.g. > 2012-01-01.
  EXPECT_GT(time, 1325376000u);
}

// Test the time required to write 1000 80-character logs to an unbuffered file.
#if defined (WEBRTC_ANDROID)
// Fails on Android: https://bugs.chromium.org/p/webrtc/issues/detail?id=4364.
#define MAYBE_Perf DISABLED_Perf
#else
#define MAYBE_Perf Perf
#endif

TEST(LogTest, MAYBE_Perf) {
  std::string path =
      webrtc::test::TempFilename(webrtc::test::OutputPath(), "ut");

  LogSinkImpl<FileStream> stream;
  EXPECT_TRUE(stream.Open(path, "wb", nullptr));
  stream.DisableBuffering();
  LogMessage::AddLogToStream(&stream, LS_SENSITIVE);

  int64_t start = TimeMillis(), finish;
  std::string message('X', 80);
  for (int i = 0; i < 1000; ++i) {
    RTC_LOG(LS_SENSITIVE) << message;
  }
  finish = TimeMillis();

  LogMessage::RemoveLogToStream(&stream);
  stream.Close();
  webrtc::test::RemoveFile(path);

  RTC_LOG(LS_INFO) << "Average log time: " << TimeDiff(finish, start) << " ms";
}

}  // namespace rtc
