/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_
#define MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_

#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/full_screen_application_handler.h"

// Used for metrics; Entries should not be renumbered and numeric values should
// never be reused.
enum class FullScreenDetectorResult {
  kUnknown = 0,
  kSuccess = 1,
  kFailureDueToSameTitleWindows = 2,
  kFailureDueToSlideShowWasNotChosen = 3,
  kMaxValue = kFailureDueToSlideShowWasNotChosen
};

// Used for metrics; Entries should not be renumbered and numeric values should
// never be reused.
enum class FullScreenFindEditorResult {
  kSuccess = 0,
  kFailureDueToSameTitleWindows = 1,
  kMaxValue = kFailureDueToSameTitleWindows
};

namespace webrtc {

class FullScreenPowerPointHandler : public FullScreenApplicationHandler {
 public:
  enum class WindowType { kEditor, kSlideShow, kOther };

  explicit FullScreenPowerPointHandler(DesktopCapturer::SourceId sourceId);

  ~FullScreenPowerPointHandler() override {}

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& window_list,
      int64_t timestamp) const override;

  DesktopCapturer::SourceId FindEditorWindow(
      const DesktopCapturer::SourceList& window_list) const override;

  void SetSlideShowCreationStateForTest(
      bool fullscreen_slide_show_started_after_capture_start) override;

  void SetEditorWasFound() override;

 private:
  WindowType GetWindowType(HWND window) const;

  // This function extracts the title from the editor. It needs to be
  // updated every time PowerPoint changes its editor title format. Currently,
  // it supports editor title in the format "Window - Title - PowerPoint".
  std::string GetDocumentTitleFromEditor(HWND window) const;

  // This function extracts the title from the slideshow when PowerPoint goes
  // fullscreen. This function needs to be updated whenever PowerPoint changes
  // its title format. Currently, it supports Fullscreen titles of the format
  // "PowerPoint Slide Show - [Window - Title]" or "PowerPoint Slide Show -
  // Window - Title".
  std::string GetDocumentTitleFromSlideShow(HWND window) const;

  bool IsEditorWindow(HWND window) const;

  bool IsSlideShowWindow(HWND window) const;

  mutable bool was_slide_show_created_after_capture_started_;

  mutable FullScreenDetectorResult full_screen_detector_result_;
};

std::unique_ptr<FullScreenApplicationHandler>
CreateFullScreenWinApplicationHandler(DesktopCapturer::SourceId sourceId);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_
