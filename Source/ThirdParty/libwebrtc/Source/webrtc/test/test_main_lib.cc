/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_main_lib.h"

#include <fstream>
#include <string>

#include "absl/memory/memory.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/perf_test.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32socketinit.h"
#endif

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"

WEBRTC_DEFINE_string(NSTreatUnknownArgumentsAsOpen,
                     "",
                     "Intentionally ignored flag intended for iOS simulator.");
WEBRTC_DEFINE_string(ApplePersistenceIgnoreState,
                     "",
                     "Intentionally ignored flag intended for iOS simulator.");
WEBRTC_DEFINE_bool(
    save_chartjson_result,
    false,
    "Store the perf results in Documents/perf_result.json in the format "
    "described by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#else

WEBRTC_DEFINE_string(
    isolated_script_test_output,
    "",
    "Path to output an empty JSON file which Chromium infra requires.");

WEBRTC_DEFINE_string(
    isolated_script_test_perf_output,
    "",
    "Path where the perf results should be stored in the JSON format described "
    "by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#endif

WEBRTC_DEFINE_bool(logs, false, "print logs to stderr");

WEBRTC_DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");

WEBRTC_DEFINE_bool(help, false, "Print this message.");

namespace webrtc {

namespace {

class TestMainImpl : public TestMain {
 public:
  int Init(int* argc, char* argv[]) override {
    ::testing::InitGoogleMock(argc, argv);

    // Default to LS_INFO, even for release builds to provide better test
    // logging.
    // TODO(pbos): Consider adding a command-line override.
    if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
      rtc::LogMessage::LogToDebug(rtc::LS_INFO);

    if (rtc::FlagList::SetFlagsFromCommandLine(argc, argv, false)) {
      return 1;
    }
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }

    // TODO(bugs.webrtc.org/9792): we need to reference something from
    // fileutils.h so that our downstream hack where we replace fileutils.cc
    // works. Otherwise the downstream flag implementation will take over and
    // botch the flag introduced by the hack. Remove this awful thing once the
    // downstream implementation has been eliminated.
    (void)webrtc::test::JoinFilename("horrible", "hack");

    webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_force_fieldtrials);
    // InitFieldTrialsFromString stores the char*, so the char array must
    // outlive the application.
    webrtc::field_trial::InitFieldTrialsFromString(FLAG_force_fieldtrials);
    webrtc::metrics::Enable();

#if defined(WEBRTC_WIN)
    winsock_init_ = absl::make_unique<rtc::WinsockInitializer>();
#endif

    rtc::LogMessage::SetLogToStderr(FLAG_logs);

    // Ensure that main thread gets wrapped as an rtc::Thread.
    // TODO(bugs.webrt.org/9714): It might be better to avoid wrapping the main
    // thread, or leave it to individual tests that need it. But as long as we
    // have automatic thread wrapping, we need this to avoid that some other
    // random thread (which one depending on which tests are run) gets
    // automatically wrapped.
    rtc::ThreadManager::Instance()->WrapCurrentThread();
    RTC_CHECK(rtc::Thread::Current());
    return 0;
  }

  int Run(int argc, char* argv[]) override {
#if defined(WEBRTC_IOS)
    rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv,
                             FLAG_save_chartjson_result);
    rtc::test::RunTestsFromIOSApp();
    return 0;
#else
    int exit_code = RUN_ALL_TESTS();

    std::string chartjson_result_file = FLAG_isolated_script_test_perf_output;
    if (!chartjson_result_file.empty()) {
      webrtc::test::WritePerfResults(chartjson_result_file);
    }

    std::string result_filename = FLAG_isolated_script_test_output;
    if (!result_filename.empty()) {
      std::ofstream result_file(result_filename);
      result_file << "{\"version\": 3}";
      result_file.close();
    }

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // We want the test flagged as failed only for sanitizer defects,
    // in which case the sanitizer will override exit code with 66.
    return 0;
#endif

    return exit_code;
#endif
  }

  ~TestMainImpl() override = default;

 private:
#if defined(WEBRTC_WIN)
  std::unique_ptr<rtc::WinsockInitializer> winsock_init_;
#endif
};

}  // namespace

std::unique_ptr<TestMain> TestMain::Create() {
  return absl::make_unique<TestMainImpl>();
}

}  // namespace webrtc
