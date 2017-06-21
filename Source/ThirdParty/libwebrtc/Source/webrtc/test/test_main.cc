/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gflags/gflags.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

#if defined(WEBRTC_IOS)
#include "webrtc/test/ios/test_support.h"
#endif

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(force_fieldtrials, "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);

  // Default to LS_INFO, even for release builds to provide better test logging.
  // TODO(pbos): Consider adding a command-line override.
  if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);

  // AllowCommandLineParsing allows us to ignore flags passed on to us by
  // Chromium build bots without having to explicitly disable them.
  google::AllowCommandLineReparsing();
  google::ParseCommandLineFlags(&argc, &argv, false);

  webrtc::test::SetExecutablePath(argv[0]);
  webrtc::test::InitFieldTrialsFromString(FLAGS_force_fieldtrials);
  webrtc::metrics::Enable();

  rtc::LogMessage::SetLogToStderr(FLAGS_logs);
  std::unique_ptr<webrtc::test::TraceToStderr> trace_to_stderr;
  if (FLAGS_logs)
      trace_to_stderr.reset(new webrtc::test::TraceToStderr);
#if defined(WEBRTC_IOS)
  rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv);
  rtc::test::RunTestsFromIOSApp();
#endif

  return RUN_ALL_TESTS();
}
