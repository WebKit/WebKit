/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/file_utils.h"

#include <stdlib.h>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/testsupport/file_utils_override.h"

#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif

#if defined(WEBRTC_WIN)
#include <Shlwapi.h>
#include <WinDef.h>
#include <direct.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <locale>

#include "rtc_base/string_utils.h"
#include "rtc_base/win32.h"

#define GET_CURRENT_DIR _getcwd
#else
#include <dirent.h>

#define GET_CURRENT_DIR getcwd
#endif

#include <sys/stat.h>  // To check for directory existence.
#ifndef S_ISDIR        // Not defined in stat.h on Windows.
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#if defined(WEBRTC_IOS)
#include "test/testsupport/ios_file_utils.h"
#elif defined(WEBRTC_MAC)
#include "test/testsupport/mac_file_utils.h"
#endif

namespace webrtc {
namespace test {

#if defined(WEBRTC_WIN)
ABSL_CONST_INIT const absl::string_view kPathDelimiter = "\\";
#else
ABSL_CONST_INIT const absl::string_view kPathDelimiter = "/";
#endif

std::string DirName(absl::string_view path) {
  if (path.empty())
    return "";
  if (path == kPathDelimiter)
    return std::string(path);

  if (path.back() == kPathDelimiter[0])
    path.remove_suffix(1);  // Remove trailing separator.

  return std::string(path.substr(0, path.find_last_of(kPathDelimiter)));
}

absl::string_view FileName(absl::string_view path) {
  if (path.find_last_of(kPathDelimiter) == absl::string_view::npos) {
    return path;
  }
  return path.substr(path.find_last_of(kPathDelimiter) + 1);
}

bool FileExists(absl::string_view file_name) {
  struct stat file_info = {.st_dev = 0};
  return stat(std::string(file_name).c_str(), &file_info) == 0;
}

bool DirExists(absl::string_view directory_name) {
  struct stat directory_info = {.st_dev = 0};
  return stat(std::string(directory_name).c_str(), &directory_info) == 0 &&
         S_ISDIR(directory_info.st_mode);
}

std::string OutputPath() {
  return test::internal::OutputPath();
}

std::string OutputPathWithRandomDirectory() {
  std::string path = test::internal::OutputPath();
  std::string rand_dir = path + CreateRandomUuid();
  RTC_CHECK(CreateDir(rand_dir)) << "Failed to create dir: " << rand_dir;
  return rand_dir + std::string(kPathDelimiter);
}

std::string WorkingDir() {
  return test::internal::WorkingDir();
}

// Generate a temporary filename in a safe way.
// Largely copied from talk/base/{unixfilesystem,win32filesystem}.cc.
std::string TempFilename(absl::string_view dir, absl::string_view prefix) {
#ifdef WIN32
  wchar_t filename[MAX_PATH];
  if (::GetTempFileNameW(webrtc::ToUtf16(dir).c_str(),
                         webrtc::ToUtf16(prefix).c_str(), 0, filename) != 0)
    return webrtc::ToUtf8(filename);
  RTC_DCHECK_NOTREACHED();
  return "";
#else
  StringBuilder os;
  os << dir << "/" << prefix << "XXXXXX";
  std::string tempname = os.Release();

  int fd = ::mkstemp(tempname.data());
  if (fd == -1) {
    RTC_DCHECK_NOTREACHED();
    return "";
  } else {
    ::close(fd);
  }
  return tempname;
#endif
}

std::string GenerateTempFilename(absl::string_view dir,
                                 absl::string_view prefix) {
  std::string filename = TempFilename(dir, prefix);
  RemoveFile(filename);
  return filename;
}

std::optional<std::vector<std::string>> ReadDirectory(absl::string_view path) {
  if (path.empty())
    return std::optional<std::vector<std::string>>();

  std::string path_str(path);

#if defined(WEBRTC_WIN)
  // Append separator character if needed.
  if (path_str.back() != '\\')
    path_str += '\\';

  // Init.
  WIN32_FIND_DATAW data;
  HANDLE handle =
      ::FindFirstFileW(webrtc::ToUtf16(path_str + '*').c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE)
    return std::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  do {
    const std::string name = webrtc::ToUtf8(data.cFileName);
    if (name != "." && name != "..")
      found_entries.emplace_back(path_str + name);
  } while (::FindNextFileW(handle, &data) == TRUE);

  // Release resources.
  if (handle != INVALID_HANDLE_VALUE)
    ::FindClose(handle);
#else
  // Append separator character if needed.
  if (path_str.back() != '/')
    path_str += '/';

  // Init.
  DIR* dir = ::opendir(path_str.c_str());
  if (dir == nullptr)
    return std::optional<std::vector<std::string>>();

  // Populate output.
  std::vector<std::string> found_entries;
  while (dirent* dirent = readdir(dir)) {
    const std::string& name = dirent->d_name;
    if (name != "." && name != "..")
      found_entries.emplace_back(path_str + name);
  }

  // Release resources.
  closedir(dir);
#endif

  return std::optional<std::vector<std::string>>(std::move(found_entries));
}

bool CreateDir(absl::string_view directory_name) {
  std::string directory_name_str(directory_name);
  struct stat path_info = {.st_dev = 0};
  // Check if the path exists already:
  if (stat(directory_name_str.c_str(), &path_info) == 0) {
    if (!S_ISDIR(path_info.st_mode)) {
      fprintf(stderr,
              "Path %s exists but is not a directory! Remove this "
              "file and re-run to create the directory.\n",
              directory_name_str.c_str());
      return false;
    }
  } else {
#ifdef WIN32
    return _mkdir(directory_name_str.c_str()) == 0;
#else
    return mkdir(directory_name_str.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == 0;
#endif
  }
  return true;
}

bool RemoveDir(absl::string_view directory_name) {
#ifdef WIN32
  return RemoveDirectoryA(std::string(directory_name).c_str()) != FALSE;
#else
  return rmdir(std::string(directory_name).c_str()) == 0;
#endif
}

bool RemoveNonEmptyDir(absl::string_view directory_name) {
  std::optional<std::vector<std::string>> dir_content =
      ReadDirectory(directory_name);
  if (dir_content.has_value()) {
    for (const std::string& entry : *dir_content) {
      if (DirExists(entry)) {
        if (!RemoveNonEmptyDir(entry)) {
          return false;
        }
      } else if (FileExists(entry)) {
        if (!RemoveFile(entry)) {
          return false;
        }
      }
    }
  }
  // Directory should be emptied.
  return RemoveDir(directory_name);
}

bool RemoveFile(absl::string_view file_name) {
#ifdef WIN32
  return DeleteFileA(std::string(file_name).c_str()) != FALSE;
#else
  return unlink(std::string(file_name).c_str()) == 0;
#endif
}

std::string ResourcePath(absl::string_view name, absl::string_view extension) {
  return test::internal::ResourcePath(name, extension);
}

std::string JoinFilename(absl::string_view dir, absl::string_view name) {
  RTC_CHECK(!dir.empty()) << "Special cases not implemented.";
  StringBuilder os;
  os << dir;
  // If the directory path already ends with a path delimiter don't append it
  if (dir.back() != kPathDelimiter.back()) {
    os << kPathDelimiter;
  }
  os << name;
  return os.Release();
}

size_t GetFileSize(absl::string_view filename) {
  FILE* f = fopen(std::string(filename).c_str(), "rb");
  size_t size = 0;
  if (f != nullptr) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

}  // namespace test
}  // namespace webrtc
