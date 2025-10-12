/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on Chromium's
// https://source.chromium.org/chromium/chromium/src/+/main:testing/android/native_test/native_test_launcher.cc
// and Angle's
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/angle/src/tests/test_utils/runner/android/AngleNativeTest.cpp

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

#include "test/android/native_test_launcher.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "test/android/native_test_util.h"
#include "test/native_test_jni/NativeTestWebrtc_jni.h"
#include "third_party/jni_zero/jni_zero.h"

// The main function of the program to be wrapped as a test apk.
extern int main(int argc, char** argv);

namespace webrtc {
namespace test {
namespace android {

namespace {

const char kCrashedMarker[] = "[ CRASHED      ]\n";

// The list of signals which are considered to be crashes.
const int kExceptionSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1};

struct sigaction g_old_sa[NSIG];

// This function runs in a compromised context. It should not allocate memory.
void SignalHandler(int sig, siginfo_t* info, void* reserved) {
  // Output the crash marker.
  write(STDOUT_FILENO, kCrashedMarker, sizeof(kCrashedMarker) - 1);
  g_old_sa[sig].sa_sigaction(sig, info, reserved);
}

}  // namespace

static void JNI_NativeTestWebrtc_RunTests(JNIEnv* env,
                                          std::string& command_line_flags,
                                          std::string& command_line_file_path,
                                          std::string& stdout_file_path,
                                          std::string& test_data_dir) {
  AndroidLog(
      ANDROID_LOG_INFO,
      "Entering JNI_NativeTestWebrtc_RunTests with command_line_flags=%s, "
      "command_line_file_path=%s, stdout_file_path=%s, test_data_dir=%s\n",
      command_line_flags.c_str(), command_line_file_path.c_str(),
      stdout_file_path.c_str(), test_data_dir.c_str());

  // Required for DEATH_TESTS.
  pthread_atfork(nullptr, nullptr, jni_zero::DisableJvmForTesting);

  std::vector<std::string> args;

  if (command_line_file_path.empty())
    args.push_back("_");
  else
    ParseArgsFromCommandLineFile(command_line_file_path.c_str(), &args);

  ParseArgsFromString(command_line_flags, &args);

  std::vector<char*> argv;
  int argc = ArgsToArgv(args, &argv);

  // A few options, such "--gtest_list_tests", will just use printf directly
  // Always redirect stdout to a known file.
  if (freopen(stdout_file_path.c_str(), "a+", stdout) == NULL) {
    AndroidLog(ANDROID_LOG_ERROR, "Failed to redirect stream to file: %s: %s\n",
               stdout_file_path.c_str(), strerror(errno));
    exit(EXIT_FAILURE);
  }
  // TODO(jbudorick): Remove this after resolving crbug.com/726880
  AndroidLog(ANDROID_LOG_INFO, "Redirecting stdout to file: %s\n",
             stdout_file_path.c_str());
  dup2(STDOUT_FILENO, STDERR_FILENO);

  // TODO(webrtc:42223878): Wait for debugger.

  ScopedMainEntryLogger scoped_main_entry_logger;
  main(argc, &argv[0]);
}

// TODO(nileshagrawal): now that we're using FIFO, test scripts can detect EOF.
// Remove the signal handlers.
void InstallHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO;

  for (unsigned int i = 0; kExceptionSignals[i] != -1; ++i) {
    sigaction(kExceptionSignals[i], &sa, &g_old_sa[kExceptionSignals[i]]);
  }
}

}  // namespace android
}  // namespace test
}  // namespace webrtc
