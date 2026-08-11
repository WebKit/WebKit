/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mouse_cursor.h"

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/checks.h"

namespace webrtc {

MouseCursor::MouseCursor() = default;

MouseCursor::MouseCursor(std::unique_ptr<DesktopFrame> image,
                         const DesktopVector& hotspot)
    : image_(std::move(image)), hotspot_(hotspot) {
  RTC_DCHECK(0 <= hotspot_.x() && hotspot_.x() <= image_->size().width());
  RTC_DCHECK(0 <= hotspot_.y() && hotspot_.y() <= image_->size().height());
}

MouseCursor::MouseCursor(DesktopFrame* image, const DesktopVector& hotspot)
    : MouseCursor(std::unique_ptr<DesktopFrame>(image), hotspot) {}

MouseCursor::~MouseCursor() = default;

// static
MouseCursor* MouseCursor::CopyOf(const MouseCursor& cursor) {
  return cursor.image()
             ? new MouseCursor(BasicDesktopFrame::CopyOf(*cursor.image()),
                               cursor.hotspot())
             : new MouseCursor();
}

std::unique_ptr<DesktopFrame> MouseCursor::TakeImage() {
  return std::move(image_);
}

}  // namespace webrtc
