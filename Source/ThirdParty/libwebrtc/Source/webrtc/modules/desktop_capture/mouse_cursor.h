/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_
#define MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT MouseCursor {
 public:
  MouseCursor();

  // `hotspot` must be within `image` boundaries.
  MouseCursor(std::unique_ptr<DesktopFrame> image,
              const DesktopVector& hotspot);

  // Deprecated. Use the overload above instead.
  // TODO(yuweih): Remove this.
  // Takes ownership of `image`. `hotspot` must be within `image` boundaries.
  MouseCursor(DesktopFrame* image, const DesktopVector& hotspot);

  ~MouseCursor();

  MouseCursor(const MouseCursor&) = delete;
  MouseCursor& operator=(const MouseCursor&) = delete;

  static MouseCursor* CopyOf(const MouseCursor& cursor);

  void set_image(std::unique_ptr<DesktopFrame> image) {
    image_ = std::move(image);
  }

  // Deprecated. Use the overload above instead.
  // TODO(yuweih): Remove this.
  // Takes ownership of `image`.
  void set_image(DesktopFrame* image) { image_.reset(image); }
  const DesktopFrame* image() const { return image_.get(); }

  // Extracts and takes ownership of the underlying cursor image. This is
  // useful, e.g., to share the cursor image using SharedDesktopFrame.
  std::unique_ptr<DesktopFrame> TakeImage();

  void set_hotspot(const DesktopVector& hotspot) { hotspot_ = hotspot; }
  const DesktopVector& hotspot() const { return hotspot_; }

 private:
  std::unique_ptr<DesktopFrame> image_;
  DesktopVector hotspot_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_H_
