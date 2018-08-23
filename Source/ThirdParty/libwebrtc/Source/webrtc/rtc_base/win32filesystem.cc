/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/win32filesystem.h"

#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>
#include "rtc_base/win32.h"

#include <memory>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/fileutils.h"
#include "rtc_base/pathutils.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringutils.h"

// In several places in this file, we test the integrity level of the process
// before calling GetLongPathName. We do this because calling GetLongPathName
// when running under protected mode IE (a low integrity process) can result in
// a virtualized path being returned, which is wrong if you only plan to read.
// TODO: Waiting to hear back from IE team on whether this is the
// best approach; IEIsProtectedModeProcess is another possible solution.

namespace rtc {

bool Win32Filesystem::DeleteFile(const Pathname& filename) {
  RTC_LOG(LS_INFO) << "Deleting file " << filename.pathname();
  if (!IsFile(filename)) {
    RTC_DCHECK(IsFile(filename));
    return false;
  }
  return ::DeleteFile(ToUtf16(filename.pathname()).c_str()) != 0;
}

bool Win32Filesystem::MoveFile(const Pathname& old_path,
                               const Pathname& new_path) {
  if (!IsFile(old_path)) {
    RTC_DCHECK(IsFile(old_path));
    return false;
  }
  RTC_LOG(LS_INFO) << "Moving " << old_path.pathname() << " to "
                   << new_path.pathname();
  return ::MoveFile(ToUtf16(old_path.pathname()).c_str(),
                    ToUtf16(new_path.pathname()).c_str()) != 0;
}

bool Win32Filesystem::IsFolder(const Pathname& path) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (0 == ::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                                 GetFileExInfoStandard, &data))
    return false;
  return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
         FILE_ATTRIBUTE_DIRECTORY;
}

bool Win32Filesystem::IsFile(const Pathname& path) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (0 == ::GetFileAttributesEx(ToUtf16(path.pathname()).c_str(),
                                 GetFileExInfoStandard, &data))
    return false;
  return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool Win32Filesystem::GetFileSize(const Pathname& pathname, size_t* size) {
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (::GetFileAttributesEx(ToUtf16(pathname.pathname()).c_str(),
                            GetFileExInfoStandard, &data) == 0)
    return false;
  *size = data.nFileSizeLow;
  return true;
}

}  // namespace rtc
