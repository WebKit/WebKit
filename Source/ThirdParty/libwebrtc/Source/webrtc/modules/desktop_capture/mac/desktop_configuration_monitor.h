/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_

#include <ApplicationServices/ApplicationServices.h>

#include <memory>
#include <set>

#include "api/refcountedbase.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

// The class provides functions to synchronize capturing and display
// reconfiguring across threads, and the up-to-date MacDesktopConfiguration.
class DesktopConfigurationMonitor : public rtc::RefCountedBase {
 public:
  DesktopConfigurationMonitor();
  // Returns the current desktop configuration.
  MacDesktopConfiguration desktop_configuration();

 protected:
  ~DesktopConfigurationMonitor() override;

 private:
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void* user_parameter);
  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);

  rtc::CriticalSection desktop_configuration_lock_;
  MacDesktopConfiguration desktop_configuration_
      RTC_GUARDED_BY(&desktop_configuration_lock_);
  std::set<CGDirectDisplayID> reconfiguring_displays_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopConfigurationMonitor);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_MONITOR_H_
