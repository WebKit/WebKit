/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <memory>
#include <utility>

namespace webrtc {

ScreenCapturer::~ScreenCapturer() = default;

bool ScreenCapturer::GetScreenList(ScreenList* screens) {
  SourceList sources;
  if (!GetSourceList(&sources)) {
    return false;
  }

  for (const Source& source : sources) {
    screens->push_back({source.id});
  }
  return true;
}

bool ScreenCapturer::SelectScreen(ScreenId id) {
  return SelectSource(id);
}

bool ScreenCapturer::GetSourceList(SourceList* sources) {
  ScreenList screens;
  if (!GetScreenList(&screens)) {
    return false;
  }

  for (const Screen& screen : screens) {
    sources->push_back({screen.id});
  }

  return true;
}

bool ScreenCapturer::SelectSource(SourceId id) {
  return SelectScreen(id);
}

}  // namespace webrtc
