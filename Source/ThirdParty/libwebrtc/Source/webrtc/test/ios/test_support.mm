/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <UIKit/UIKit.h>

#include "api/test/metrics/chrome_perf_dashboard_metrics_exporter.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metrics_exporter.h"
#include "api/test/metrics/metrics_set_proto_file_exporter.h"
#include "api/test/metrics/print_result_proxy_metrics_exporter.h"
#include "api/test/metrics/stdout_metrics_exporter.h"
#include "test/ios/coverage_util_ios.h"
#include "test/ios/google_test_runner_delegate.h"
#include "test/ios/test_support.h"
#include "test/testsupport/perf_test.h"

#import "sdk/objc/helpers/NSString+StdString.h"

// Springboard will kill any iOS app that fails to check in after launch within
// a given time. Starting a UIApplication before invoking TestSuite::Run
// prevents this from happening.

// InitIOSRunHook saves the TestSuite and argc/argv, then invoking
// RunTestsFromIOSApp calls UIApplicationMain(), providing an application
// delegate class: WebRtcUnitTestDelegate. The delegate implements
// application:didFinishLaunchingWithOptions: to invoke the TestSuite's Run
// method.

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

// If enabled, runs unittests using the XCTest test runner.
const char kEnableRunIOSUnittestsWithXCTest[] = "enable-run-ios-unittests-with-xctest";

static int (*g_test_suite)(void) = NULL;
static int g_argc;
static char **g_argv;
static bool g_write_perf_output;
static bool g_export_perf_results_new_api;
static std::string g_webrtc_test_metrics_output_path;
static std::optional<bool> g_is_xctest;
static std::optional<std::vector<std::string>> g_metrics_to_plot;

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

@interface WebRtcUnitTestDelegate : NSObject <GoogleTestRunnerDelegate> {
  UIWindow *_window;
}
- (void)runTests;
@end

@implementation WebRtcUnitTestDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  CGRect bounds = [[UIScreen mainScreen] bounds];

  _window = [[UIWindow alloc] initWithFrame:bounds];
  [_window setBackgroundColor:[UIColor whiteColor]];
  [_window makeKeyAndVisible];

  // Add a label with the app name.
  UILabel *label = [[UILabel alloc] initWithFrame:bounds];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  [_window addSubview:label];

  // An NSInternalInconsistencyException is thrown if the app doesn't have a
  // root view controller. Set an empty one here.
  [_window setRootViewController:[[UIViewController alloc] init]];

  if (!rtc::test::ShouldRunIOSUnittestsWithXCTest()) {
    // When running in XCTest mode, XCTest will invoke `runGoogleTest` directly.
    // Otherwise, schedule a call to `runTests`.
    [self performSelector:@selector(runTests) withObject:nil afterDelay:0.1];
  }

  return YES;
}

- (BOOL)supportsRunningGoogleTests {
  return rtc::test::ShouldRunIOSUnittestsWithXCTest();
}

- (int)runGoogleTests {
  rtc::test::ConfigureCoverageReportPath();

  int exitStatus = g_test_suite();

  NSArray<NSString *> *outputDirectories =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  std::vector<std::unique_ptr<webrtc::test::MetricsExporter>> exporters;
  if (g_export_perf_results_new_api) {
    exporters.push_back(std::make_unique<webrtc::test::StdoutMetricsExporter>());
    if (g_write_perf_output) {
      // Stores data into a proto file under the app's document directory.
      NSString *fileName = @"perftest-output.pb";
      if ([outputDirectories count] != 0) {
        NSString *outputPath = [outputDirectories[0] stringByAppendingPathComponent:fileName];

        exporters.push_back(std::make_unique<webrtc::test::ChromePerfDashboardMetricsExporter>(
            [NSString stdStringForString:outputPath]));
      }
    }
    if (!g_webrtc_test_metrics_output_path.empty()) {
      RTC_CHECK_EQ(g_webrtc_test_metrics_output_path.find('/'), std::string::npos)
          << "On iOS, --webrtc_test_metrics_output_path must only be a file name.";
      if ([outputDirectories count] != 0) {
        NSString *fileName = [NSString stringWithCString:g_webrtc_test_metrics_output_path.c_str()
                                                encoding:[NSString defaultCStringEncoding]];
        NSString *outputPath = [outputDirectories[0] stringByAppendingPathComponent:fileName];
        exporters.push_back(std::make_unique<webrtc::test::MetricsSetProtoFileExporter>(
            webrtc::test::MetricsSetProtoFileExporter::Options(
                [NSString stdStringForString:outputPath])));
      }
    }
  } else {
    exporters.push_back(std::make_unique<webrtc::test::PrintResultProxyMetricsExporter>());
  }
  webrtc::test::ExportPerfMetric(*webrtc::test::GetGlobalMetricsLogger(), std::move(exporters));
  if (!g_export_perf_results_new_api) {
    if (g_write_perf_output) {
      // Stores data into a proto file under the app's document directory.
      NSString *fileName = @"perftest-output.pb";
      if ([outputDirectories count] != 0) {
        NSString *outputPath = [outputDirectories[0] stringByAppendingPathComponent:fileName];

        if (!webrtc::test::WritePerfResults([NSString stdStringForString:outputPath])) {
          return 1;
        }
      }
    }
    if (g_metrics_to_plot) {
      webrtc::test::PrintPlottableResults(*g_metrics_to_plot);
    }
  }

  return exitStatus;
}

- (void)runTests {
  RTC_DCHECK(!rtc::test::ShouldRunIOSUnittestsWithXCTest());
  rtc::test::ConfigureCoverageReportPath();

  int exitStatus = [self runGoogleTests];

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(crbug.com/137010): Figure out how much time is actually needed, and
  // sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication *application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end
namespace rtc {
namespace test {

// Note: This is not thread safe, and must be called from the same thread as
// runTests above.
void InitTestSuite(int (*test_suite)(void),
                   int argc,
                   char *argv[],
                   bool write_perf_output,
                   bool export_perf_results_new_api,
                   std::string webrtc_test_metrics_output_path,
                   std::optional<std::vector<std::string>> metrics_to_plot) {
  g_test_suite = test_suite;
  g_argc = argc;
  g_argv = argv;
  g_write_perf_output = write_perf_output;
  g_export_perf_results_new_api = export_perf_results_new_api;
  g_webrtc_test_metrics_output_path = webrtc_test_metrics_output_path;
  g_metrics_to_plot = std::move(metrics_to_plot);
}

void RunTestsFromIOSApp() {
  @autoreleasepool {
    exit(UIApplicationMain(g_argc, g_argv, nil, @"WebRtcUnitTestDelegate"));
  }
}

bool ShouldRunIOSUnittestsWithXCTest() {
  if (g_is_xctest.has_value()) {
    return g_is_xctest.value();
  }

  char **argv = g_argv;
  while (*argv != nullptr) {
    if (strstr(*argv, kEnableRunIOSUnittestsWithXCTest) != nullptr) {
      g_is_xctest = std::optional<bool>(true);
      return true;
    }
    argv++;
  }
  g_is_xctest = std::optional<bool>(false);
  return false;
}

}  // namespace test
}  // namespace rtc
