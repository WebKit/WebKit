/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/unixfilesystem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#include <IOKit/IOCFBundle.h>
#include <sys/statvfs.h>
#include "webrtc/base/macutils.h"
#endif  // WEBRTC_MAC && !defined(WEBRTC_IOS)

#if defined(WEBRTC_POSIX) && !defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include <sys/types.h>
#if defined(WEBRTC_ANDROID)
#include <sys/statfs.h>
#elif !defined(__native_client__)
#include <sys/statvfs.h>
#endif  //  !defined(__native_client__)
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#endif  // WEBRTC_POSIX && !WEBRTC_MAC || WEBRTC_IOS

#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
#include <ctype.h>
#include <algorithm>
#endif

#if defined(__native_client__) && !defined(__GLIBC__)
#include <sys/syslimits.h>
#endif

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringutils.h"

#if defined(WEBRTC_MAC)
// Defined in applefilesystem.mm.  No header file to discourage use
// elsewhere; other places should use GetApp{Data,Temp}Folder() in
// this file.  Don't copy/paste.  I mean it.
char* AppleDataDirectory();
char* AppleTempDirectory();
void AppleAppName(rtc::Pathname* path);
#endif

namespace rtc {

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_MAC)
char* UnixFilesystem::app_temp_path_ = nullptr;
#else
char* UnixFilesystem::provided_app_data_folder_ = nullptr;
char* UnixFilesystem::provided_app_temp_folder_ = nullptr;

void UnixFilesystem::SetAppDataFolder(const std::string& folder) {
  delete [] provided_app_data_folder_;
  provided_app_data_folder_ = CopyString(folder);
}

void UnixFilesystem::SetAppTempFolder(const std::string& folder) {
  delete [] provided_app_temp_folder_;
  provided_app_temp_folder_ = CopyString(folder);
}
#endif

UnixFilesystem::UnixFilesystem() {
#if defined(WEBRTC_MAC)
  if (!provided_app_data_folder_)
    provided_app_data_folder_ = AppleDataDirectory();
  if (!provided_app_temp_folder_)
    provided_app_temp_folder_ = AppleTempDirectory();
#endif
}

UnixFilesystem::~UnixFilesystem() {}

bool UnixFilesystem::CreateFolder(const Pathname &path, mode_t mode) {
  std::string pathname(path.pathname());
  int len = pathname.length();
  if ((len == 0) || (pathname[len - 1] != '/'))
    return false;

  struct stat st;
  int res = ::stat(pathname.c_str(), &st);
  if (res == 0) {
    // Something exists at this location, check if it is a directory
    return S_ISDIR(st.st_mode) != 0;
  } else if (errno != ENOENT) {
    // Unexpected error
    return false;
  }

  // Directory doesn't exist, look up one directory level
  do {
    --len;
  } while ((len > 0) && (pathname[len - 1] != '/'));

  if (!CreateFolder(Pathname(pathname.substr(0, len)), mode)) {
    return false;
  }

  LOG(LS_INFO) << "Creating folder: " << pathname;
  return (0 == ::mkdir(pathname.c_str(), mode));
}

bool UnixFilesystem::CreateFolder(const Pathname &path) {
  return CreateFolder(path, 0755);
}

bool UnixFilesystem::DeleteFile(const Pathname &filename) {
  LOG(LS_INFO) << "Deleting file:" << filename.pathname();

  if (!IsFile(filename)) {
    RTC_DCHECK(IsFile(filename));
    return false;
  }
  return ::unlink(filename.pathname().c_str()) == 0;
}

bool UnixFilesystem::GetTemporaryFolder(Pathname &pathname, bool create,
                                        const std::string *append) {
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_MAC)
  RTC_DCHECK(provided_app_temp_folder_ != nullptr);
  pathname.SetPathname(provided_app_temp_folder_, "");
#else
  if (const char* tmpdir = getenv("TMPDIR")) {
    pathname.SetPathname(tmpdir, "");
  } else if (const char* tmp = getenv("TMP")) {
    pathname.SetPathname(tmp, "");
  } else {
#ifdef P_tmpdir
    pathname.SetPathname(P_tmpdir, "");
#else  // !P_tmpdir
    pathname.SetPathname("/tmp/", "");
#endif  // !P_tmpdir
  }
#endif  // defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  if (append) {
    RTC_DCHECK(!append->empty());
    pathname.AppendFolder(*append);
  }
  return !create || CreateFolder(pathname);
}

std::string UnixFilesystem::TempFilename(const Pathname &dir,
                                         const std::string &prefix) {
  int len = dir.pathname().size() + prefix.size() + 2 + 6;
  char *tempname = new char[len];

  snprintf(tempname, len, "%s/%sXXXXXX", dir.pathname().c_str(),
           prefix.c_str());
  int fd = ::mkstemp(tempname);
  if (fd != -1)
    ::close(fd);
  std::string ret(tempname);
  delete[] tempname;

  return ret;
}

bool UnixFilesystem::MoveFile(const Pathname &old_path,
                              const Pathname &new_path) {
  if (!IsFile(old_path)) {
    RTC_DCHECK(IsFile(old_path));
    return false;
  }
  LOG(LS_VERBOSE) << "Moving " << old_path.pathname()
                  << " to " << new_path.pathname();
  if (rename(old_path.pathname().c_str(), new_path.pathname().c_str()) != 0) {
    return false;
  }
  return true;
}

bool UnixFilesystem::IsFolder(const Pathname &path) {
  struct stat st;
  if (stat(path.pathname().c_str(), &st) < 0)
    return false;
  return S_ISDIR(st.st_mode);
}

bool UnixFilesystem::IsFile(const Pathname& pathname) {
  struct stat st;
  int res = ::stat(pathname.pathname().c_str(), &st);
  // Treat symlinks, named pipes, etc. all as files.
  return res == 0 && !S_ISDIR(st.st_mode);
}

bool UnixFilesystem::IsAbsent(const Pathname& pathname) {
  struct stat st;
  int res = ::stat(pathname.pathname().c_str(), &st);
  // Note: we specifically maintain ENOTDIR as an error, because that implies
  // that you could not call CreateFolder(pathname).
  return res != 0 && ENOENT == errno;
}

bool UnixFilesystem::GetFileSize(const Pathname& pathname, size_t *size) {
  struct stat st;
  if (::stat(pathname.pathname().c_str(), &st) != 0)
    return false;
  *size = st.st_size;
  return true;
}

char* UnixFilesystem::CopyString(const std::string& str) {
  size_t size = str.length() + 1;

  char* buf = new char[size];
  if (!buf) {
    return nullptr;
  }

  strcpyn(buf, size, str.c_str());
  return buf;
}

}  // namespace rtc

#if defined(__native_client__)
extern "C" int __attribute__((weak))
link(const char* oldpath, const char* newpath) {
  errno = EACCES;
  return -1;
}
#endif
