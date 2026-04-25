/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_
#define MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_

#include <cstdint>

#include "modules/desktop_capture/desktop_capturer.h"

namespace webrtc {

// Base class for application specific handler to check criteria for switch to
// full-screen mode and find if possible the full-screen window to share.
// Supposed to be created and owned by platform specific
// FullScreenWindowDetector.
class FullScreenApplicationHandler {
 public:
  virtual ~FullScreenApplicationHandler() {}

  FullScreenApplicationHandler(const FullScreenApplicationHandler&) = delete;
  FullScreenApplicationHandler& operator=(const FullScreenApplicationHandler&) =
      delete;

  explicit FullScreenApplicationHandler(DesktopCapturer::SourceId sourceId);

  // Returns the full-screen window in place of the original window if all the
  // criteria are met, or 0 if no such window found.
  virtual DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& window_list,
      int64_t timestamp) const;

  void SetHeuristicForFindingEditor(bool use_heuristic) {
    use_heuristic_for_finding_editor_ = use_heuristic;
  }

  bool UseHeuristicForFindingEditor() const {
    return use_heuristic_for_finding_editor_;
  }

  // Returns the editor window id if the `source_id_` corresponds to a full
  // screen window or `source_id_` if it corresponds to an editor window.
  // Returns 0 if no such window is found.
  virtual DesktopCapturer::SourceId FindEditorWindow(
      const DesktopCapturer::SourceList& window_list) const;

  // Returns source id of original window associated with
  // FullScreenApplicationHandler
  DesktopCapturer::SourceId GetSourceId() const;

  virtual void SetSlideShowCreationStateForTest(
      bool fullscreen_slide_show_started_after_capture_start) {}

  virtual void SetEditorWasFound() {}

 private:
  const DesktopCapturer::SourceId source_id_;
  // `use_heuristic_fullscreen_powerpoint_windows_` is used to implement a
  // finch experiment.
  bool use_heuristic_for_finding_editor_ = false;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_FULL_SCREEN_APPLICATION_HANDLER_H_
