/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mac/desktop_configuration_monitor.h"

#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

// The amount of time allowed for displays to reconfigure.
static const int64_t kDisplayConfigurationEventTimeoutMs = 10 * 1000;

DesktopConfigurationMonitor::DesktopConfigurationMonitor()
    : ref_count_(0),
      display_configuration_capture_event_(EventWrapper::Create()) {
  CGError err = CGDisplayRegisterReconfigurationCallback(
      DesktopConfigurationMonitor::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(LS_ERROR) << "CGDisplayRegisterReconfigurationCallback " << err;
    abort();
  }
  display_configuration_capture_event_->Set();

  desktop_configuration_ = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::TopLeftOrigin);
}

DesktopConfigurationMonitor::~DesktopConfigurationMonitor() {
  CGError err = CGDisplayRemoveReconfigurationCallback(
      DesktopConfigurationMonitor::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess)
    LOG(LS_ERROR) << "CGDisplayRemoveReconfigurationCallback " << err;
}

void DesktopConfigurationMonitor::Lock() {
  if (!display_configuration_capture_event_->Wait(
              kDisplayConfigurationEventTimeoutMs)) {
    LOG_F(LS_ERROR) << "Event wait timed out.";
    abort();
  }
}

void DesktopConfigurationMonitor::Unlock() {
  display_configuration_capture_event_->Set();
}

// static
void DesktopConfigurationMonitor::DisplaysReconfiguredCallback(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void *user_parameter) {
  DesktopConfigurationMonitor* monitor =
      reinterpret_cast<DesktopConfigurationMonitor*>(user_parameter);
  monitor->DisplaysReconfigured(display, flags);
}

void DesktopConfigurationMonitor::DisplaysReconfigured(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags) {
  if (flags & kCGDisplayBeginConfigurationFlag) {
    if (reconfiguring_displays_.empty()) {
      // If this is the first display to start reconfiguring then wait on
      // |display_configuration_capture_event_| to block the capture thread
      // from accessing display memory until the reconfiguration completes.
      if (!display_configuration_capture_event_->Wait(
              kDisplayConfigurationEventTimeoutMs)) {
        LOG_F(LS_ERROR) << "Event wait timed out.";
        abort();
      }
    }
    reconfiguring_displays_.insert(display);
  } else {
    reconfiguring_displays_.erase(display);
    if (reconfiguring_displays_.empty()) {
      desktop_configuration_ = MacDesktopConfiguration::GetCurrent(
          MacDesktopConfiguration::TopLeftOrigin);
      display_configuration_capture_event_->Set();
    }
  }
}

}  // namespace webrtc
