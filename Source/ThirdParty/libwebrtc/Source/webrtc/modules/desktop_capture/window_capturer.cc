/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_capturer.h"

#include <string>
#include <utility>

namespace webrtc {

WindowCapturer::~WindowCapturer() = default;

bool WindowCapturer::GetWindowList(WindowList* windows) {
  SourceList sources;
  if (!GetSourceList(&sources)) {
    return false;
  }

  for (const Source& source : sources) {
    windows->push_back({source.id, std::move(source.title)});
  }
  return true;
}

bool WindowCapturer::SelectWindow(WindowId id) {
  return SelectSource(id);
}

bool WindowCapturer::BringSelectedWindowToFront() {
  return FocusOnSelectedSource();
}

bool WindowCapturer::GetSourceList(SourceList* sources) {
  WindowList windows;
  if (!GetWindowList(&windows)) {
    return false;
  }

  for (const Window& window : windows) {
    sources->push_back({window.id, std::move(window.title)});
  }

  return true;
}

bool WindowCapturer::SelectSource(SourceId id) {
  return SelectWindow(id);
}

bool WindowCapturer::FocusOnSelectedSource() {
  return BringSelectedWindowToFront();
}

}  // namespace webrtc
