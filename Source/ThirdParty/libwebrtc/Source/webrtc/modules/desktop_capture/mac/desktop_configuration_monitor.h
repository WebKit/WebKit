/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_

#include <ApplicationServices/ApplicationServices.h>

#include <memory>
#include <set>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/system_wrappers/include/atomic32.h"

namespace webrtc {

class EventWrapper;

// The class provides functions to synchronize capturing and display
// reconfiguring across threads, and the up-to-date MacDesktopConfiguration.
class DesktopConfigurationMonitor {
 public:
  DesktopConfigurationMonitor();
  // Acquires a lock on the current configuration.
  void Lock();
  // Releases the lock previously acquired.
  void Unlock();
  // Returns the current desktop configuration. Should only be called when the
  // lock has been acquired.
  const MacDesktopConfiguration& desktop_configuration() {
    return desktop_configuration_;
  }

  void AddRef() { ++ref_count_; }
  void Release() {
    if (--ref_count_ == 0)
      delete this;
  }

 private:
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);
  ~DesktopConfigurationMonitor();

  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);

  Atomic32 ref_count_;
  std::set<CGDirectDisplayID> reconfiguring_displays_;
  MacDesktopConfiguration desktop_configuration_;
  std::unique_ptr<EventWrapper> display_configuration_capture_event_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopConfigurationMonitor);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_
