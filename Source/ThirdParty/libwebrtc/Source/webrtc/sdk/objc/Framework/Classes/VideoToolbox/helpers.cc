/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "helpers.h"

#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

// Copies characters from a CFStringRef into a std::string.
std::string CFStringToString(const CFStringRef cf_string) {
  RTC_DCHECK(cf_string);
  std::string std_string;
  // Get the size needed for UTF8 plus terminating character.
  size_t buffer_size =
      CFStringGetMaximumSizeForEncoding(CFStringGetLength(cf_string),
                                        kCFStringEncodingUTF8) +
      1;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  if (CFStringGetCString(cf_string, buffer.get(), buffer_size,
                         kCFStringEncodingUTF8)) {
    // Copy over the characters.
    std_string.assign(buffer.get());
  }
  return std_string;
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          int32_t value) {
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  OSStatus status = VTSessionSetProperty(session, key, cfNum);
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          uint32_t value) {
  int64_t value_64 = value;
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value_64);
  OSStatus status = VTSessionSetProperty(session, key, cfNum);
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session, CFStringRef key, bool value) {
  CFBooleanRef cf_bool = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  OSStatus status = VTSessionSetProperty(session, key, cf_bool);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          CFStringRef value) {
  OSStatus status = VTSessionSetProperty(session, key, value);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    std::string val_string = CFStringToString(value);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << val_string << ": " << status;
  }
}
