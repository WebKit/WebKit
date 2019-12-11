/*
 *  Copyright 2007 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
//
// A reuseable entry point for gunit tests.

#if defined(WEBRTC_WIN)
#include <crtdbg.h>
#endif

#include "rtc_base/flags.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/sslstreamadapter.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32socketinit.h"
#endif

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"
#endif

WEBRTC_DEFINE_bool(help, false, "prints this message");
WEBRTC_DEFINE_string(log, "", "logging options to use");
WEBRTC_DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");
#if defined(WEBRTC_WIN)
WEBRTC_DEFINE_int(crt_break_alloc, -1, "memory allocation to break on");
WEBRTC_DEFINE_bool(
    default_error_handlers,
    false,
    "leave the default exception/dbg handler functions in place");

void TestInvalidParameterHandler(const wchar_t* expression,
                                 const wchar_t* function,
                                 const wchar_t* file,
                                 unsigned int line,
                                 uintptr_t pReserved) {
  // In order to log `expression`, `function`, and `file` here, we would have
  // to convert them to const char*. std::wcsrtombs can do that, but it's
  // locale dependent.
  RTC_LOG(LS_ERROR) << "InvalidParameter Handler called.  Exiting.";
  exit(1);
}
void TestPureCallHandler() {
  RTC_LOG(LS_ERROR) << "Purecall Handler called.  Exiting.";
  exit(1);
}
int TestCrtReportHandler(int report_type, char* msg, int* retval) {
  RTC_LOG(LS_ERROR) << "CrtReport Handler called...";
  RTC_LOG(LS_ERROR) << msg;
  if (report_type == _CRT_ASSERT) {
    exit(1);
  } else {
    *retval = 0;
    return TRUE;
  }
}
#endif  // WEBRTC_WIN

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, false);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_force_fieldtrials);
  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  webrtc::field_trial::InitFieldTrialsFromString(FLAG_force_fieldtrials);
  webrtc::metrics::Enable();

#if defined(WEBRTC_WIN)
  rtc::WinsockInitializer winsock_init;

  if (!FLAG_default_error_handlers) {
    // Make sure any errors don't throw dialogs hanging the test run.
    _set_invalid_parameter_handler(TestInvalidParameterHandler);
    _set_purecall_handler(TestPureCallHandler);
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, TestCrtReportHandler);
  }

#if !defined(NDEBUG)  // Turn on memory leak checking on Windows.
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  if (FLAG_crt_break_alloc >= 0) {
    _crtBreakAlloc = FLAG_crt_break_alloc;
  }
#endif
#endif  // WEBRTC_WIN

  // By default, log timestamps. Allow overrides by used of a --log flag.
  rtc::LogMessage::LogTimestamps();
  if (*FLAG_log != '\0') {
    rtc::LogMessage::ConfigureLogging(FLAG_log);
  } else if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO) {
    // Default to LS_INFO, even for release builds to provide better test
    // logging.
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
  }

  // Initialize SSL which are used by several tests.
  rtc::InitializeSSL();
  rtc::SSLStreamAdapter::EnableTimeCallbackForTesting();

#if defined(WEBRTC_IOS)
  rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv, false);
  rtc::test::RunTestsFromIOSApp();
#endif
  const int res = RUN_ALL_TESTS();

  rtc::CleanupSSL();

  // clean up logging so we don't appear to leak memory.
  rtc::LogMessage::ConfigureLogging("");

#if defined(WEBRTC_WIN)
  // Unhook crt function so that we don't ever log after statics have been
  // uninitialized.
  if (!FLAG_default_error_handlers)
    _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, TestCrtReportHandler);
#endif

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
  // We want the test flagged as failed only for sanitizer defects,
  // in which case the sanitizer will override exit code with 66.
  return 0;
#endif

  return res;
}
