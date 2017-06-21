/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mac/full_screen_chrome_window_detector.h"

#include <assert.h>
#include <libproc.h>
#include <string>

#include "webrtc/base/macutils.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/desktop_capture/mac/window_list_utils.h"


namespace webrtc {

namespace {

const int64_t kUpdateIntervalMs = 500;

std::string GetWindowTitle(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  std::string title;

  if (window_array && CFArrayGetCount(window_array)) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0));
    CFStringRef title_ref =  reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));

    if (title_ref)
      rtc::ToUtf8(title_ref, &title);
  }
  CFRelease(window_id_array);
  CFRelease(window_array);

  return title;
}

int GetWindowOwnerPid(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  int pid = 0;

  if (window_array && CFArrayGetCount(window_array)) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0));
    CFNumberRef pid_ref =  reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowOwnerPID));

    if (pid_ref)
      CFNumberGetValue(pid_ref, kCFNumberIntType, &pid);
  }
  CFRelease(window_id_array);
  CFRelease(window_array);

  return pid;
}

// Returns the window that is full-screen and has the same title and owner pid
// as the input window.
CGWindowID FindFullScreenWindowWithSamePidAndTitle(CGWindowID id) {
  int pid = GetWindowOwnerPid(id);
  std::string title = GetWindowTitle(id);

  // Only get on screen, non-desktop windows.
  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array)
    return kCGNullWindowID;

  CGWindowID full_screen_window = kCGNullWindowID;

  MacDesktopConfiguration desktop_config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::TopLeftOrigin);

  // Check windows to make sure they have an id, title, and use window layer
  // other than 0.
  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, i));
    CFStringRef window_title_ref = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));
    CFNumberRef window_id_ref = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));
    CFNumberRef window_pid_ref =  reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowOwnerPID));

    if (!window_title_ref || !window_id_ref || !window_pid_ref)
      continue;

    int window_pid = 0;
    CFNumberGetValue(window_pid_ref, kCFNumberIntType, &window_pid);
    if (window_pid != pid)
      continue;

    std::string window_title;
    if (!rtc::ToUtf8(window_title_ref, &window_title) ||
        window_title != title) {
      continue;
    }

    CGWindowID window_id;
    CFNumberGetValue(window_id_ref, kCFNumberIntType, &window_id);
    if (IsWindowFullScreen(desktop_config, window)) {
      full_screen_window = window_id;
      break;
    }
  }

  CFRelease(window_array);
  return full_screen_window;
}

bool IsChromeWindow(CGWindowID id) {
  int pid = GetWindowOwnerPid(id);
  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  int path_length = proc_pidpath(pid, buffer, sizeof(buffer));
  if (path_length <= 0)
    return false;

  const char* last_slash = strrchr(buffer, '/');
  std::string name(last_slash ? last_slash + 1 : buffer);
  return name.find("Google Chrome") == 0 || name == "Chromium";
}

}  // namespace

FullScreenChromeWindowDetector::FullScreenChromeWindowDetector()
    : ref_count_(0), last_update_time_ns_(0) {}

FullScreenChromeWindowDetector::~FullScreenChromeWindowDetector() {}

CGWindowID FullScreenChromeWindowDetector::FindFullScreenWindow(
    CGWindowID original_window) {
  if (!IsChromeWindow(original_window) || !IsWindowMinimized(original_window))
    return kCGNullWindowID;

  CGWindowID full_screen_window_id =
      FindFullScreenWindowWithSamePidAndTitle(original_window);

  if (full_screen_window_id == kCGNullWindowID)
    return kCGNullWindowID;

  for (const auto& window : previous_window_list_) {
    if (static_cast<CGWindowID>(window.id) != full_screen_window_id)
      continue;

    LOG(LS_WARNING) << "The full-screen window exists in the list.";
    return kCGNullWindowID;
  }

  return full_screen_window_id;
}

void FullScreenChromeWindowDetector::UpdateWindowListIfNeeded(
    CGWindowID original_window) {
  if (IsChromeWindow(original_window) &&
      (rtc::TimeNanos() - last_update_time_ns_) / rtc::kNumNanosecsPerMillisec
          > kUpdateIntervalMs) {
    previous_window_list_.clear();
    previous_window_list_.swap(current_window_list_);

    // No need to update the window list when the window is minimized.
    if (IsWindowMinimized(original_window)) {
      previous_window_list_.clear();
      return;
    }

    GetWindowList(&current_window_list_, false);
    last_update_time_ns_ = rtc::TimeNanos();
  }
}

}  // namespace webrtc
