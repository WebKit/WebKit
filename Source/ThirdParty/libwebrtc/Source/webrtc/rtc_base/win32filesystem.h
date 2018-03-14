/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_WIN32FILESYSTEM_H_
#define RTC_BASE_WIN32FILESYSTEM_H_

#include "fileutils.h"

namespace rtc {

class Win32Filesystem : public FilesystemInterface {
 public:
  // This will attempt to delete the path located at filename.
  // If the path points to a folder, it will fail with VERIFY
  bool DeleteFile(const Pathname& filename) override;

  // This moves a file from old_path to new_path. If the new path is on a
  // different volume than the old, it will attempt to copy and then delete
  // the folder
  // Returns true if the file is successfully moved
  bool MoveFile(const Pathname& old_path, const Pathname& new_path) override;

  // Returns true if a pathname is a directory
  bool IsFolder(const Pathname& pathname) override;

  // Returns true if a file exists at path
  bool IsFile(const Pathname& path) override;

  bool GetFileSize(const Pathname& path, size_t* size) override;
};

}  // namespace rtc

#endif  // RTC_BASE_WIN32FILESYSTEM_H_
