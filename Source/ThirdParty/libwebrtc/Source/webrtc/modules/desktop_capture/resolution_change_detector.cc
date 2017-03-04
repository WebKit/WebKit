/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/resolution_change_detector.h"

namespace webrtc {

bool ResolutionChangeDetector::IsChanged(DesktopSize size) {
  if (!initialized_) {
    initialized_ = true;
    last_size_ = size;
    return false;
  }

  return !last_size_.equals(size);
}

void ResolutionChangeDetector::Reset() {
  initialized_ = false;
}

}  // namespace webrtc
