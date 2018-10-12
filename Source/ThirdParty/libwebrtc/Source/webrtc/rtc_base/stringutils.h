/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGUTILS_H_
#define RTC_BASE_STRINGUTILS_H_

#if !defined(WEBRTC_WEBKIT_BUILD)
#include <ctype.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(WEBRTC_WIN)
#include <malloc.h>
#include <wchar.h>
#include <windows.h>
#define alloca _alloca
#endif  // WEBRTC_WIN

#if defined(WEBRTC_POSIX)
#ifdef BSD
#include <stdlib.h>
#else  // BSD
#include <alloca.h>
#endif  // !BSD
#endif  // WEBRTC_POSIX

#include <string>

///////////////////////////////////////////////////////////////////////////////
// Generic string/memory utilities
///////////////////////////////////////////////////////////////////////////////

#define STACK_ARRAY(TYPE, LEN) \
  static_cast<TYPE*>(::alloca((LEN) * sizeof(TYPE)))

namespace rtc {

// Determines whether the simple wildcard pattern matches target.
// Alpha characters in pattern match case-insensitively.
// Asterisks in pattern match 0 or more characters.
// Ex: string_match("www.TEST.GOOGLE.COM", "www.*.com") -> true
bool string_match(const char* target, const char* pattern);

}  // namespace rtc

#if !defined(WEBRTC_WEBKIT_BUILD)
///////////////////////////////////////////////////////////////////////////////
// Rename a few common string functions so they are consistent across platforms.
// tolowercase is like tolower, but not compatible with end-of-file value
//
// It's not clear if we will ever use wchar_t strings on unix.  In theory,
// all strings should be Utf8 all the time, except when interfacing with Win32
// APIs that require Utf16.
///////////////////////////////////////////////////////////////////////////////
inline char tolowercase(char c) {
  return static_cast<char>(tolower(c));
}
#endif // !defined(WEBRTC_WEBKIT_BUILD)

#if defined(WEBRTC_WIN)

inline wchar_t tolowercase(wchar_t c) {
  return static_cast<wchar_t>(towlower(c));
}

#endif  // WEBRTC_WIN

#if defined(WEBRTC_POSIX)

inline int _stricmp(const char* s1, const char* s2) {
  return strcasecmp(s1, s2);
}
inline int _strnicmp(const char* s1, const char* s2, size_t n) {
  return strncasecmp(s1, s2, n);
}

#endif  // WEBRTC_POSIX

///////////////////////////////////////////////////////////////////////////////
// Traits simplifies porting string functions to be CTYPE-agnostic
///////////////////////////////////////////////////////////////////////////////

namespace rtc {

const size_t SIZE_UNKNOWN = static_cast<size_t>(-1);

template <class CTYPE>
struct Traits {
  // STL string type
  // typedef XXX string;
  // Null-terminated string
  // inline static const CTYPE* empty_str();
};

///////////////////////////////////////////////////////////////////////////////
// String utilities which work with char or wchar_t
///////////////////////////////////////////////////////////////////////////////

template <class CTYPE>
inline const CTYPE* nonnull(const CTYPE* str, const CTYPE* def_str = nullptr) {
  return str ? str : (def_str ? def_str : Traits<CTYPE>::empty_str());
}

template <class CTYPE>
const CTYPE* strchr(const CTYPE* str, const CTYPE* chs) {
  for (size_t i = 0; str[i]; ++i) {
    for (size_t j = 0; chs[j]; ++j) {
      if (str[i] == chs[j]) {
        return str + i;
      }
    }
  }
  return 0;
}

template <class CTYPE>
const CTYPE* strchrn(const CTYPE* str, size_t slen, CTYPE ch) {
  for (size_t i = 0; i < slen && str[i]; ++i) {
    if (str[i] == ch) {
      return str + i;
    }
  }
  return 0;
}

template <class CTYPE>
size_t strlenn(const CTYPE* buffer, size_t buflen) {
  size_t bufpos = 0;
  while (buffer[bufpos] && (bufpos < buflen)) {
    ++bufpos;
  }
  return bufpos;
}

// Safe versions of strncpy, strncat, snprintf and vsnprintf that always
// null-terminate.

template <class CTYPE>
size_t strcpyn(CTYPE* buffer,
               size_t buflen,
               const CTYPE* source,
               size_t srclen = SIZE_UNKNOWN) {
  if (buflen <= 0)
    return 0;

  if (srclen == SIZE_UNKNOWN) {
    srclen = strlenn(source, buflen - 1);
  } else if (srclen >= buflen) {
    srclen = buflen - 1;
  }
  memcpy(buffer, source, srclen * sizeof(CTYPE));
  buffer[srclen] = 0;
  return srclen;
}

template <class CTYPE>
size_t strcatn(CTYPE* buffer,
               size_t buflen,
               const CTYPE* source,
               size_t srclen = SIZE_UNKNOWN) {
  if (buflen <= 0)
    return 0;

  size_t bufpos = strlenn(buffer, buflen - 1);
  return bufpos + strcpyn(buffer + bufpos, buflen - bufpos, source, srclen);
}

// Some compilers (clang specifically) require vsprintfn be defined before
// sprintfn.
template <class CTYPE>
size_t vsprintfn(CTYPE* buffer,
                 size_t buflen,
                 const CTYPE* format,
                 va_list args) {
  int len = vsnprintf(buffer, buflen, format, args);
  if ((len < 0) || (static_cast<size_t>(len) >= buflen)) {
    len = static_cast<int>(buflen - 1);
    buffer[len] = 0;
  }
  return len;
}

template <class CTYPE>
size_t sprintfn(CTYPE* buffer, size_t buflen, const CTYPE* format, ...);
template <class CTYPE>
size_t sprintfn(CTYPE* buffer, size_t buflen, const CTYPE* format, ...) {
  va_list args;
  va_start(args, format);
  size_t len = vsprintfn(buffer, buflen, format, args);
  va_end(args);
  return len;
}

///////////////////////////////////////////////////////////////////////////////
// Allow safe comparing and copying ascii (not UTF-8) with both wide and
// non-wide character strings.
///////////////////////////////////////////////////////////////////////////////

inline int asccmp(const char* s1, const char* s2) {
  return strcmp(s1, s2);
}
inline int ascicmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}
inline int ascncmp(const char* s1, const char* s2, size_t n) {
  return strncmp(s1, s2, n);
}
inline int ascnicmp(const char* s1, const char* s2, size_t n) {
  return _strnicmp(s1, s2, n);
}
inline size_t asccpyn(char* buffer,
                      size_t buflen,
                      const char* source,
                      size_t srclen = SIZE_UNKNOWN) {
  return strcpyn(buffer, buflen, source, srclen);
}

#if defined(WEBRTC_WIN)

typedef wchar_t (*CharacterTransformation)(wchar_t);
inline wchar_t identity(wchar_t c) {
  return c;
}
int ascii_string_compare(const wchar_t* s1,
                         const char* s2,
                         size_t n,
                         CharacterTransformation transformation);

inline int asccmp(const wchar_t* s1, const char* s2) {
  return ascii_string_compare(s1, s2, static_cast<size_t>(-1), identity);
}
inline int ascicmp(const wchar_t* s1, const char* s2) {
  return ascii_string_compare(s1, s2, static_cast<size_t>(-1), tolowercase);
}
inline int ascncmp(const wchar_t* s1, const char* s2, size_t n) {
  return ascii_string_compare(s1, s2, n, identity);
}
inline int ascnicmp(const wchar_t* s1, const char* s2, size_t n) {
  return ascii_string_compare(s1, s2, n, tolowercase);
}
size_t asccpyn(wchar_t* buffer,
               size_t buflen,
               const char* source,
               size_t srclen = SIZE_UNKNOWN);

#endif  // WEBRTC_WIN

///////////////////////////////////////////////////////////////////////////////
// Traits<char> specializations
///////////////////////////////////////////////////////////////////////////////

template <>
struct Traits<char> {
  typedef std::string string;
  inline static const char* empty_str() { return ""; }
};

///////////////////////////////////////////////////////////////////////////////
// Traits<wchar_t> specializations (Windows only, currently)
///////////////////////////////////////////////////////////////////////////////

#if defined(WEBRTC_WIN)

template <>
struct Traits<wchar_t> {
  typedef std::wstring string;
  inline static const wchar_t* empty_str() { return L""; }
};

#endif  // WEBRTC_WIN

///////////////////////////////////////////////////////////////////////////////
// UTF helpers (Windows only)
///////////////////////////////////////////////////////////////////////////////

#if defined(WEBRTC_WIN)

inline std::wstring ToUtf16(const char* utf8, size_t len) {
  int len16 = ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len),
                                    nullptr, 0);
  wchar_t* ws = STACK_ARRAY(wchar_t, len16);
  ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len), ws, len16);
  return std::wstring(ws, len16);
}

inline std::wstring ToUtf16(const std::string& str) {
  return ToUtf16(str.data(), str.length());
}

inline std::string ToUtf8(const wchar_t* wide, size_t len) {
  int len8 = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len),
                                   nullptr, 0, nullptr, nullptr);
  char* ns = STACK_ARRAY(char, len8);
  ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len), ns, len8,
                        nullptr, nullptr);
  return std::string(ns, len8);
}

inline std::string ToUtf8(const wchar_t* wide) {
  return ToUtf8(wide, wcslen(wide));
}

inline std::string ToUtf8(const std::wstring& wstr) {
  return ToUtf8(wstr.data(), wstr.length());
}

#endif  // WEBRTC_WIN

// Replaces all occurrences of "search" with "replace".
void replace_substrs(const char* search,
                     size_t search_len,
                     const char* replace,
                     size_t replace_len,
                     std::string* s);

// True iff s1 starts with s2.
bool starts_with(const char* s1, const char* s2);

// True iff s1 ends with s2.
bool ends_with(const char* s1, const char* s2);

// Remove leading and trailing whitespaces.
std::string string_trim(const std::string& s);

// TODO(jonasolsson): replace with absl::Hex when that becomes available.
std::string ToHex(const int i);

std::string LeftPad(char padding, unsigned length, std::string s);

}  // namespace rtc

#endif  // RTC_BASE_STRINGUTILS_H_
