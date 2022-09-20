/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class SharedScreenCastStreamPrivate;

class RTC_EXPORT SharedScreenCastStream
    : public rtc::RefCountedNonVirtual<SharedScreenCastStream> {
 public:
  static rtc::scoped_refptr<SharedScreenCastStream> CreateDefault();

  bool StartScreenCastStream(uint32_t stream_node_id);
  bool StartScreenCastStream(uint32_t stream_node_id,
                             int fd,
                             uint32_t width = 0,
                             uint32_t height = 0);
  void UpdateScreenCastStreamResolution(uint32_t width, uint32_t height);
  void StopScreenCastStream();

  // Below functions return the most recent information we get from a
  // PipeWire buffer on each Process() callback. This assumes that we
  // managed to successfuly connect to a PipeWire stream provided by the
  // compositor (based on stream parameters). The cursor data are obtained
  // from spa_meta_cursor stream metadata and therefore the cursor is not
  // part of actual screen/window frame.

  // Returns the most recent screen/window frame we obtained from PipeWire
  // buffer. Will return an empty frame in case we didn't manage to get a frame
  // from PipeWire buffer.
  std::unique_ptr<DesktopFrame> CaptureFrame();

  // Returns the most recent mouse cursor image. Will return an nullptr cursor
  // in case we didn't manage to get a cursor from PipeWire buffer. NOTE: the
  // cursor image might not be updated on every cursor location change, but
  // actually only when its shape changes.
  std::unique_ptr<MouseCursor> CaptureCursor();

  // Returns the most recent mouse cursor position. Will not return a value in
  // case we didn't manage to get it from PipeWire buffer.
  absl::optional<DesktopVector> CaptureCursorPosition();

  ~SharedScreenCastStream();

 protected:
  SharedScreenCastStream();

 private:
  SharedScreenCastStream(const SharedScreenCastStream&) = delete;
  SharedScreenCastStream& operator=(const SharedScreenCastStream&) = delete;

  std::unique_ptr<SharedScreenCastStreamPrivate> private_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_
