/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/file.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>

#include "rtc_base/checks.h"

namespace rtc {

size_t File::Write(const uint8_t* data, size_t length) {
  size_t total_written = 0;
  do {
    ssize_t written;
    do {
      written = ::write(file_, data + total_written, length - total_written);
    } while (written == -1 && errno == EINTR);
    if (written == -1)
      break;
    total_written += written;
  } while (total_written < length);
  return total_written;
}

size_t File::Read(uint8_t* buffer, size_t length) {
  size_t total_read = 0;
  do {
    ssize_t read;
    do {
      read = ::read(file_, buffer + total_read, length - total_read);
    } while (read == -1 && errno == EINTR);
    if (read == -1)
      break;
    total_read += read;
  } while (total_read < length);
  return total_read;
}

size_t File::WriteAt(const uint8_t* data, size_t length, size_t offset) {
  size_t total_written = 0;
  do {
    ssize_t written;
    do {
      written = ::pwrite(file_, data + total_written, length - total_written,
                         offset + total_written);
    } while (written == -1 && errno == EINTR);
    if (written == -1)
      break;
    total_written += written;
  } while (total_written < length);
  return total_written;
}

size_t File::ReadAt(uint8_t* buffer, size_t length, size_t offset) {
  size_t total_read = 0;
  do {
    ssize_t read;
    do {
      read = ::pread(file_, buffer + total_read, length - total_read,
                     offset + total_read);
    } while (read == -1 && errno == EINTR);
    if (read == -1)
      break;
    total_read += read;
  } while (total_read < length);
  return total_read;
}

bool File::Seek(size_t offset) {
  RTC_DCHECK_LE(offset, std::numeric_limits<off_t>::max());
  return lseek(file_, static_cast<off_t>(offset), SEEK_SET) != -1;
}

bool File::Close() {
  if (file_ == rtc::kInvalidPlatformFileValue)
    return false;
  bool ret = close(file_) == 0;
  file_ = rtc::kInvalidPlatformFileValue;
  return ret;
}

}  // namespace rtc
