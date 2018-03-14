/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FILEUTILS_H_
#define RTC_BASE_FILEUTILS_H_

#include <string>

#if defined(WEBRTC_WIN)
#include <windows.h>
#else
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // WEBRTC_WIN

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/platform_file.h"

namespace rtc {

class FileStream;
class Pathname;

//////////////////////////
// Directory Iterator   //
//////////////////////////

// A DirectoryIterator is created with a given directory. It originally points
// to the first file in the directory, and can be advanecd with Next(). This
// allows you to get information about each file.

class DirectoryIterator {
  friend class Filesystem;
 public:
  // Constructor
  DirectoryIterator();
  // Destructor
  virtual ~DirectoryIterator();

  // Starts traversing a directory
  // dir is the directory to traverse
  // returns true if the directory exists and is valid
  // The iterator will point to the first entry in the directory
  virtual bool Iterate(const Pathname &path);

  // Advances to the next file
  // returns true if there were more files in the directory.
  virtual bool Next();

  // returns true if the file currently pointed to is a directory
  virtual bool IsDirectory() const;

  // returns the name of the file currently pointed to
  virtual std::string Name() const;

 private:
  std::string directory_;
#if defined(WEBRTC_WIN)
  WIN32_FIND_DATA data_;
  HANDLE handle_;
#else
  DIR *dir_;
  struct dirent *dirent_;
  struct stat stat_;
#endif
};

class FilesystemInterface {
 public:
  virtual ~FilesystemInterface() {}

  // This will attempt to delete the path located at filename.
  // It DCHECKs and returns false if the path points to a folder or a
  // non-existent file.
  virtual bool DeleteFile(const Pathname &filename) = 0;

  // This moves a file from old_path to new_path, where "old_path" is a
  // plain file. This DCHECKs and returns false if old_path points to a
  // directory, and returns true if the function succeeds.
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path) = 0;

  // Returns true if pathname refers to a directory
  virtual bool IsFolder(const Pathname& pathname) = 0;

  // Returns true if pathname refers to a file
  virtual bool IsFile(const Pathname& pathname) = 0;

  // Determines the size of the file indicated by path.
  virtual bool GetFileSize(const Pathname& path, size_t* size) = 0;
};

class Filesystem {
 public:
  static FilesystemInterface *default_filesystem() {
    RTC_DCHECK(default_filesystem_);
    return default_filesystem_;
  }

  static void set_default_filesystem(FilesystemInterface *filesystem) {
    default_filesystem_ = filesystem;
  }

  static FilesystemInterface *swap_default_filesystem(
      FilesystemInterface *filesystem) {
    FilesystemInterface *cur = default_filesystem_;
    default_filesystem_ = filesystem;
    return cur;
  }

  static bool DeleteFile(const Pathname &filename) {
    return EnsureDefaultFilesystem()->DeleteFile(filename);
  }

  static bool MoveFile(const Pathname &old_path, const Pathname &new_path) {
    return EnsureDefaultFilesystem()->MoveFile(old_path, new_path);
  }

  static bool IsFolder(const Pathname& pathname) {
    return EnsureDefaultFilesystem()->IsFolder(pathname);
  }

  static bool IsFile(const Pathname &pathname) {
    return EnsureDefaultFilesystem()->IsFile(pathname);
  }

  static bool GetFileSize(const Pathname& path, size_t* size) {
    return EnsureDefaultFilesystem()->GetFileSize(path, size);
  }

 private:
  static FilesystemInterface* default_filesystem_;

  static FilesystemInterface *EnsureDefaultFilesystem();
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Filesystem);
};

}  // namespace rtc

#endif  // RTC_BASE_FILEUTILS_H_
