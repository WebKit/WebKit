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
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/event_tracer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/test_flags.h"
#include "test/testsupport/perf_test.h"
#include "test/testsupport/resources_dir_flag.h"

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

// This is the cousin of isolated_script_test_perf_output, but we can't dictate
// where to write on iOS so the semantics of this flag are a bit different.
ABSL_FLAG(
    bool,
    write_perf_output_on_ios,
    false,
    "Store the perf results in Documents/perftest_result.pb in the format "
    "described by histogram.proto in "
    "https://chromium.googlesource.com/catapult/.");

#endif

ABSL_FLAG(std::string,
          isolated_script_test_output,
          "",
          "Path to output an empty JSON file which Chromium infra requires.");

ABSL_FLAG(bool, logs, true, "print logs to stderr");
ABSL_FLAG(bool, verbose, false, "verbose logs to stderr");

ABSL_FLAG(std::string,
          trace_event,
          "",
          "Path to collect trace events (json file) for chrome://tracing. "
          "If not set, events aren't captured.");

namespace webrtc {

namespace {

constexpr char kPlotAllMetrics[] = "all";

class TestMainImpl : public TestMain {
 public:
  int Init(int* argc, char* argv[]) override { return Init(); }

  int Init() override {
    // Make sure we always pull in the --resources_dir flag, even if the test
    // binary doesn't link with fileutils (downstream expects all test mains to
    // have this flag).
    (void)absl::GetFlag(FLAGS_resources_dir);

    // Default to LS_INFO, even for release builds to provide better test
    // logging.
    if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
      rtc::LogMessage::LogToDebug(rtc::LS_INFO);

    if (absl::GetFlag(FLAGS_verbose))
      rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    rtc::LogMessage::SetLogToStderr(absl::GetFlag(FLAGS_logs) ||
                                    absl::GetFlag(FLAGS_verbose));

    // InitFieldTrialsFromString stores the char*, so the char array must
    // outlive the application.
    field_trials_ = absl::GetFlag(FLAGS_force_fieldtrials);
    webrtc::field_trial::InitFieldTrialsFromString(field_trials_.c_str());
    webrtc::metrics::Enable();

#if defined(WEBRTC_WIN)
    winsock_init_ = std::make_unique<rtc::WinsockInitializer>();
#endif

    // Initialize SSL which are used by several tests.
    rtc::InitializeSSL();
    rtc::SSLStreamAdapter::EnableTimeCallbackForTesting();

    return 0;
  }

  int Run(int argc, char* argv[]) override {
    std::string trace_event_path = absl::GetFlag(FLAGS_trace_event);
    const bool capture_events = !trace_event_path.empty();
    if (capture_events) {
      rtc::tracing::SetupInternalTracer();
      rtc::tracing::StartInternalCapture(trace_event_path);
    }

    absl::optional<std::vector<std::string>> metrics_to_plot =
        absl::GetFlag(FLAGS_plot);

    if (metrics_to_plot->empty()) {
      metrics_to_plot = absl::nullopt;
    } else {
      if (metrics_to_plot->size() == 1 &&
          (*metrics_to_plot)[0] == kPlotAllMetrics) {
        metrics_to_plot->clear();
      }
    }

#if defined(WEBRTC_IOS)
    rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv,
                             absl::GetFlag(FLAGS_write_perf_output_on_ios),
                             metrics_to_plot);
    rtc::test::RunTestsFromIOSApp();
    int exit_code = 0;
#else
    int exit_code = RUN_ALL_TESTS();

    std::string perf_output_file =
        absl::GetFlag(FLAGS_isolated_script_test_perf_output);
    if (!perf_output_file.empty()) {
      if (!webrtc::test::WritePerfResults(perf_output_file)) {
        return 1;
      }
    }
    if (metrics_to_plot) {
      webrtc::test::PrintPlottableResults(*metrics_to_plot);
    }

    std::string result_filename =
        absl::GetFlag(FLAGS_isolated_script_test_output);
    if (!result_filename.empty()) {
      std::ofstream result_file(result_filename);
      result_file << "{\"version\": 3}";
    }
#endif

    if (capture_events) {
      rtc::tracing::StopInternalCapture();
    }

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // We want the test flagged as failed only for sanitizer defects,
    // in which case the sanitizer will override exit code with 66.
    exit_code = 0;
#endif

    return exit_code;
  }

  ~TestMainImpl() override = default;

 private:
#if defined(WEBRTC_WIN)
  std::unique_ptr<rtc::WinsockInitializer> winsock_init_;
#endif
};

}  // namespace

std::unique_ptr<TestMain> TestMain::Create() {
  return std::make_unique<TestMainImpl>();
}

}  // namespace webrtc
