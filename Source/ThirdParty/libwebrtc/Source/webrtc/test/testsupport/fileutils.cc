/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fileutils.h"

#include <assert.h>

#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif

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
#include <dirent.h>
#include <unistd.h>

#define GET_CURRENT_DIR getcwd
#endif

#include <sys/stat.h>  // To check for directory existence.
#ifndef S_ISDIR        // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#if defined(WEBRTC_IOS)
#include "test/testsupport/iosfileutils.h"
#endif

#if defined(WEBRTC_MAC)
#include "test/testsupport/macfileutils.h"
#endif

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {

namespace {

#if defined(WEBRTC_WIN)
const char* kPathDelimiter = "\\";
#else
const char* kPathDelimiter = "/";
#endif

#if defined(WEBRTC_ANDROID)
// This is a special case in Chrome infrastructure. See
// base/test/test_support_android.cc.
const char* kAndroidChromiumTestsRoot = "/sdcard/chromium_tests_root/";
#endif

#if !defined(WEBRTC_IOS)
const char* kResourcesDirName = "resources";
#endif

char relative_dir_path[FILENAME_MAX];
bool relative_dir_path_set = false;

}  // namespace

const char* kCannotFindProjectRootDir = "ERROR_CANNOT_FIND_PROJECT_ROOT_DIR";

std::string DirName(const std::string& path) {
  if (path.empty())
    return "";
  if (path == kPathDelimiter)
    return path;

  std::string result = path;
  if (result.back() == *kPathDelimiter)
    result.pop_back();  // Remove trailing separator.

  return result.substr(0, result.find_last_of(kPathDelimiter));
}

void SetExecutablePath(const std::string& path) {
  std::string working_dir = WorkingDir();
  std::string temp_path = path;

  // Handle absolute paths; convert them to relative paths to the working dir.
  if (path.find(working_dir) != std::string::npos) {
    temp_path = path.substr(working_dir.length() + 1);
  }
// On Windows, when tests are run under memory tools like DrMemory and TSan,
// slashes occur in the path as directory separators. Make sure we replace
// such cases with backslashes in order for the paths to be correct.
#ifdef WIN32
  std::replace(temp_path.begin(), temp_path.end(), '/', '\\');
#endif

  // Trim away the executable name; only store the relative dir path.
  temp_path = DirName(temp_path);
  strncpy(relative_dir_path, temp_path.c_str(), FILENAME_MAX);
  relative_dir_path_set = true;
}

bool FileExists(const std::string& file_name) {
  struct stat file_info = {0};
  return stat(file_name.c_str(), &file_info) == 0;
}

bool DirExists(const std::string& directory_name) {
  struct stat directory_info = {0};
  return stat(directory_name.c_str(), &directory_info) == 0 &&
         S_ISDIR(directory_info.st_mode);
}

// Finds the WebRTC src dir.
// The returned path always ends with a path separator.
std::string ProjectRootPath() {
#if defined(WEBRTC_ANDROID)
  return kAndroidChromiumTestsRoot;
#elif defined WEBRTC_IOS
  return IOSRootPath();
#elif defined(WEBRTC_MAC)
  std::string path;
  GetNSExecutablePath(&path);
  std::string exe_dir = DirName(path);
  // On Mac, tests execute in out/Whatever, so src is two levels up except if
  // the test is bundled (which our tests are not), in which case it's 5 levels.
  return DirName(DirName(exe_dir)) + kPathDelimiter;
#elif defined(WEBRTC_POSIX)
  char buf[PATH_MAX];
  ssize_t count = ::readlink("/proc/self/exe", buf, arraysize(buf));
  if (count <= 0) {
    RTC_NOTREACHED() << "Unable to resolve /proc/self/exe.";
    return kCannotFindProjectRootDir;
  }
  // On POSIX, tests execute in out/Whatever, so src is two levels up.
  std::string exe_dir = DirName(std::string(buf, count));
  return DirName(DirName(exe_dir)) + kPathDelimiter;
#elif defined(WEBRTC_WIN)
  wchar_t buf[MAX_PATH];
  buf[0] = 0;
  if (GetModuleFileName(NULL, buf, MAX_PATH) == 0)
    return kCannotFindProjectRootDir;

  std::string exe_path = rtc::ToUtf8(std::wstring(buf));
  std::string exe_dir = DirName(exe_path);
  return DirName(DirName(exe_dir)) + kPathDelimiter;
#endif
}

std::string OutputPath() {
#if defined(WEBRTC_IOS)
  return IOSOutputPath();
#elif defined(WEBRTC_ANDROID)
  return kAndroidChromiumTestsRoot;
#else
  std::string path = ProjectRootPath();
  RTC_DCHECK_NE(path, kCannotFindProjectRootDir);
  path += "out";
  if (!CreateDir(path)) {
    return "./";
  }
  return path + kPathDelimiter;
#endif
}

std::string WorkingDir() {
#if defined(WEBRTC_ANDROID)
  return kAndroidChromiumTestsRoot;
#endif
  char path_buffer[FILENAME_MAX];
  if (!GET_CURRENT_DIR(path_buffer, sizeof(path_buffer))) {
    fprintf(stderr, "Cannot get current directory!\n");
    return "./";
  } else {
    return std::string(path_buffer);
  }
}

// Generate a temporary filename in a safe way.
// Largely copied from talk/base/{unixfilesystem,win32filesystem}.cc.
std::string TempFilename(const std::string& dir, const std::string& prefix) {
#ifdef WIN32
  wchar_t filename[MAX_PATH];
  if (::GetTempFileName(rtc::ToUtf16(dir).c_str(), rtc::ToUtf16(prefix).c_str(),
                        0, filename) != 0)
    return rtc::ToUtf8(filename);
  assert(false);
  return "";
#else
  int len = dir.size() + prefix.size() + 2 + 6;
  std::unique_ptr<char[]> tempname(new char[len]);

  snprintf(tempname.get(), len, "%s/%sXXXXXX", dir.c_str(), prefix.c_str());
  int fd = ::mkstemp(tempname.get());
  if (fd == -1) {
    assert(false);
    return "";
  } else {
    ::close(fd);
  }
  std::string ret(tempname.get());
  return ret;
#endif
}

std::string GenerateTempFilename(const std::string& dir,
                                 const std::string& prefix) {
  std::string filename = TempFilename(dir, prefix);
  RemoveFile(filename);
  return filename;
}

absl::optional<std::vector<std::string>> ReadDirectory(std::string path) {
  if (path.length() == 0)
    return absl::optional<std::vector<std::string>>();

#if defined(WEBRTC_WIN)
  // Append separator character if needed.
  if (path.back() != '\\')
    path += '\\';

  // Init.
  WIN32_FIND_DATA data;
  HANDLE handle = ::FindFirstFile(rtc::ToUtf16(path + '*').c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE)
    return absl::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  do {
    const std::string name = rtc::ToUtf8(data.cFileName);
    if (name != "." && name != "..")
      found_entries.emplace_back(path + name);
  } while (::FindNextFile(handle, &data) == TRUE);

  // Release resources.
  if (handle != INVALID_HANDLE_VALUE)
    ::FindClose(handle);
#else
  // Append separator character if needed.
  if (path.back() != '/')
    path += '/';

  // Init.
  DIR* dir = ::opendir(path.c_str());
  if (dir == nullptr)
    return absl::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  while (dirent* dirent = readdir(dir)) {
    const std::string& name = dirent->d_name;
    if (name != "." && name != "..")
      found_entries.emplace_back(path + name);
  }

  // Release resources.
  closedir(dir);
#endif

  return absl::optional<std::vector<std::string>>(std::move(found_entries));
}

bool CreateDir(const std::string& directory_name) {
  struct stat path_info = {0};
  // Check if the path exists already:
  if (stat(directory_name.c_str(), &path_info) == 0) {
    if (!S_ISDIR(path_info.st_mode)) {
      fprintf(stderr,
              "Path %s exists but is not a directory! Remove this "
              "file and re-run to create the directory.\n",
              directory_name.c_str());
      return false;
    }
  } else {
#ifdef WIN32
    return _mkdir(directory_name.c_str()) == 0;
#else
    return mkdir(directory_name.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
  }
  return true;
}

bool RemoveDir(const std::string& directory_name) {
#ifdef WIN32
  return RemoveDirectoryA(directory_name.c_str()) != FALSE;
#else
  return rmdir(directory_name.c_str()) == 0;
#endif
}

bool RemoveFile(const std::string& file_name) {
#ifdef WIN32
  return DeleteFileA(file_name.c_str()) != FALSE;
#else
  return unlink(file_name.c_str()) == 0;
#endif
}

std::string ResourcePath(const std::string& name,
                         const std::string& extension) {
#if defined(WEBRTC_IOS)
  return IOSResourcePath(name, extension);
#else
  std::string platform = "win";
#ifdef WEBRTC_LINUX
  platform = "linux";
#endif  // WEBRTC_LINUX
#ifdef WEBRTC_MAC
  platform = "mac";
#endif  // WEBRTC_MAC
#ifdef WEBRTC_ANDROID
  platform = "android";
#endif  // WEBRTC_ANDROID

  std::string resources_path =
      ProjectRootPath() + kResourcesDirName + kPathDelimiter;
  std::string resource_file =
      resources_path + name + "_" + platform + "." + extension;
  if (FileExists(resource_file)) {
    return resource_file;
  }
  // Fall back on name without platform.
  return resources_path + name + "." + extension;
#endif  // defined (WEBRTC_IOS)
}

std::string JoinFilename(const std::string& dir, const std::string& name) {
  RTC_CHECK(!dir.empty()) << "Special cases not implemented.";
  return dir + kPathDelimiter + name;
}

size_t GetFileSize(const std::string& filename) {
  FILE* f = fopen(filename.c_str(), "rb");
  size_t size = 0;
  if (f != NULL) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

}  // namespace test
}  // namespace webrtc
