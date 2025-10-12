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

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::HasSubstr;
using ::testing::StartsWith;

#if defined(WEBRTC_WIN)
constexpr char kFakeFilePath[] = "some\\path\\myfile.cc";
#else
constexpr char kFakeFilePath[] = "some/path/myfile.cc";
#endif

}  // namespace

class LogSinkImpl : public LogSink {
 public:
  explicit LogSinkImpl(std::string* log_data) : log_data_(log_data) {}

  template <typename P>
  explicit LogSinkImpl(P* p) {}

 private:
  void OnLogMessage(const std::string& message) override {
    OnLogMessage(absl::string_view(message));
  }
  void OnLogMessage(absl::string_view message) override {
    log_data_->append(message.begin(), message.end());
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
  const char* get_tag() const { return log_line_.tag().data(); }
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
  EXPECT_THAT(str, HasSubstr("INFO"));
  EXPECT_THAT(str, Not(HasSubstr("VERBOSE")));

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

  // Log all supported types(except doubles/floats) as a sanity-check.
  RTC_LOG(LS_INFO) << "|" << i << "|" << l << "|" << ll << "|" << u << "|" << ul
                   << "|" << ull << "|" << s1.c_str() << "|" << s2 << "|"
                   << absl::string_view(s3) << "|" << p << "|" << null_string
                   << "|";

  // Signed integers
  EXPECT_THAT(str, HasSubstr("|1|"));
  EXPECT_THAT(str, HasSubstr("|2|"));
  EXPECT_THAT(str, HasSubstr("|3|"));

  // Unsigned integers
  EXPECT_THAT(str, HasSubstr("|4|"));
  EXPECT_THAT(str, HasSubstr("|5|"));
  EXPECT_THAT(str, HasSubstr("|6|"));

  // Strings
  EXPECT_THAT(str, HasSubstr("|char*|"));
  EXPECT_THAT(str, HasSubstr("|std::string|"));
  EXPECT_THAT(str, HasSubstr("|absl::stringview|"));

  // void*
  EXPECT_THAT(str, HasSubstr("|abcd|"));

  // null char*
  EXPECT_THAT(str, HasSubstr("|(null)|"));

  LogMessage::RemoveLogToStream(&stream);
  EXPECT_EQ(LS_NONE, LogMessage::GetLogToStream(&stream));
  EXPECT_EQ(sev, LogMessage::GetLogToStream(nullptr));
}

TEST(LogTest, LogIfLogIfConditionIsTrue) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);

  RTC_LOG_IF(LS_INFO, true) << "Hello";
  EXPECT_THAT(str, HasSubstr("Hello"));

  LogMessage::RemoveLogToStream(&stream);
}

TEST(LogTest, LogIfDontLogIfConditionIsFalse) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);

  RTC_LOG_IF(LS_INFO, false) << "Hello";
  EXPECT_THAT(str, Not(HasSubstr("Hello")));

  LogMessage::RemoveLogToStream(&stream);
}

TEST(LogTest, LogIfFLogIfConditionIsTrue) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);

  RTC_LOG_IF_F(LS_INFO, true) << "Hello";
  EXPECT_THAT(str, HasSubstr(__FUNCTION__));
  EXPECT_THAT(str, HasSubstr("Hello"));

  LogMessage::RemoveLogToStream(&stream);
}

TEST(LogTest, LogIfFDontLogIfConditionIsFalse) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);

  RTC_LOG_IF_F(LS_INFO, false) << "Not";
  EXPECT_THAT(str, Not(HasSubstr(__FUNCTION__)));
  EXPECT_THAT(str, Not(HasSubstr("Not")));

  LogMessage::RemoveLogToStream(&stream);
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
  LogMessageForTesting log_msg(kFakeFilePath, 100, LS_WARNING, ERRCTX_ERRNO,
                               0xD);
  log_msg.stream() << "This gets added at dtor time";

  EXPECT_THAT(log_msg.get_extra(), StartsWith("[0x0000000D]"));
}

TEST(LogTest, CheckFilePathParsed) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream));
#if defined(WEBRTC_ANDROID)
  const char* tag = nullptr;
#endif
  {
    LogMessageForTesting log_msg(kFakeFilePath, 100, LS_INFO);
    log_msg.stream() << "<- Does this look right?";
#if defined(WEBRTC_ANDROID)
    tag = log_msg.get_tag();
#endif
  }

#if defined(WEBRTC_ANDROID)
  EXPECT_NE(nullptr, strstr(tag, "myfile.cc"));
  EXPECT_THAT(str, HasSubstr("100"));
#else
  EXPECT_THAT(str, HasSubstr("(myfile.cc:100)"));
#endif
  LogMessage::RemoveLogToStream(&stream);
}

#if defined(WEBRTC_ANDROID)
TEST(LogTest, CheckTagAddedToStringInDefaultOnLogMessageAndroid) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  EXPECT_EQ(LS_INFO, LogMessage::GetLogToStream(&stream));

  RTC_LOG_TAG(LS_INFO, "my_tag") << "INFO";
  EXPECT_THAT(str, HasSubstr("INFO"));
  EXPECT_THAT(str, HasSubstr("my_tag"));
}
#endif

// Test the time required to write 1000 80-character logs to a string.
TEST(LogTest, Perf) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_VERBOSE);

  const std::string message(80, 'X');
  {
    LogMessageForTesting sanity_check_msg(__FILE__, __LINE__, LS_VERBOSE);
  }

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
  EXPECT_THAT(str, HasSubstr("[0]"));
  EXPECT_THAT(str, Not(HasSubstr("[1]")));
  RTC_LOG(LS_INFO) << "[" << TestEnum::kValue1 << "]";
  EXPECT_THAT(str, HasSubstr("[1]"));
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

struct StructWithStringfy {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const StructWithStringfy& /*self*/) {
    sink.Append("absl-stringify");
  }
};

TEST(LogTest, UseAbslStringForCustomTypes) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  StructWithStringfy t;

  RTC_LOG(LS_INFO) << t;

  EXPECT_THAT(str, HasSubstr("absl-stringify"));

  LogMessage::RemoveLogToStream(&stream);
}

enum class TestEnumStringify { kValue0 = 0, kValue1 = 1 };

template <typename Sink>
void AbslStringify(Sink& sink, TestEnumStringify value) {
  switch (value) {
    case TestEnumStringify::kValue0:
      sink.Append("kValue0");
      break;
    case TestEnumStringify::kValue1:
      sink.Append("kValue1");
      break;
  }
}

TEST(LogTest, EnumSupportsAbslStringify) {
  std::string str;
  LogSinkImpl stream(&str);
  LogMessage::AddLogToStream(&stream, LS_INFO);
  RTC_LOG(LS_INFO) << "[" << TestEnumStringify::kValue0 << "]";
  EXPECT_THAT(str, HasSubstr("[kValue0]"));
  EXPECT_THAT(str, Not(HasSubstr("[kValue1]")));
  RTC_LOG(LS_INFO) << "[" << TestEnumStringify::kValue1 << "]";
  EXPECT_THAT(str, HasSubstr("[kValue1]"));
  LogMessage::RemoveLogToStream(&stream);
}

}  // namespace webrtc
#endif  // RTC_LOG_ENABLED()
