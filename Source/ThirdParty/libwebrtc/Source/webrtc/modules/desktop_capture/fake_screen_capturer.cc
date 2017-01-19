/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/fake_screen_capturer.h"

#include <stdint.h>

#include <utility>

namespace webrtc {

FakeScreenCapturer::FakeScreenCapturer()
    : FakeDesktopCapturer(),
      // A random number for the fake screen.
      id_(1378277498) {}
FakeScreenCapturer::~FakeScreenCapturer() {}

bool FakeScreenCapturer::GetScreenList(ScreenList* list) {
  list->push_back(Screen{id_});
  return true;
}

bool FakeScreenCapturer::SelectScreen(ScreenId id) {
  return id == kFullDesktopScreenId || id == id_;
}

}  // namespace webrtc
