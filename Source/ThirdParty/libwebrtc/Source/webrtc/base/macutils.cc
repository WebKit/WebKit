/*
 *  Copyright 2007 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <memory>
#include <sstream>

#include <sys/utsname.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/macutils.h"
#include "webrtc/base/stringutils.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////

bool ToUtf8(const CFStringRef str16, std::string* str8) {
  if ((nullptr == str16) || (nullptr == str8)) {
    return false;
  }
  size_t maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str16),
                                                    kCFStringEncodingUTF8) + 1;
  std::unique_ptr<char[]> buffer(new char[maxlen]);
  if (!buffer || !CFStringGetCString(str16, buffer.get(), maxlen,
                                     kCFStringEncodingUTF8)) {
    return false;
  }
  str8->assign(buffer.get());
  return true;
}

bool ToUtf16(const std::string& str8, CFStringRef* str16) {
  if (nullptr == str16) {
    return false;
  }
  *str16 = CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8*>(str8.data()),
                                   str8.length(), kCFStringEncodingUTF8,
                                   false);
  return nullptr != *str16;
}

void DecodeFourChar(UInt32 fc, std::string* out) {
  std::stringstream ss;
  ss << '\'';
  bool printable = true;
  for (int i = 3; i >= 0; --i) {
    char ch = (fc >> (8 * i)) & 0xFF;
    if (isprint(static_cast<unsigned char>(ch))) {
      ss << ch;
    } else {
      printable = false;
      break;
    }
  }
  if (printable) {
    ss << '\'';
  } else {
    ss.str("");
    ss << "0x" << std::hex << fc;
  }
  out->append(ss.str());
}

static bool GetOSVersion(int* major, int* minor, int* bugfix) {
  RTC_DCHECK(major && minor && bugfix);
  struct utsname uname_info;
  if (uname(&uname_info) != 0)
    return false;

  if (strcmp(uname_info.sysname, "Darwin") != 0)
    return false;
  *major = 10;

  // The market version of macOS is always 4 lower than the internal version.
  int minor_version = atoi(uname_info.release);
  RTC_CHECK(minor_version >= 6);
  *minor = minor_version - 4;

  const char* dot = ::strchr(uname_info.release, '.');
  if (!dot)
    return false;
  *bugfix = atoi(dot + 1);
  return true;
}

MacOSVersionName GetOSVersionName() {
  int major = 0, minor = 0, bugfix = 0;
  if (!GetOSVersion(&major, &minor, &bugfix)) {
    return kMacOSUnknown;
  }
  if (major > 10) {
    return kMacOSNewer;
  }
  if ((major < 10) || (minor < 3)) {
    return kMacOSOlder;
  }
  switch (minor) {
    case 3:
      return kMacOSPanther;
    case 4:
      return kMacOSTiger;
    case 5:
      return kMacOSLeopard;
    case 6:
      return kMacOSSnowLeopard;
    case 7:
      return kMacOSLion;
    case 8:
      return kMacOSMountainLion;
    case 9:
      return kMacOSMavericks;
  }
  return kMacOSNewer;
}
}  // namespace rtc
