/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/window_finder_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include <utility>

#include "modules/desktop_capture/mac/window_list_utils.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

WindowFinderMac::WindowFinderMac(
    rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor)
    : configuration_monitor_(std::move(configuration_monitor)) {}
WindowFinderMac::~WindowFinderMac() = default;

WindowId WindowFinderMac::GetWindowUnderPoint(DesktopVector point) {
  WindowId id = kNullWindowId;
  MacDesktopConfiguration configuration_holder;
  MacDesktopConfiguration* configuration = nullptr;
  if (configuration_monitor_) {
    configuration_monitor_->Lock();
    configuration_holder = configuration_monitor_->desktop_configuration();
    configuration_monitor_->Unlock();
    configuration = &configuration_holder;
  }
  GetWindowList([&id, point, configuration](CFDictionaryRef window) {
                  DesktopRect bounds;
                  if (configuration) {
                    bounds = GetWindowBounds(*configuration, window);
                  } else {
                    bounds = GetWindowBounds(window);
                  }
                  if (bounds.Contains(point)) {
                    id = GetWindowId(window);
                    return false;
                  }
                  return true;
                },
                true);
  return id;
}

// static
std::unique_ptr<WindowFinder> WindowFinder::Create(
    const WindowFinder::Options& options) {
  return rtc::MakeUnique<WindowFinderMac>(options.configuration_monitor);
}

}  // namespace webrtc
