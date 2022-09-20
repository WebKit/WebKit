/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/file_utils_override.h"

#include <limits.h>
#include <stdio.h>

#if defined(WEBRTC_WIN)
#include <direct.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <codecvt>
#include <locale>

#include "Shlwapi.h"
#include "WinDef.h"
#include "rtc_base/win32.h"

#define GET_CURRENT_DIR _getcwd
#else
#include <unistd.h>

#define GET_CURRENT_DIR getcwd
#endif

#if defined(WEBRTC_IOS)
#include "test/testsupport/ios_file_utils.h"
#endif

#if defined(WEBRTC_MAC)
#include "test/testsupport/mac_file_utils.h"
#endif

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {

std::string DirName(absl::string_view path);
bool CreateDir(absl::string_view directory_name);

namespace internal {

namespace {
#if defined(WEBRTC_WIN)
const absl::string_view kPathDelimiter = "\\";
#elif !defined(WEBRTC_IOS)
const absl::string_view kPathDelimiter = "/";
#endif

#if defined(WEBRTC_ANDROID)
// This is a special case in Chrome infrastructure. See
// base/test/test_support_android.cc.
const absl::string_view kAndroidChromiumTestsRoot =
    "/sdcard/chromium_tests_root/";
#endif

#if !defined(WEBRTC_IOS)
const absl::string_view kResourcesDirName = "resources";
#endif

}  // namespace

// Finds the WebRTC src dir.
// The returned path always ends with a path separator.
absl::optional<std::string> ProjectRootPath() {
#if defined(WEBRTC_ANDROID)
  return std::string(kAndroidChromiumTestsRoot);
#elif defined WEBRTC_IOS
  return IOSRootPath();
#elif defined(WEBRTC_MAC)
  std::string path;
  GetNSExecutablePath(&path);
  std::string exe_dir = DirName(path);
  // On Mac, tests execute in out/Whatever, so src is two levels up except if
  // the test is bundled (which our tests are not), in which case it's 5 levels.
  return DirName(DirName(exe_dir)) + std::string(kPathDelimiter);
#elif defined(WEBRTC_POSIX)
  char buf[PATH_MAX];
  ssize_t count = ::readlink("/proc/self/exe", buf, arraysize(buf));
  if (count <= 0) {
    RTC_DCHECK_NOTREACHED() << "Unable to resolve /proc/self/exe.";
    return absl::nullopt;
  }
  // On POSIX, tests execute in out/Whatever, so src is two levels up.
  std::string exe_dir = DirName(absl::string_view(buf, count));
  return DirName(DirName(exe_dir)) + std::string(kPathDelimiter);
#elif defined(WEBRTC_WIN)
  wchar_t buf[MAX_PATH];
  buf[0] = 0;
  if (GetModuleFileNameW(NULL, buf, MAX_PATH) == 0)
    return absl::nullopt;

  std::string exe_path = rtc::ToUtf8(std::wstring(buf));
  std::string exe_dir = DirName(exe_path);
  return DirName(DirName(exe_dir)) + std::string(kPathDelimiter);
#endif
}

std::string OutputPath() {
#if defined(WEBRTC_IOS)
  return IOSOutputPath();
#elif defined(WEBRTC_ANDROID)
  return std::string(kAndroidChromiumTestsRoot);
#else
  absl::optional<std::string> path_opt = ProjectRootPath();
  RTC_DCHECK(path_opt);
  std::string path = *path_opt + "out";
  if (!CreateDir(path)) {
    return "./";
  }
  return path + std::string(kPathDelimiter);
#endif
}

std::string WorkingDir() {
#if defined(WEBRTC_ANDROID)
  return std::string(kAndroidChromiumTestsRoot);
#else
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return "./";
  } else {
    return std::string(path_buffer);
  }
#endif
}

std::string ResourcePath(absl::string_view name, absl::string_view extension) {
#if defined(WEBRTC_IOS)
  return IOSResourcePath(name, extension);
#else
  absl::optional<std::string> path_opt = ProjectRootPath();
  RTC_DCHECK(path_opt);
  rtc::StringBuilder os(*path_opt);
  os << kResourcesDirName << kPathDelimiter << name << "." << extension;
  return os.Release();
#endif
}

}  // namespace internal
}  // namespace test
}  // namespace webrtc
