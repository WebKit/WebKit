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

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/test/metrics/chrome_perf_dashboard_metrics_exporter.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metrics_exporter.h"
#include "api/test/metrics/metrics_set_proto_file_exporter.h"
#include "api/test/metrics/print_result_proxy_metrics_exporter.h"
#include "api/test/metrics/stdout_metrics_exporter.h"
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

#if defined(RTC_USE_PERFETTO)
#include "rtc_base/event_tracer.h"
#include "third_party/perfetto/include/perfetto/tracing/backend_type.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"
#endif

#if defined(WEBRTC_WIN)
#include "rtc_base/win32_socket_init.h"
#endif

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"

ABSL_FLAG(std::string,
          NSTreatUnknownArgumentsAsOpen,
          "",
          "Intentionally ignored flag intended for iOS test runner.");
ABSL_FLAG(std::string,
          ApplePersistenceIgnoreState,
          "",
          "Intentionally ignored flag intended for iOS test runner.");
ABSL_FLAG(bool,
          enable_run_ios_unittests_with_xctest,
          false,
          "Intentionally ignored flag intended for iOS test runner.");
ABSL_FLAG(bool,
          write_compiled_tests_json_to_writable_path,
          false,
          "Intentionally ignored flag intended for iOS test runner.");

// This is the cousin of isolated_script_test_perf_output, but we can't dictate
// where to write on iOS so the semantics of this flag are a bit different.
ABSL_FLAG(
    bool,
    write_perf_output_on_ios,
    false,
    "Store the perf results in Documents/perftest_result.pb in the format "
    "described by histogram.proto in "
    "https://chromium.googlesource.com/catapult/.");

#elif defined(WEBRTC_FUCHSIA)
ABSL_FLAG(std::string, use_vulkan, "", "Intentionally ignored flag.");
#else
// TODO(bugs.webrtc.org/8115): Remove workaround when fixed.
ABSL_FLAG(bool, no_sandbox, false, "Intentionally ignored flag.");
ABSL_FLAG(bool, test_launcher_bot_mode, false, "Intentionally ignored flag.");
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
          "Path to collect trace events. If not set, events aren't captured.");

ABSL_FLAG(std::string,
          test_launcher_shard_index,
          "",
          "Index of the test shard to run, from 0 to "
          "the value specified with --test_launcher_total_shards.");

ABSL_FLAG(std::string,
          test_launcher_total_shards,
          "",
          "Total number of shards.");

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
      StartTracingCapture(trace_event_path);
    }

    std::optional<std::vector<std::string>> metrics_to_plot =
        absl::GetFlag(FLAGS_plot);

    if (metrics_to_plot->empty()) {
      metrics_to_plot = std::nullopt;
    } else {
      if (metrics_to_plot->size() == 1 &&
          (*metrics_to_plot)[0] == kPlotAllMetrics) {
        metrics_to_plot->clear();
      }
    }
    // The sharding arguments take precedence over the sharding environment
    // variables.
    if (!absl::GetFlag(FLAGS_test_launcher_shard_index).empty() &&
        !absl::GetFlag(FLAGS_test_launcher_total_shards).empty()) {
      std::string shard_index =
          "GTEST_SHARD_INDEX=" + absl::GetFlag(FLAGS_test_launcher_shard_index);
      std::string total_shards =
          "GTEST_TOTAL_SHARDS=" +
          absl::GetFlag(FLAGS_test_launcher_total_shards);
      putenv(total_shards.data());
      putenv(shard_index.data());
    }

#if defined(WEBRTC_IOS)
    rtc::test::InitTestSuite(
        RUN_ALL_TESTS, argc, argv,
        absl::GetFlag(FLAGS_write_perf_output_on_ios),
        absl::GetFlag(FLAGS_export_perf_results_new_api),
        absl::GetFlag(FLAGS_webrtc_test_metrics_output_path), metrics_to_plot);
    rtc::test::RunTestsFromIOSApp();
    int exit_code = 0;
#else
    int exit_code = RUN_ALL_TESTS();

    std::vector<std::unique_ptr<test::MetricsExporter>> exporters;
    if (absl::GetFlag(FLAGS_export_perf_results_new_api)) {
      exporters.push_back(std::make_unique<test::StdoutMetricsExporter>());
      if (!absl::GetFlag(FLAGS_webrtc_test_metrics_output_path).empty()) {
        exporters.push_back(
            std::make_unique<webrtc::test::MetricsSetProtoFileExporter>(
                webrtc::test::MetricsSetProtoFileExporter::Options(
                    absl::GetFlag(FLAGS_webrtc_test_metrics_output_path))));
      }
      if (!absl::GetFlag(FLAGS_isolated_script_test_perf_output).empty()) {
        exporters.push_back(
            std::make_unique<test::ChromePerfDashboardMetricsExporter>(
                absl::GetFlag(FLAGS_isolated_script_test_perf_output)));
      }
    } else {
      exporters.push_back(
          std::make_unique<test::PrintResultProxyMetricsExporter>());
    }
    test::ExportPerfMetric(*test::GetGlobalMetricsLogger(),
                           std::move(exporters));
    if (!absl::GetFlag(FLAGS_export_perf_results_new_api)) {
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
    }

    std::string result_filename =
        absl::GetFlag(FLAGS_isolated_script_test_output);
    if (!result_filename.empty()) {
      std::ofstream result_file(result_filename);
      result_file << "{\"version\": 3}";
    }
#endif

    if (capture_events) {
      StopTracingCapture();
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
#if defined(RTC_USE_PERFETTO)
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  FILE* tracing_output_file_ = nullptr;
#endif

  void StartTracingCapture(absl::string_view trace_output_file) {
#if defined(RTC_USE_PERFETTO)
    tracing_output_file_ = std::fopen(trace_output_file.data(), "w");
    if (!tracing_output_file_) {
      RTC_LOG(LS_ERROR) << "Failed to open trace file \"" << trace_output_file
                        << "\". Tracing will be disabled.";
    }
    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kInProcessBackend;
    perfetto::Tracing::Initialize(args);
    webrtc::RegisterPerfettoTrackEvents();

    perfetto::TraceConfig cfg;
    cfg.add_buffers()->set_size_kb(1024);  // Record up to 1 MiB.
    tracing_session_ = perfetto::Tracing::NewTrace();
    tracing_session_->Setup(cfg);
    RTC_LOG(LS_INFO)
        << "Starting tracing with Perfetto and outputting to file \""
        << trace_output_file << "\"";
    tracing_session_->StartBlocking();
#else
    rtc::tracing::SetupInternalTracer();
    rtc::tracing::StartInternalCapture(trace_output_file);
#endif
  }

  void StopTracingCapture() {
#if defined(RTC_USE_PERFETTO)
    if (tracing_output_file_) {
      RTC_CHECK(tracing_session_);
      tracing_session_->StopBlocking();
      std::vector<char> tracing_data = tracing_session_->ReadTraceBlocking();
      size_t count = std::fwrite(tracing_data.data(), sizeof tracing_data[0],
                                 tracing_data.size(), tracing_output_file_);
      if (count != tracing_data.size()) {
        RTC_LOG(LS_ERROR) << "Expected to write " << tracing_data.size()
                          << " bytes but only " << count << " bytes written";
      }
      std::fclose(tracing_output_file_);
      tracing_output_file_ = nullptr;
    } else {
      RTC_LOG(LS_INFO) << "no file";
    }

#else
    rtc::tracing::StopInternalCapture();
#endif
  }
};

}  // namespace

std::unique_ptr<TestMain> TestMain::Create() {
  return std::make_unique<TestMainImpl>();
}

}  // namespace webrtc
