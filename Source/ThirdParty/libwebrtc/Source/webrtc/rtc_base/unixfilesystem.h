/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_UNIXFILESYSTEM_H_
#define RTC_BASE_UNIXFILESYSTEM_H_

#include <sys/types.h>

#include "rtc_base/fileutils.h"

namespace rtc {

class UnixFilesystem : public FilesystemInterface {
 public:
  UnixFilesystem();
  ~UnixFilesystem() override;

  // This will attempt to delete the file located at filename.
  // It will fail with VERIY if you pass it a non-existant file, or a directory.
  bool DeleteFile(const Pathname& filename) override;

  // This moves a file from old_path to new_path, where "file" can be a plain
  // file or directory, which will be moved recursively.
  // Returns true if function succeeds.
  bool MoveFile(const Pathname& old_path, const Pathname& new_path) override;

  // Returns true if a pathname is a directory
  bool IsFolder(const Pathname& pathname) override;

  // Returns true of pathname represents an existing file
  bool IsFile(const Pathname& pathname) override;

  bool GetFileSize(const Pathname& path, size_t* size) override;
};

}  // namespace rtc

#endif  // RTC_BASE_UNIXFILESYSTEM_H_
