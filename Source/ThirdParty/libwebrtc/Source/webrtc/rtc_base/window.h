/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_WINDOW_H_
#define RTC_BASE_WINDOW_H_

#include <stdint.h>

#include "rtc_base/stringencode.h"

// Define platform specific window types.
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
typedef unsigned long Window;  // Avoid include <X11/Xlib.h>.
#elif defined(WEBRTC_WIN)
// We commonly include win32.h in webrtc/rtc_base so just include it here.
#include "rtc_base/win32.h"  // Include HWND, HMONITOR.
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
typedef unsigned int CGWindowID;
typedef unsigned int CGDirectDisplayID;
#endif

namespace rtc {

class WindowId {
 public:
  // Define WindowT for each platform.
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
  typedef Window WindowT;
#elif defined(WEBRTC_WIN)
  typedef HWND WindowT;
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  typedef CGWindowID WindowT;
#else
  typedef unsigned int WindowT;
#endif

  static WindowId Cast(uint64_t id) {
#if defined(WEBRTC_WIN)
    return WindowId(reinterpret_cast<WindowId::WindowT>(id));
#else
    return WindowId(static_cast<WindowId::WindowT>(id));
#endif
  }

  static uint64_t Format(const WindowT& id) {
#if defined(WEBRTC_WIN)
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(id));
#else
    return static_cast<uint64_t>(id);
#endif
  }

  WindowId() : id_(0) {}
  WindowId(const WindowT& id) : id_(id) {}  // NOLINT
  const WindowT& id() const { return id_; }
  bool IsValid() const { return id_ != 0; }
  bool Equals(const WindowId& other) const {
    return id_ == other.id();
  }

 private:
  WindowT id_;
};

inline std::string ToString(const WindowId& window) {
  return ToString(window.id());
}

}  // namespace rtc

#endif  // RTC_BASE_WINDOW_H_
