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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event_tracer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/perf_test.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32_socket_init.h"
#endif

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"

ABSL_FLAG(std::string,
          NSTreatUnknownArgumentsAsOpen,
          "",
          "Intentionally ignored flag intended for iOS simulator.");
ABSL_FLAG(std::string,
          ApplePersistenceIgnoreState,
          "",
          "Intentionally ignored flag intended for iOS simulator.");
ABSL_FLAG(
    bool,
    save_chartjson_result,
    false,
    "Store the perf results in Documents/perf_result.json in the format "
    "described by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#else

ABSL_FLAG(std::string,
          isolated_script_test_output,
          "",
          "Path to output an empty JSON file which Chromium infra requires.");

ABSL_FLAG(
    std::string,
    isolated_script_test_perf_output,
    "",
    "Path where the perf results should be stored in the JSON format described "
    "by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#endif

ABSL_FLAG(bool, logs, true, "print logs to stderr");
ABSL_FLAG(bool, verbose, false, "verbose logs to stderr");

ABSL_FLAG(std::string,
          trace_event,
          "",
          "Path to collect trace events (json file) for chrome://tracing. "
          "If not set, events aren't captured.");

ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

namespace webrtc {

namespace {

class TestMainImpl : public TestMain {
 public:
  int Init(int* argc, char* argv[]) override {
    ::testing::InitGoogleMock(argc, argv);
    absl::ParseCommandLine(*argc, argv);

    // Default to LS_INFO, even for release builds to provide better test
    // logging.
    if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
      rtc::LogMessage::LogToDebug(rtc::LS_INFO);

    if (absl::GetFlag(FLAGS_verbose))
      rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    rtc::LogMessage::SetLogToStderr(absl::GetFlag(FLAGS_logs) ||
                                    absl::GetFlag(FLAGS_verbose));

    std::string trace_event_path = absl::GetFlag(FLAGS_trace_event);
    const bool capture_events = !trace_event_path.empty();
    if (capture_events) {
      rtc::tracing::SetupInternalTracer();
      rtc::tracing::StartInternalCapture(trace_event_path.c_str());
    }

    // TODO(bugs.webrtc.org/9792): we need to reference something from
    // fileutils.h so that our downstream hack where we replace fileutils.cc
    // works. Otherwise the downstream flag implementation will take over and
    // botch the flag introduced by the hack. Remove this awful thing once the
    // downstream implementation has been eliminated.
    (void)webrtc::test::JoinFilename("horrible", "hack");

    // InitFieldTrialsFromString stores the char*, so the char array must
    // outlive the application.
    field_trials_ = absl::GetFlag(FLAGS_force_fieldtrials);
    webrtc::field_trial::InitFieldTrialsFromString(field_trials_.c_str());
    webrtc::metrics::Enable();

#if defined(WEBRTC_WIN)
    winsock_init_ = absl::make_unique<rtc::WinsockInitializer>();
#endif

    // Initialize SSL which are used by several tests.
    rtc::InitializeSSL();
    rtc::SSLStreamAdapter::EnableTimeCallbackForTesting();

    // Ensure that main thread gets wrapped as an rtc::Thread.
    // TODO(bugs.webrt.org/9714): It might be better to avoid wrapping the main
    // thread, or leave it to individual tests that need it. But as long as we
    // have automatic thread wrapping, we need this to avoid that some other
    // random thread (which one depending on which tests are run) gets
    // automatically wrapped.
    rtc::ThreadManager::Instance()->WrapCurrentThread();
    RTC_CHECK(rtc::Thread::Current());

    if (capture_events) {
      rtc::tracing::StopInternalCapture();
    }
    return 0;
  }

  int Run(int argc, char* argv[]) override {
#if defined(WEBRTC_IOS)
    rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv,
                             absl::GetFlag(FLAGS_save_chartjson_result));
    rtc::test::RunTestsFromIOSApp();
    return 0;
#else
    int exit_code = RUN_ALL_TESTS();

    std::string chartjson_result_file =
        absl::GetFlag(FLAGS_isolated_script_test_perf_output);
    if (!chartjson_result_file.empty()) {
      webrtc::test::WritePerfResults(chartjson_result_file);
    }

    std::string result_filename =
        absl::GetFlag(FLAGS_isolated_script_test_output);
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
