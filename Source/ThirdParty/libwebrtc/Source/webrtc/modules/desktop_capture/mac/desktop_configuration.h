/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_H_

#include <ApplicationServices/ApplicationServices.h>
#include <vector>

#include "webrtc/typedefs.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// Describes the configuration of a specific display.
struct MacDisplayConfiguration {
  MacDisplayConfiguration();
  MacDisplayConfiguration(const MacDisplayConfiguration& other);
  MacDisplayConfiguration(MacDisplayConfiguration&& other);
  ~MacDisplayConfiguration();

  MacDisplayConfiguration& operator=(const MacDisplayConfiguration& other);
  MacDisplayConfiguration& operator=(MacDisplayConfiguration&& other);

  // Cocoa identifier for this display.
  CGDirectDisplayID id = 0;

  // Bounds of this display in Density-Independent Pixels (DIPs).
  DesktopRect bounds;

  // Bounds of this display in physical pixels.
  DesktopRect pixel_bounds;

  // Scale factor from DIPs to physical pixels.
  float dip_to_pixel_scale = 1.0f;
};

typedef std::vector<MacDisplayConfiguration> MacDisplayConfigurations;

// Describes the configuration of the whole desktop.
struct MacDesktopConfiguration {
  // Used to request bottom-up or top-down coordinates.
  enum Origin { BottomLeftOrigin, TopLeftOrigin };

  MacDesktopConfiguration();
  MacDesktopConfiguration(const MacDesktopConfiguration& other);
  MacDesktopConfiguration(MacDesktopConfiguration&& other);
  ~MacDesktopConfiguration();

  MacDesktopConfiguration& operator=(const MacDesktopConfiguration& other);
  MacDesktopConfiguration& operator=(MacDesktopConfiguration&& other);

  // Returns the desktop & display configurations in Cocoa-style "bottom-up"
  // (the origin is the bottom-left of the primary monitor, and coordinates
  // increase as you move up the screen).
  static MacDesktopConfiguration GetCurrent(Origin origin);

  // Returns true if the given desktop configuration equals this one.
  bool Equals(const MacDesktopConfiguration& other);

  // Returns the pointer to the display configuration with the specified id.
  const MacDisplayConfiguration* FindDisplayConfigurationById(
      CGDirectDisplayID id);

  // Bounds of the desktop excluding monitors with DPI settings different from
  // the main monitor. In Density-Independent Pixels (DIPs).
  DesktopRect bounds;

  // Same as bounds, but expressed in physical pixels.
  DesktopRect pixel_bounds;

  // Scale factor from DIPs to physical pixels.
  float dip_to_pixel_scale = 1.0f;

  // Configurations of the displays making up the desktop area.
  MacDisplayConfigurations displays;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_H_
