/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logging.h"

#if RTC_LOG_ENABLED()

#include <string.h>

#include <algorithm>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace rtc {

class LogSinkImpl : public LogSink {
 public:
  explicit LogSinkImpl(std::string* log_data) : log_data_(log_data) {}

  template <typename P>
  explicit LogSinkImpl(P* p) {}

 private:
  void OnLogMessage(const std::string& message) override {
    log_data_->append(message);
  }
  std::string* const log_data_;
};

class LogMessageForTesting : public LogMessage {
 public:
  LogMessageForTesting(const char* file,
                       int line,
                       LoggingSeverity sev,
                       LogErrorContext err_ctx = ERRCTX_NONE,
                       int err = 0)
      : LogMessage(file, line, sev, err_ctx, err) {}

  const std::string& get_extra() const { return extra_; }
#if defined(WEBRTC_ANDROID)
  const char* get_tag() const { return tag_; }
#endif

  // Returns the contents of the internal log stream.
  // Note that parts of the stream won't (as is) be available until *after* the
  // dtor of the parent class has run. So, as is, this only represents a
  // partially built stream.
  std::string GetPrintStream() {
    RTC_DCHECK(!is_finished_);
    is_finished_ = true;
    FinishPrintStream();
    return print_stream_.Release();
  }

 private:
  bool is_finished_ = false;
};

// Test basic logging operation. We should get the INFO log but not the VERBOSE.
// We should restore the correct global state at the end.
TEST(LogTest, SingleStream) {
  int sev = LogMessage::GetLogToStream(nullptr);

  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream));

  RTC_LOG(LS_INFO) << "INFO";
  RTC_LOG(LS_VERBOSE) << "VERBOSE";
  EXPECT_NE(std::string::npos, str.find("INFO"));
  EXPECT_EQ(std::string::npos, str.find("VERBOSE"));

  int i = 1;
  long l = 2l;
  long long ll = 3ll;

  unsigned int u = 4u;
  unsigned long ul = 5ul;
  unsigned long long ull = 6ull;

  std::string s1 = "char*";
  std::string s2 = "std::string";
  std::string s3 = "absl::stringview";
  const char* null_string = nullptr;
  void* p = reinterpret_cast<void*>(0xabcd);

  // Log all suported types(except doubles/floats) as a sanity-check.
  RTC_LOG(LS_INFO) << "|" << i << "|" << l << "|" << ll << "|" << u << "|" << ul
                   << "|" << ull << "|" << s1.c_str() << "|" << s2 << "|"
                   << absl::string_view(s3) << "|" << p << "|" << null_string
                   << "|";

  // Signed integers
  EXPECT_NE(std::string::npos, str.find("|1|"));
  EXPECT_NE(std::string::npos, str.find("|2|"));
  EXPECT_NE(std::string::npos, str.find("|3|"));

  // Unsigned integers
  EXPECT_NE(std::string::npos, str.find("|4|"));
  EXPECT_NE(std::string::npos, str.find("|5|"));
  EXPECT_NE(std::string::npos, str.find("|6|"));

  // Strings
  EXPECT_NE(std::string::npos, str.find("|char*|"));
  EXPECT_NE(std::string::npos, str.find("|std::string|"));
  EXPECT_NE(std::string::npos, str.find("|absl::stringview|"));

  // void*
  EXPECT_NE(std::string::npos, str.find("|abcd|"));

  // null char*
  EXPECT_NE(std::string::npos, str.find("|(null)|"));

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
  LogSinkImpl stream1(&str1), stream2(&str2);
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

class LogThread {
 public:
  void Start() {
    thread_ = PlatformThread::SpawnJoinable(
        [] { RTC_LOG(LS_VERBOSE) << "RTC_LOG"; }, "LogThread");
  }

 private:
  PlatformThread thread_;
};

// Ensure we don't crash when adding/removing streams while threads are going.
// We should restore the correct global state at the end.
TEST(LogTest, MultipleThreads) {
  int sev = LogMessage::GetLogToStream(nullptr);

  LogThread thread1, thread2, thread3;
  thread1.Start();
  thread2.Start();
  thread3.Start();

  std::string s1, s2, s3;
  LogSinkImpl stream1(&s1), stream2(&s2), stream3(&s3);
  for (int i = 0; i < 1000; ++i) {
    LogMessage::AddLogToStream(&stream1, LS_WARNING);
    LogMessage::AddLogToStream(&stream2, LS_INFO);
    LogMessage::AddLogToStream(&stream3, LS_VERBOSE);
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

TEST(LogTest, CheckExtraErrorField) {
  LogMessageForTesting log_msg("some/path/myfile.cc", 100, LS_WARNING,
                               ERRCTX_ERRNO, 0xD);
  log_msg.stream() << "This gets added at dtor time";

  const std::string& extra = log_msg.get_extra();
  const size_t length_to_check = arraysize("[0x12345678]") - 1;
  ASSERT_GE(extra.length(), length_to_check);
  EXPECT_EQ(std::string("[0x0000000D]"), extra.substr(0, length_to_check));
}

TEST(LogTest, CheckFilePathParsed) {
  LogMessageForTesting log_msg("some/path/myfile.cc", 100, LS_INFO);
  log_msg.stream() << "<- Does this look right?";

  const std::string stream = log_msg.GetPrintStream();
#if defined(WEBRTC_ANDROID)
  const char* tag = log_msg.get_tag();
  EXPECT_NE(nullptr, strstr(tag, "myfile.cc"));
  EXPECT_NE(std::string::npos, stream.find("100"));
#else
  EXPECT_NE(std::string::npos, stream.find("(myfile.cc:100)"));
#endif
}

#if defined(WEBRTC_ANDROID)
TEST(LogTest, CheckTagAddedToStringInDefaultOnLogMessageAndroid) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream));

  RTC_LOG_TAG(LS_INFO, "my_tag") << "INFO";
  EXPECT_NE(std::string::npos, str.find("INFO"));
  EXPECT_NE(std::string::npos, str.find("my_tag"));
}
#endif

// Test the time required to write 1000 80-character logs to a string.
TEST(LogTest, Perf) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_VERBOSE);

  const std::string message(80, 'X');
  { LogMessageForTesting sanity_check_msg(__FILE__, __LINE__, LS_VERBOSE); }

  // We now know how many bytes the logging framework will tag onto every msg.
  const size_t logging_overhead = str.size();
  // Reset the stream to 0 size.
  str.clear();
  str.reserve(120000);
  static const int kRepetitions = 1000;

  int64_t start = TimeMillis(), finish;
  for (int i = 0; i < kRepetitions; ++i) {
    LogMessageForTesting(__FILE__, __LINE__, LS_VERBOSE).stream() << message;
  }
  finish = TimeMillis();

  LogMessage::RemoveLogToStream(&stream);

  EXPECT_EQ(str.size(), (message.size() + logging_overhead) * kRepetitions);
  RTC_LOG(LS_INFO) << "Total log time: " << TimeDiff(finish, start)
                   << " ms "
                      " total bytes logged: "
                   << str.size();
}

TEST(LogTest, EnumsAreSupported) {
  enum class TestEnum { kValue0 = 0, kValue1 = 1 };
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  RTC_LOG(LS_INFO) << "[" << TestEnum::kValue0 << "]";
  EXPECT_NE(std::string::npos, str.find("[0]"));
  EXPECT_EQ(std::string::npos, str.find("[1]"));
  RTC_LOG(LS_INFO) << "[" << TestEnum::kValue1 << "]";
  EXPECT_NE(std::string::npos, str.find("[1]"));
  LogMessage::RemoveLogToStream(&stream);
}

TEST(LogTest, NoopSeverityDoesNotRunStringFormatting) {
  if (!LogMessage::IsNoop(LS_VERBOSE)) {
    RTC_LOG(LS_WARNING) << "Skipping test since verbose logging is turned on.";
    return;
  }
  bool was_called = false;
  auto cb = [&was_called]() {
    was_called = true;
    return "This could be an expensive callback.";
  };
  RTC_LOG(LS_VERBOSE) << "This should not be logged: " << cb();
  EXPECT_FALSE(was_called);
}

}  // namespace rtc
#endif  // RTC_LOG_ENABLED()
