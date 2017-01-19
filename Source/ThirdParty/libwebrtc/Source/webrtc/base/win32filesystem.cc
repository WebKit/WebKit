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

  return (::CreateDirectory(path16.c_str(), NULL) != 0);
}

FileStream *Win32Filesystem::OpenFile(const Pathname &filename,
                                      const std::string &mode) {
  FileStream *fs = new FileStream();
  if (fs && !fs->Open(filename.pathname().c_str(), mode.c_str(), NULL)) {
    delete fs;
    fs = NULL;
  }
  return fs;
}

bool Win32Filesystem::DeleteFile(const Pathname &filename) {
  LOG(LS_INFO) << "Deleting file " << filename.pathname();
  if (!IsFile(filename)) {
    ASSERT(IsFile(filename));
    return false;
  }
  return ::DeleteFile(ToUtf16(filename.pathname()).c_str()) != 0;
}

bool Win32Filesystem::DeleteEmptyFolder(const Pathname &folder) {
  LOG(LS_INFO) << "Deleting folder " << folder.pathname();

  std::string no_slash(folder.pathname(), 0, folder.pathname().length()-1);
  return ::RemoveDirectory(ToUtf16(no_slash).c_str()) != 0;
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
  if (append != NULL) {
    ASSERT(!append->empty());
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
  ASSERT(false);
  return "";
}

bool Win32Filesystem::MoveFile(const Pathname &old_path,
                               const Pathname &new_path) {
  if (!IsFile(old_path)) {
    ASSERT(IsFile(old_path));
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

bool Win32Filesystem::CopyFile(const Pathname &old_path,
                               const Pathname &new_path) {
  return ::CopyFile(ToUtf16(old_path.pathname()).c_str(),
                    ToUtf16(new_path.pathname()).c_str(), TRUE) != 0;
}

bool Win32Filesystem::IsTemporaryPath(const Pathname& pathname) {
  TCHAR buffer[MAX_PATH + 1];
  if (!::GetTempPath(arraysize(buffer), buffer))
    return false;
  if (!IsCurrentProcessLowIntegrity() &&
      !::GetLongPathName(buffer, buffer, arraysize(buffer)))
    return false;
  return (::strnicmp(ToUtf16(pathname.pathname()).c_str(),
                     buffer, strlen(buffer)) == 0);
}

bool Win32Filesystem::GetFileSize(const Pathname &pathname, size_t *size) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (::GetFileAttributesEx(ToUtf16(pathname.pathname()).c_str(),
                            GetFileExInfoStandard, &data) == 0)
  return false;
  *size = data.nFileSizeLow;
  return true;
}

bool Win32Filesystem::GetFileTime(const Pathname& path, FileTimeType which,
                                  time_t* time) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                            GetFileExInfoStandard, &data) == 0)
    return false;
  switch (which) {
  case FTT_CREATED:
    FileTimeToUnixTime(data.ftCreationTime, time);
    break;
  case FTT_MODIFIED:
    FileTimeToUnixTime(data.ftLastWriteTime, time);
    break;
  case FTT_ACCESSED:
    FileTimeToUnixTime(data.ftLastAccessTime, time);
    break;
  default:
    return false;
  }
  return true;
}

bool Win32Filesystem::GetAppPathname(Pathname* path) {
  TCHAR buffer[MAX_PATH + 1];
  if (0 == ::GetModuleFileName(NULL, buffer, arraysize(buffer)))
    return false;
  path->SetPathname(ToUtf8(buffer));
  return true;
}

bool Win32Filesystem::GetAppDataFolder(Pathname* path, bool per_user) {
  ASSERT(!organization_name_.empty());
  ASSERT(!application_name_.empty());
  TCHAR buffer[MAX_PATH + 1];
  int csidl = per_user ? CSIDL_LOCAL_APPDATA : CSIDL_COMMON_APPDATA;
  if (!::SHGetSpecialFolderPath(NULL, buffer, csidl, TRUE))
    return false;
  if (!IsCurrentProcessLowIntegrity() &&
      !::GetLongPathName(buffer, buffer, arraysize(buffer)))
    return false;
  size_t len = strcatn(buffer, arraysize(buffer), __T("\\"));
  len += strcpyn(buffer + len, arraysize(buffer) - len,
                 ToUtf16(organization_name_).c_str());
  if ((len > 0) && (buffer[len-1] != __T('\\'))) {
    len += strcpyn(buffer + len, arraysize(buffer) - len, __T("\\"));
  }
  len += strcpyn(buffer + len, arraysize(buffer) - len,
                 ToUtf16(application_name_).c_str());
  if ((len > 0) && (buffer[len-1] != __T('\\'))) {
    len += strcpyn(buffer + len, arraysize(buffer) - len, __T("\\"));
  }
  if (len >= arraysize(buffer) - 1)
    return false;
  path->clear();
  path->SetFolder(ToUtf8(buffer));
  return CreateFolder(*path);
}

bool Win32Filesystem::GetAppTempFolder(Pathname* path) {
  if (!GetAppPathname(path))
    return false;
  std::string filename(path->filename());
  return GetTemporaryFolder(*path, true, &filename);
}

bool Win32Filesystem::GetDiskFreeSpace(const Pathname& path,
                                       int64_t* free_bytes) {
  if (!free_bytes) {
    return false;
  }
  char drive[4];
  std::wstring drive16;
  const wchar_t* target_drive = NULL;
  if (path.GetDrive(drive, sizeof(drive))) {
    drive16 = ToUtf16(drive);
    target_drive = drive16.c_str();
  } else if (path.folder().substr(0, 2) == "\\\\") {
    // UNC path, fail.
    // TODO: Handle UNC paths.
    return false;
  } else {
    // The path is probably relative.  GetDriveType and GetDiskFreeSpaceEx
    // use the current drive if NULL is passed as the drive name.
    // TODO: Add method to Pathname to determine if the path is relative.
    // TODO: Add method to Pathname to convert a path to absolute.
  }
  UINT drive_type = ::GetDriveType(target_drive);
  if ((drive_type == DRIVE_REMOTE) || (drive_type == DRIVE_UNKNOWN)) {
    LOG(LS_VERBOSE) << "Remote or unknown drive: " << drive;
    return false;
  }

  int64_t total_number_of_bytes;       // receives the number of bytes on disk
  int64_t total_number_of_free_bytes;  // receives the free bytes on disk
  // make sure things won't change in 64 bit machine
  // TODO replace with compile time assert
  ASSERT(sizeof(ULARGE_INTEGER) == sizeof(uint64_t));  // NOLINT
  if (::GetDiskFreeSpaceEx(target_drive,
                           (PULARGE_INTEGER)free_bytes,
                           (PULARGE_INTEGER)&total_number_of_bytes,
                           (PULARGE_INTEGER)&total_number_of_free_bytes)) {
    return true;
  } else {
    LOG(LS_VERBOSE) << "GetDiskFreeSpaceEx returns error.";
    return false;
  }
}

}  // namespace rtc
