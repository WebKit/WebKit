/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/full_screen_window_detector.h"

#include <cstdint>

#include "api/function_view.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/full_screen_application_handler.h"
#include "rtc_base/time_utils.h"

#if defined(WEBRTC_WIN)
#include "modules/desktop_capture/win/full_screen_win_application_handler.h"
#endif

namespace webrtc {

FullScreenWindowDetector::FullScreenWindowDetector(
    ApplicationHandlerFactory application_handler_factory)
    : application_handler_factory_(application_handler_factory),
      last_update_time_ms_(0),
      previous_source_id_(0),
      no_handler_source_id_(0) {}

DesktopCapturer::SourceId FullScreenWindowDetector::FindFullScreenWindow(
    DesktopCapturer::SourceId original_source_id) {
  if (app_handler_ == nullptr ||
      app_handler_->GetSourceId() != original_source_id) {
    return 0;
  }
  return app_handler_->FindFullScreenWindow(window_list_, last_update_time_ms_);
}

DesktopCapturer::SourceId FullScreenWindowDetector::FindEditorWindow(
    DesktopCapturer::SourceId original_source_id) {
  if (!UseHeuristicForFindingEditor()) {
    return 0;
  }

  if (app_handler_ == nullptr ||
      app_handler_->GetSourceId() != original_source_id) {
    return 0;
  }
  return app_handler_->FindEditorWindow(window_list_);
}

void FullScreenWindowDetector::UpdateWindowListIfNeeded(
    DesktopCapturer::SourceId original_source_id,
    FunctionView<bool(DesktopCapturer::SourceList*)> get_sources) {
  // Don't skip update if app_handler_ exists.
  const bool skip_update =
      !app_handler_ && (previous_source_id_ != original_source_id);
  previous_source_id_ = original_source_id;

  // Here is an attempt to avoid redundant creating application handler in case
  // when an instance of WindowCapturer is used to generate a thumbnail to show
  // in picker by calling SelectSource and CaptureFrame for every available
  // source.
  if (skip_update) {
    return;
  }

  CreateApplicationHandlerIfNeeded(original_source_id);
  if (app_handler_ == nullptr) {
    // There is no FullScreenApplicationHandler specific for
    // current application
    return;
  }

  constexpr int64_t kUpdateIntervalMs = 500;

  if ((TimeMillis() - last_update_time_ms_) <= kUpdateIntervalMs) {
    return;
  }

  DesktopCapturer::SourceList window_list;
  if (get_sources(&window_list)) {
    last_update_time_ms_ = TimeMillis();

    bool should_swap_windows = true;
#if defined(WEBRTC_WIN)
    bool is_original_source_window_alive =
        ::IsWindow(reinterpret_cast<HWND>(original_source_id));
    bool is_original_source_enumerated = false;
    for (auto& source : window_list) {
      if (source.id == original_source_id) {
        is_original_source_enumerated = true;
        break;
      }
    }
    // Don't swap window list if there is a mismatch between original window's
    // state and its enumerated state.
    should_swap_windows =
        (is_original_source_enumerated == is_original_source_window_alive);
#endif
    if (should_swap_windows) {
      window_list_.swap(window_list);
    }
  }
}

void FullScreenWindowDetector::CreateApplicationHandlerIfNeeded(
    DesktopCapturer::SourceId source_id) {
  if (no_handler_source_id_ == source_id) {
    return;
  }

  if (app_handler_ && app_handler_->GetSourceId() == source_id) {
    return;
  }

  app_handler_ = application_handler_factory_
                     ? application_handler_factory_(source_id)
                     : nullptr;

  if (app_handler_ == nullptr) {
    no_handler_source_id_ = source_id;
  } else {
    app_handler_->SetHeuristicForFindingEditor(
        use_heuristic_for_finding_editor_);
    if (found_editor_for_chosen_slide_show_) {
      app_handler_->SetEditorWasFound();
    }
  }
}

void FullScreenWindowDetector::SetEditorWasFoundForChosenSlideShow() {
  if (!UseHeuristicForFindingEditor())
    return;

  found_editor_for_chosen_slide_show_ = true;
}

void FullScreenWindowDetector::CreateFullScreenApplicationHandlerForTest(
    DesktopCapturer::SourceId source_id,
    bool fullscreen_slide_show_started_after_capture_start,
    bool use_heuristic_for_finding_editor) {
  if (app_handler_) {
    return;
  }
#if defined(WEBRTC_WIN)
  app_handler_ = std::make_unique<FullScreenPowerPointHandler>(source_id);
  app_handler_->SetSlideShowCreationStateForTest(
      fullscreen_slide_show_started_after_capture_start);
  use_heuristic_for_finding_editor_ = use_heuristic_for_finding_editor;
  app_handler_->SetHeuristicForFindingEditor(use_heuristic_for_finding_editor);
#endif
}

}  // namespace webrtc
