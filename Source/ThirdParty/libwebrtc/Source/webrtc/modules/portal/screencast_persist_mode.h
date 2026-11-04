/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PORTAL_SCREENCAST_PERSIST_MODE_H_
#define MODULES_PORTAL_SCREENCAST_PERSIST_MODE_H_

#include <cstdint>

namespace webrtc {
namespace xdg_portal {

// Values are set based on persist mode property in
// xdg-desktop-portal/screencast
// https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.portal.ScreenCast.xml
enum class ScreenCastPersistMode : uint32_t {
  // Do not allow to restore stream
  kDoNotPersist = 0b00,
  // The restore token is valid as long as the application is alive. It's
  // stored in memory and revoked when the application closes its DBus
  // connection
  kTransient = 0b01,
  // The restore token is stored in disk and is valid until the user manually
  // revokes it
  kPersistent = 0b10
};

}  // namespace xdg_portal
}  // namespace webrtc

#endif  // MODULES_PORTAL_SCREENCAST_PERSIST_MODE_H_
