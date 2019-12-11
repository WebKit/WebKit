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
#include "test/testsupport/fileutils_override.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_WIN)
const char* kPathDelimiter = "\\";
#else
const char* kPathDelimiter = "/";
#endif

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

bool FileExists(const std::string& file_name) {
  struct stat file_info = {0};
  return stat(file_name.c_str(), &file_info) == 0;
}

bool DirExists(const std::string& directory_name) {
  struct stat directory_info = {0};
  return stat(directory_name.c_str(), &directory_info) == 0 &&
         S_ISDIR(directory_info.st_mode);
}

std::string OutputPath() {
  return webrtc::test::internal::OutputPath();
}

std::string WorkingDir() {
  return webrtc::test::internal::WorkingDir();
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
  return webrtc::test::internal::ResourcePath(name, extension);
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
