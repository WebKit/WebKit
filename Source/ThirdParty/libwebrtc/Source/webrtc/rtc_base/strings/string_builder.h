/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGS_STRING_BUILDER_H_
#define RTC_BASE_STRINGS_STRING_BUILDER_H_

#include <cstdio>
#include <cstring>
#include <string>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/stringutils.h"

namespace rtc {

// This is a minimalistic string builder class meant to cover the most cases of
// when you might otherwise be tempted to use a stringstream (discouraged for
// anything except logging). It uses a fixed-size buffer provided by the caller
// and concatenates strings and numbers into it, allowing the results to be
// read via |str()|.
class SimpleStringBuilder {
 public:
  explicit SimpleStringBuilder(rtc::ArrayView<char> buffer);
  SimpleStringBuilder(const SimpleStringBuilder&) = delete;
  SimpleStringBuilder& operator=(const SimpleStringBuilder&) = delete;

  SimpleStringBuilder& operator<<(const char* str);
  SimpleStringBuilder& operator<<(char ch);
  SimpleStringBuilder& operator<<(const std::string& str);
  SimpleStringBuilder& operator<<(int i);
  SimpleStringBuilder& operator<<(unsigned i);
  SimpleStringBuilder& operator<<(long i);                // NOLINT
  SimpleStringBuilder& operator<<(long long i);           // NOLINT
  SimpleStringBuilder& operator<<(unsigned long i);       // NOLINT
  SimpleStringBuilder& operator<<(unsigned long long i);  // NOLINT
  SimpleStringBuilder& operator<<(float f);
  SimpleStringBuilder& operator<<(double f);
  SimpleStringBuilder& operator<<(long double f);

  // Returns a pointer to the built string. The name |str()| is borrowed for
  // compatibility reasons as we replace usage of stringstream throughout the
  // code base.
  const char* str() const { return buffer_.data(); }

  // Returns the length of the string. The name |size()| is picked for STL
  // compatibility reasons.
  size_t size() const { return size_; }

// Allows appending a printf style formatted string.
#if defined(__GNUC__)
  __attribute__((__format__(__printf__, 2, 3)))
#endif
  SimpleStringBuilder&
  AppendFormat(const char* fmt, ...);

  // An alternate way from operator<<() to append a string. This variant is
  // slightly more efficient when the length of the string to append, is known.
  SimpleStringBuilder& Append(const char* str, size_t length = SIZE_UNKNOWN);

 private:
  bool IsConsistent() const {
    return size_ <= buffer_.size() - 1 && buffer_[size_] == '\0';
  }

  // An always-zero-terminated fixed-size buffer that we write to. The fixed
  // size allows the buffer to be stack allocated, which helps performance.
  // Having a fixed size is furthermore useful to avoid unnecessary resizing
  // while building it.
  const rtc::ArrayView<char> buffer_;

  // Represents the number of characters written to the buffer.
  // This does not include the terminating '\0'.
  size_t size_ = 0;
};

}  // namespace rtc

#endif  // RTC_BASE_STRINGS_STRING_BUILDER_H_
