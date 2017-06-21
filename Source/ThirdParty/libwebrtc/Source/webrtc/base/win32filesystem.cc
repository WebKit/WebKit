/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/win32filesystem.h"

#include "webrtc/base/win32.h"
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>

#include <memory>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringutils.h"

// In several places in this file, we test the integrity level of the process
// before calling GetLongPathName. We do this because calling GetLongPathName
// when running under protected mode IE (a low integrity process) can result in
// a virtualized path being returned, which is wrong if you only plan to read.
// TODO: Waiting to hear back from IE team on whether this is the
// best approach; IEIsProtectedModeProcess is another possible solution.

namespace rtc {

bool Win32Filesystem::CreateFolder(const Pathname &pathname) {
  if (pathname.pathname().empty() || !pathname.filename().empty())
    return false;

  std::wstring path16;
  if (!Utf8ToWindowsFilename(pathname.pathname(), &path16))
    return false;

  DWORD res = ::GetFileAttributes(path16.c_str());
  if (res != INVALID_FILE_ATTRIBUTES) {
    // Something exists at this location, check if it is a directory
    return ((res & FILE_ATTRIBUTE_DIRECTORY) != 0);
  } else if ((GetLastError() != ERROR_FILE_NOT_FOUND)
              && (GetLastError() != ERROR_PATH_NOT_FOUND)) {
    // Unexpected error
    return false;
  }

  // Directory doesn't exist, look up one directory level
  if (!pathname.parent_folder().empty()) {
    Pathname parent(pathname);
    parent.SetFolder(pathname.parent_folder());
    if (!CreateFolder(parent)) {
      return false;
    }
  }

  return (::CreateDirectory(path16.c_str(), nullptr) != 0);
}

bool Win32Filesystem::DeleteFile(const Pathname &filename) {
  LOG(LS_INFO) << "Deleting file " << filename.pathname();
  if (!IsFile(filename)) {
    RTC_DCHECK(IsFile(filename));
    return false;
  }
  return ::DeleteFile(ToUtf16(filename.pathname()).c_str()) != 0;
}

bool Win32Filesystem::GetTemporaryFolder(Pathname &pathname, bool create,
                                         const std::string *append) {
  wchar_t buffer[MAX_PATH + 1];
  if (!::GetTempPath(arraysize(buffer), buffer))
    return false;
  if (!IsCurrentProcessLowIntegrity() &&
      !::GetLongPathName(buffer, buffer, arraysize(buffer)))
    return false;
  size_t len = strlen(buffer);
  if ((len > 0) && (buffer[len-1] != '\\')) {
    len += strcpyn(buffer + len, arraysize(buffer) - len, L"\\");
  }
  if (len >= arraysize(buffer) - 1)
    return false;
  pathname.clear();
  pathname.SetFolder(ToUtf8(buffer));
  if (append != nullptr) {
    RTC_DCHECK(!append->empty());
    pathname.AppendFolder(*append);
  }
  return !create || CreateFolder(pathname);
}

std::string Win32Filesystem::TempFilename(const Pathname &dir,
                                          const std::string &prefix) {
  wchar_t filename[MAX_PATH];
  if (::GetTempFileName(ToUtf16(dir.pathname()).c_str(),
                        ToUtf16(prefix).c_str(), 0, filename) != 0)
    return ToUtf8(filename);
  RTC_NOTREACHED();
  return "";
}

bool Win32Filesystem::MoveFile(const Pathname &old_path,
                               const Pathname &new_path) {
  if (!IsFile(old_path)) {
    RTC_DCHECK(IsFile(old_path));
    return false;
  }
  LOG(LS_INFO) << "Moving " << old_path.pathname()
               << " to " << new_path.pathname();
  return ::MoveFile(ToUtf16(old_path.pathname()).c_str(),
                    ToUtf16(new_path.pathname()).c_str()) != 0;
}

bool Win32Filesystem::IsFolder(const Pathname &path) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (0 == ::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                                 GetFileExInfoStandard, &data))
    return false;
  return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
      FILE_ATTRIBUTE_DIRECTORY;
}

bool Win32Filesystem::IsFile(const Pathname &path) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (0 == ::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                                 GetFileExInfoStandard, &data))
    return false;
  return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool Win32Filesystem::IsAbsent(const Pathname& path) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (0 != ::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                                 GetFileExInfoStandard, &data))
    return false;
  DWORD err = ::GetLastError();
  return (ERROR_FILE_NOT_FOUND == err || ERROR_PATH_NOT_FOUND == err);
}

bool Win32Filesystem::GetFileSize(const Pathname &pathname, size_t *size) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (::GetFileAttributesEx(ToUtf16(pathname.pathname()).c_str(),
                            GetFileExInfoStandard, &data) == 0)
  return false;
  *size = data.nFileSizeLow;
  return true;
}

}  // namespace rtc
