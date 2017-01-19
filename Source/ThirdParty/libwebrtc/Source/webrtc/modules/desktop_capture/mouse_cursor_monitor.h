/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_MONITOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_MONITOR_H_

#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class DesktopCaptureOptions;
class DesktopFrame;
class MouseCursor;

// Captures mouse shape and position.
class MouseCursorMonitor {
 public:
  enum CursorState {
    // Cursor on top of the window including window decorations.
    INSIDE,

    // Cursor is outside of the window.
    OUTSIDE,
  };

  enum Mode {
    // Capture only shape of the mouse cursor, but not position.
    SHAPE_ONLY,

    // Capture both, mouse cursor shape and position.
    SHAPE_AND_POSITION,
  };

  // Callback interface used to pass current mouse cursor position and shape.
  class Callback {
   public:
    // Called in response to Capture() when the cursor shape has changed. Must
    // take ownership of |cursor|.
    virtual void OnMouseCursor(MouseCursor* cursor) = 0;

    // Called in response to Capture(). |position| indicates cursor position
    // relative to the |window| specified in the constructor.
    virtual void OnMouseCursorPosition(CursorState state,
                                       const DesktopVector& position) = 0;

   protected:
    virtual ~Callback() {}
  };

  virtual ~MouseCursorMonitor() {}

  // Creates a capturer that notifies of mouse cursor events while the cursor is
  // over the specified window.
  static MouseCursorMonitor* CreateForWindow(
      const DesktopCaptureOptions& options,
      WindowId window);

  // Creates a capturer that monitors the mouse cursor shape and position across
  // the entire desktop.
  //
  // TODO(sergeyu): Provide a way to select a specific screen.
  static MouseCursorMonitor* CreateForScreen(
      const DesktopCaptureOptions& options,
      ScreenId screen);

  // Initializes the monitor with the |callback|, which must remain valid until
  // capturer is destroyed.
  virtual void Init(Callback* callback, Mode mode) = 0;

  // Captures current cursor shape and position (depending on the |mode| passed
  // to Init()). Calls Callback::OnMouseCursor() if cursor shape has
  // changed since the last call (or when Capture() is called for the first
  // time) and then Callback::OnMouseCursorPosition() if mode is set to
  // SHAPE_AND_POSITION.
  virtual void Capture() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_MONITOR_H_

