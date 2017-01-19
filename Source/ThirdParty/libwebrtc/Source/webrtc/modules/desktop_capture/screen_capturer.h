/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_H_

#include <vector>

#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class DesktopCaptureOptions;

// TODO(zijiehe): Remove this class.
// Class used to capture video frames asynchronously.
//
// The full capture sequence is as follows:
//
// (1) Start
//     This is when pre-capture steps are executed, such as flagging the
//     display to prevent it from sleeping during a session.
//
// (2) CaptureFrame
//     This is where the bits for the invalid rects are packaged up and sent
//     to the encoder.
//     A screen capture is performed if needed. For example, Windows requires
//     a capture to calculate the diff from the previous screen, whereas the
//     Mac version does not.
//
// Implementation has to ensure the following guarantees:
// 1. Double buffering
//    Since data can be read while another capture action is happening.
class ScreenCapturer : public DesktopCapturer {
 public:
  // Use a struct to represent a screen although it has only an id for now,
  // because we may want to add more fields (e.g. description) in the future.
  struct Screen {
    ScreenId id;
  };
  typedef std::vector<Screen> ScreenList;

  ~ScreenCapturer() override;

  // Creates a platform-specific capturer.
  static ScreenCapturer* Create(const DesktopCaptureOptions& options);

  // Deprecated, use GetSourceList().
  // Get the list of screens (not containing kFullDesktopScreenId). Returns
  // false in case of a failure.
  virtual bool GetScreenList(ScreenList* screens);

  // Deprecated, use SelectSource().
  // Select the screen to be captured. Returns false in case of a failure (e.g.
  // if there is no screen with the specified id). If this is never called, the
  // full desktop is captured.
  virtual bool SelectScreen(ScreenId id);

  // DesktopCapturer interfaces.
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_H_
