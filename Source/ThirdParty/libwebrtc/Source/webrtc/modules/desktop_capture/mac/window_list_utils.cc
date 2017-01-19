/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mac/window_list_utils.h"

#include <ApplicationServices/ApplicationServices.h>

#include "webrtc/base/macutils.h"

namespace webrtc {

bool GetWindowList(DesktopCapturer::SourceList* windows,
                   bool ignore_minimized) {
  // Only get on screen, non-desktop windows.
  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array)
    return false;

  MacDesktopConfiguration desktop_config;
  if (ignore_minimized) {
    desktop_config = MacDesktopConfiguration::GetCurrent(
        MacDesktopConfiguration::TopLeftOrigin);
  }

  // Check windows to make sure they have an id, title, and use window layer
  // other than 0.
  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, i));
    CFStringRef window_title = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));
    CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));
    CFNumberRef window_layer = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowLayer));
    if (window_title && window_id && window_layer) {
      // Skip windows with layer=0 (menu, dock).
      int layer;
      CFNumberGetValue(window_layer, kCFNumberIntType, &layer);
      if (layer != 0)
        continue;

      int id;
      CFNumberGetValue(window_id, kCFNumberIntType, &id);

      // Skip windows that are minimized and not full screen.
      if (ignore_minimized && IsWindowMinimized(id) &&
          !IsWindowFullScreen(desktop_config, window)) {
        continue;
      }

      DesktopCapturer::Source window;
      window.id = id;
      if (!rtc::ToUtf8(window_title, &(window.title)) ||
          window.title.empty()) {
        continue;
      }
      windows->push_back(window);
    }
  }

  CFRelease(window_array);
  return true;
}

// Returns true if the window is occupying a full screen.
bool IsWindowFullScreen(
    const MacDesktopConfiguration& desktop_config,
    CFDictionaryRef window) {
  bool fullscreen = false;
  CFDictionaryRef bounds_ref = reinterpret_cast<CFDictionaryRef>(
      CFDictionaryGetValue(window, kCGWindowBounds));

  CGRect bounds;
  if (bounds_ref &&
      CGRectMakeWithDictionaryRepresentation(bounds_ref, &bounds)) {
    for (MacDisplayConfigurations::const_iterator it =
             desktop_config.displays.begin();
         it != desktop_config.displays.end(); ++it) {
      if (it->bounds.equals(DesktopRect::MakeXYWH(bounds.origin.x,
                                                  bounds.origin.y,
                                                  bounds.size.width,
                                                  bounds.size.height))) {
        fullscreen = true;
        break;
      }
    }
  }

  return fullscreen;
}

// Returns true if the window is minimized.
bool IsWindowMinimized(CGWindowID id) {
  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);
  bool minimized = false;

  if (window_array && CFArrayGetCount(window_array)) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0));
    CFBooleanRef on_screen =  reinterpret_cast<CFBooleanRef>(
        CFDictionaryGetValue(window, kCGWindowIsOnscreen));

    minimized = !on_screen;
  }

  CFRelease(window_id_array);
  CFRelease(window_array);

  return minimized;
}



}  // namespace webrtc
