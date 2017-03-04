/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FILE_H_
#define WEBRTC_BASE_FILE_H_

#include <stdint.h>

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/platform_file.h"

namespace rtc {

// This class wraps the platform specific APIs for simple file interactions.
//
// The various read and write methods are best effort, i.e. if an underlying
// call does not manage to read/write all the data more calls will be performed,
// until an error is detected or all data is read/written.
class File {
 public:
  // Wraps the given PlatformFile. This class is then responsible for closing
  // the file, which will be done in the destructor if Close is never called.
  explicit File(PlatformFile);
  // The default constructor produces a closed file.
  File();
  ~File();

  File(File&& other);
  File& operator=(File&& other);

  // Open and Create give files with both reading and writing enabled.
  static File Open(const std::string& path);
  static File Open(Pathname&& path);
  static File Open(const Pathname& path);
  // If the file already exists it will be overwritten.
  static File Create(const std::string& path);
  static File Create(Pathname&& path);
  static File Create(const Pathname& path);

  // Remove a file in the file system.
  static bool Remove(const std::string& path);
  static bool Remove(Pathname&& path);
  static bool Remove(const Pathname& path);

  size_t Write(const uint8_t* data, size_t length);
  size_t Read(uint8_t* buffer, size_t length);

  // The current position in the file after a call to these methods is platform
  // dependent (MSVC gives position offset+length, most other
  // compilers/platforms do not alter the position), i.e. do not depend on it,
  // do a Seek before any subsequent Read/Write.
  size_t WriteAt(const uint8_t* data, size_t length, size_t offset);
  size_t ReadAt(uint8_t* buffer, size_t length, size_t offset);

  // Attempt to position the file at the given offset from the start.
  // Returns true if successful, false otherwise.
  bool Seek(size_t offset);

  // Attempt to close the file. Returns true if successful, false otherwise,
  // most notably when the file is already closed.
  bool Close();

  bool IsOpen();

 private:
  PlatformFile file_;
  RTC_DISALLOW_COPY_AND_ASSIGN(File);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FILE_H_
