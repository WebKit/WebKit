/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/string_encode.h"

#include <cstdio>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"

namespace rtc {

/////////////////////////////////////////////////////////////////////////////
// String Encoding Utilities
/////////////////////////////////////////////////////////////////////////////

namespace {
const char HEX[] = "0123456789abcdef";

// Convert an unsigned value from 0 to 15 to the hex character equivalent...
char hex_encode(unsigned char val) {
  RTC_DCHECK_LT(val, 16);
  return (val < 16) ? HEX[val] : '!';
}

// ...and vice-versa.
bool hex_decode(char ch, unsigned char* val) {
  if ((ch >= '0') && (ch <= '9')) {
    *val = ch - '0';
  } else if ((ch >= 'A') && (ch <= 'F')) {
    *val = (ch - 'A') + 10;
  } else if ((ch >= 'a') && (ch <= 'f')) {
    *val = (ch - 'a') + 10;
  } else {
    return false;
  }
  return true;
}

size_t hex_encode_output_length(size_t srclen, char delimiter) {
  return delimiter && srclen > 0 ? (srclen * 3 - 1) : (srclen * 2);
}

// hex_encode shows the hex representation of binary data in ascii, with
// `delimiter` between bytes, or none if `delimiter` == 0.
void hex_encode_with_delimiter(char* buffer,
                               const char* csource,
                               size_t srclen,
                               char delimiter) {
  RTC_DCHECK(buffer);

  // Init and check bounds.
  const unsigned char* bsource =
      reinterpret_cast<const unsigned char*>(csource);
  size_t srcpos = 0, bufpos = 0;

  while (srcpos < srclen) {
    unsigned char ch = bsource[srcpos++];
    buffer[bufpos] = hex_encode((ch >> 4) & 0xF);
    buffer[bufpos + 1] = hex_encode((ch)&0xF);
    bufpos += 2;

    // Don't write a delimiter after the last byte.
    if (delimiter && (srcpos < srclen)) {
      buffer[bufpos] = delimiter;
      ++bufpos;
    }
  }
}

}  // namespace

std::string hex_encode(const std::string& str) {
  return hex_encode(str.c_str(), str.size());
}

std::string hex_encode(const char* source, size_t srclen) {
  return hex_encode_with_delimiter(source, srclen, 0);
}

std::string hex_encode_with_delimiter(const char* source,
                                      size_t srclen,
                                      char delimiter) {
  std::string s(hex_encode_output_length(srclen, delimiter), 0);
  hex_encode_with_delimiter(&s[0], source, srclen, delimiter);
  return s;
}

size_t hex_decode(char* cbuffer,
                  size_t buflen,
                  const char* source,
                  size_t srclen) {
  return hex_decode_with_delimiter(cbuffer, buflen, source, srclen, 0);
}

size_t hex_decode_with_delimiter(char* cbuffer,
                                 size_t buflen,
                                 const char* source,
                                 size_t srclen,
                                 char delimiter) {
  RTC_DCHECK(cbuffer);  // TODO(kwiberg): estimate output size
  if (buflen == 0)
    return 0;

  // Init and bounds check.
  unsigned char* bbuffer = reinterpret_cast<unsigned char*>(cbuffer);
  size_t srcpos = 0, bufpos = 0;
  size_t needed = (delimiter) ? (srclen + 1) / 3 : srclen / 2;
  if (buflen < needed)
    return 0;

  while (srcpos < srclen) {
    if ((srclen - srcpos) < 2) {
      // This means we have an odd number of bytes.
      return 0;
    }

    unsigned char h1, h2;
    if (!hex_decode(source[srcpos], &h1) ||
        !hex_decode(source[srcpos + 1], &h2))
      return 0;

    bbuffer[bufpos++] = (h1 << 4) | h2;
    srcpos += 2;

    // Remove the delimiter if needed.
    if (delimiter && (srclen - srcpos) > 1) {
      if (source[srcpos] != delimiter)
        return 0;
      ++srcpos;
    }
  }

  return bufpos;
}

size_t hex_decode(char* buffer, size_t buflen, const std::string& source) {
  return hex_decode_with_delimiter(buffer, buflen, source, 0);
}
size_t hex_decode_with_delimiter(char* buffer,
                                 size_t buflen,
                                 const std::string& source,
                                 char delimiter) {
  return hex_decode_with_delimiter(buffer, buflen, source.c_str(),
                                   source.length(), delimiter);
}

size_t tokenize(absl::string_view source,
                char delimiter,
                std::vector<std::string>* fields) {
  fields->clear();
  size_t last = 0;
  for (size_t i = 0; i < source.length(); ++i) {
    if (source[i] == delimiter) {
      if (i != last) {
        fields->emplace_back(source.substr(last, i - last));
      }
      last = i + 1;
    }
  }
  if (last != source.length()) {
    fields->emplace_back(source.substr(last, source.length() - last));
  }
  return fields->size();
}

bool tokenize_first(absl::string_view source,
                    const char delimiter,
                    std::string* token,
                    std::string* rest) {
  // Find the first delimiter
  size_t left_pos = source.find(delimiter);
  if (left_pos == std::string::npos) {
    return false;
  }

  // Look for additional occurrances of delimiter.
  size_t right_pos = left_pos + 1;
  while (right_pos < source.size() && source[right_pos] == delimiter) {
    right_pos++;
  }

  *token = std::string(source.substr(0, left_pos));
  *rest = std::string(source.substr(right_pos));
  return true;
}

std::string join(const std::vector<std::string>& source, char delimiter) {
  if (source.size() == 0) {
    return std::string();
  }
  // Find length of the string to be returned to pre-allocate memory.
  size_t source_string_length = 0;
  for (size_t i = 0; i < source.size(); ++i) {
    source_string_length += source[i].length();
  }

  // Build the joined string.
  std::string joined_string;
  joined_string.reserve(source_string_length + source.size() - 1);
  for (size_t i = 0; i < source.size(); ++i) {
    if (i != 0) {
      joined_string += delimiter;
    }
    joined_string += source[i];
  }
  return joined_string;
}

size_t split(absl::string_view source,
             char delimiter,
             std::vector<std::string>* fields) {
  RTC_DCHECK(fields);
  fields->clear();
  size_t last = 0;
  for (size_t i = 0; i < source.length(); ++i) {
    if (source[i] == delimiter) {
      fields->emplace_back(source.substr(last, i - last));
      last = i + 1;
    }
  }
  fields->emplace_back(source.substr(last));
  return fields->size();
}

std::string ToString(const bool b) {
  return b ? "true" : "false";
}

std::string ToString(const char* const s) {
  return std::string(s);
}
std::string ToString(const std::string s) {
  return s;
}

std::string ToString(const short s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%hd", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const unsigned short s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%hu", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%d", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const unsigned int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%u", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const long int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%ld", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const unsigned long int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%lu", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const long long int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%lld", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}
std::string ToString(const unsigned long long int s) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%llu", s);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

std::string ToString(const double d) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%g", d);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

std::string ToString(const long double d) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%Lg", d);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

std::string ToString(const void* const p) {
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%p", p);
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

bool FromString(const std::string& s, bool* b) {
  if (s == "false") {
    *b = false;
    return true;
  }
  if (s == "true") {
    *b = true;
    return true;
  }
  return false;
}

}  // namespace rtc
