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
void SetVTSessionProperty(VTCompressionSessionRef vtSession, VCPCompressionSessionRef vcpSession,
                          CFStringRef key,
                          int32_t value) {
  RTC_DCHECK(vtSession || vcpSession);
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  OSStatus status = noErr;
  if (vtSession)
    status = VTSessionSetProperty(vtSession, key, cfNum);
#if ENABLE_VCP_ENCODER
  else
    status = webrtc::VCPCompressionSessionSetProperty(vcpSession, key, cfNum);
#endif
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTCompressionSessionRef vtSession, VCPCompressionSessionRef vcpSession,
                          CFStringRef key,
                          uint32_t value) {
  RTC_DCHECK(vtSession || vcpSession);
  int64_t value_64 = value;
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value_64);
  OSStatus status = noErr;
  if (vtSession)
    status = VTSessionSetProperty(vtSession, key, cfNum);
#if ENABLE_VCP_ENCODER
  else
    status = webrtc::VCPCompressionSessionSetProperty(vcpSession, key, cfNum);
#endif
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTCompressionSessionRef vtSession, VCPCompressionSessionRef vcpSession, CFStringRef key, bool value) {
  RTC_DCHECK(vtSession || vcpSession);
  CFBooleanRef cf_bool = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  OSStatus status = noErr;
  if (vtSession)
    status = VTSessionSetProperty(vtSession, key, cf_bool);
#if ENABLE_VCP_ENCODER
  else
    status = webrtc::VCPCompressionSessionSetProperty(vcpSession, key, cf_bool);
#endif
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTCompressionSessionRef vtSession, VCPCompressionSessionRef vcpSession,
                          CFStringRef key,
                          CFStringRef value) {
  RTC_DCHECK(vtSession || vcpSession);
  OSStatus status = noErr;
  if (vtSession)
    status = VTSessionSetProperty(vtSession, key, value);
#if ENABLE_VCP_ENCODER
  else
    status = webrtc::VCPCompressionSessionSetProperty(vcpSession, key, value);
#endif
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    std::string val_string = CFStringToString(value);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to " << val_string << ": " << status;
  }
}

// Convenience function for setting a VT property.
void SetVTSessionProperty(VTCompressionSessionRef vtSession, VCPCompressionSessionRef vcpSession,
                          CFStringRef key,
                          CFArrayRef value) {
  RTC_DCHECK(vtSession || vcpSession);
  OSStatus status = noErr;
  if (vtSession)
    status = VTSessionSetProperty(vtSession, key, value);
#if ENABLE_VCP_ENCODER
  else
    status = webrtc::VCPCompressionSessionSetProperty(vcpSession, key, value);
#endif

  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                      << " to array value: " << status << ", vcpSession: " << vcpSession;
  }
}
