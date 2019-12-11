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

#include <io.h>
#include <windows.h>

#include <limits>

#include "rtc_base/checks.h"

namespace rtc {

size_t File::Write(const uint8_t* data, size_t length) {
  RTC_DCHECK_LT(length, std::numeric_limits<DWORD>::max());
  size_t total_written = 0;
  do {
    DWORD written;
    if (!::WriteFile(file_, data + total_written,
                     static_cast<DWORD>(length - total_written), &written,
                     nullptr)) {
      break;
    }
    total_written += written;
  } while (total_written < length);
  return total_written;
}

size_t File::Read(uint8_t* buffer, size_t length) {
  RTC_DCHECK_LT(length, std::numeric_limits<DWORD>::max());
  size_t total_read = 0;
  do {
    DWORD read;
    if (!::ReadFile(file_, buffer + total_read,
                    static_cast<DWORD>(length - total_read), &read, nullptr)) {
      break;
    }
    total_read += read;
  } while (total_read < length);
  return total_read;
}

size_t File::WriteAt(const uint8_t* data, size_t length, size_t offset) {
  RTC_DCHECK_LT(length, std::numeric_limits<DWORD>::max());
  size_t total_written = 0;
  do {
    DWORD written;

    LARGE_INTEGER offset_li;
    offset_li.QuadPart = offset + total_written;

    OVERLAPPED overlapped = {0};
    overlapped.Offset = offset_li.LowPart;
    overlapped.OffsetHigh = offset_li.HighPart;

    if (!::WriteFile(file_, data + total_written,
                     static_cast<DWORD>(length - total_written), &written,
                     &overlapped)) {
      break;
    }

    total_written += written;
  } while (total_written < length);
  return total_written;
}

size_t File::ReadAt(uint8_t* buffer, size_t length, size_t offset) {
  RTC_DCHECK_LT(length, std::numeric_limits<DWORD>::max());
  size_t total_read = 0;
  do {
    DWORD read;

    LARGE_INTEGER offset_li;
    offset_li.QuadPart = offset + total_read;

    OVERLAPPED overlapped = {0};
    overlapped.Offset = offset_li.LowPart;
    overlapped.OffsetHigh = offset_li.HighPart;

    if (!::ReadFile(file_, buffer + total_read,
                    static_cast<DWORD>(length - total_read), &read,
                    &overlapped)) {
      break;
    }

    total_read += read;
  } while (total_read < length);
  return total_read;
}

bool File::Seek(size_t offset) {
  LARGE_INTEGER distance;
  distance.QuadPart = offset;
  return SetFilePointerEx(file_, distance, nullptr, FILE_BEGIN) != 0;
}

bool File::Close() {
  if (file_ == kInvalidPlatformFileValue)
    return false;
  bool ret = CloseHandle(file_) != 0;
  file_ = kInvalidPlatformFileValue;
  return ret;
}

}  // namespace rtc
