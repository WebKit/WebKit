/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGENCODE_H_
#define RTC_BASE_STRINGENCODE_H_

#include <sstream>
#include <string>
#include <vector>

#include "rtc_base/checks.h"

namespace rtc {

//////////////////////////////////////////////////////////////////////
// String Encoding Utilities
//////////////////////////////////////////////////////////////////////

// Note: in-place decoding (buffer == source) is allowed.
size_t url_decode(char * buffer, size_t buflen,
                  const char * source, size_t srclen);

// Convert an unsigned value from 0 to 15 to the hex character equivalent...
char hex_encode(unsigned char val);
// ...and vice-versa.
bool hex_decode(char ch, unsigned char* val);

// hex_encode shows the hex representation of binary data in ascii.
size_t hex_encode(char* buffer, size_t buflen,
                  const char* source, size_t srclen);

// hex_encode, but separate each byte representation with a delimiter.
// |delimiter| == 0 means no delimiter
// If the buffer is too short, we return 0
size_t hex_encode_with_delimiter(char* buffer, size_t buflen,
                                 const char* source, size_t srclen,
                                 char delimiter);

// Helper functions for hex_encode.
std::string hex_encode(const std::string& str);
std::string hex_encode(const char* source, size_t srclen);
std::string hex_encode_with_delimiter(const char* source, size_t srclen,
                                      char delimiter);

// hex_decode converts ascii hex to binary.
size_t hex_decode(char* buffer, size_t buflen,
                  const char* source, size_t srclen);

// hex_decode, assuming that there is a delimiter between every byte
// pair.
// |delimiter| == 0 means no delimiter
// If the buffer is too short or the data is invalid, we return 0.
size_t hex_decode_with_delimiter(char* buffer, size_t buflen,
                                 const char* source, size_t srclen,
                                 char delimiter);

// Helper functions for hex_decode.
size_t hex_decode(char* buffer, size_t buflen, const std::string& source);
size_t hex_decode_with_delimiter(char* buffer, size_t buflen,
                                 const std::string& source, char delimiter);

// Apply any suitable string transform (including the ones above) to an STL
// string.  Stack-allocated temporary space is used for the transformation,
// so value and source may refer to the same string.
typedef size_t (*Transform)(char * buffer, size_t buflen,
                            const char * source, size_t srclen);
size_t transform(std::string& value, size_t maxlen, const std::string& source,
                 Transform t);

// Return the result of applying transform t to source.
std::string s_transform(const std::string& source, Transform t);

// Convenience wrappers.
inline std::string s_url_decode(const std::string& source) {
  return s_transform(source, url_decode);
}

// Joins the source vector of strings into a single string, with each
// field in source being separated by delimiter. No trailing delimiter is added.
std::string join(const std::vector<std::string>& source, char delimiter);

// Splits the source string into multiple fields separated by delimiter,
// with duplicates of delimiter creating empty fields.
size_t split(const std::string& source, char delimiter,
             std::vector<std::string>* fields);

// Splits the source string into multiple fields separated by delimiter,
// with duplicates of delimiter ignored.  Trailing delimiter ignored.
size_t tokenize(const std::string& source, char delimiter,
                std::vector<std::string>* fields);

// Tokenize, including the empty tokens.
size_t tokenize_with_empty_tokens(const std::string& source,
                                  char delimiter,
                                  std::vector<std::string>* fields);

// Tokenize and append the tokens to fields. Return the new size of fields.
size_t tokenize_append(const std::string& source, char delimiter,
                       std::vector<std::string>* fields);

// Splits the source string into multiple fields separated by delimiter, with
// duplicates of delimiter ignored. Trailing delimiter ignored. A substring in
// between the start_mark and the end_mark is treated as a single field. Return
// the size of fields. For example, if source is "filename
// \"/Library/Application Support/media content.txt\"", delimiter is ' ', and
// the start_mark and end_mark are '"', this method returns two fields:
// "filename" and "/Library/Application Support/media content.txt".
size_t tokenize(const std::string& source, char delimiter, char start_mark,
                char end_mark, std::vector<std::string>* fields);

// Extract the first token from source as separated by delimiter, with
// duplicates of delimiter ignored. Return false if the delimiter could not be
// found, otherwise return true.
bool tokenize_first(const std::string& source,
                    const char delimiter,
                    std::string* token,
                    std::string* rest);

// Convert arbitrary values to/from a string.

template <class T>
static bool ToString(const T &t, std::string* s) {
  RTC_DCHECK(s);
  std::ostringstream oss;
  oss << std::boolalpha << t;
  *s = oss.str();
  return !oss.fail();
}

template <class T>
static bool FromString(const std::string& s, T* t) {
  RTC_DCHECK(t);
  std::istringstream iss(s);
  iss >> std::boolalpha >> *t;
  return !iss.fail();
}

// Inline versions of the string conversion routines.

template<typename T>
static inline std::string ToString(const T& val) {
  std::string str; ToString(val, &str); return str;
}

template<typename T>
static inline T FromString(const std::string& str) {
  T val; FromString(str, &val); return val;
}

template<typename T>
static inline T FromString(const T& defaultValue, const std::string& str) {
  T val(defaultValue); FromString(str, &val); return val;
}

//////////////////////////////////////////////////////////////////////

}  // namespace rtc

#endif  // RTC_BASE_STRINGENCODE_H__
