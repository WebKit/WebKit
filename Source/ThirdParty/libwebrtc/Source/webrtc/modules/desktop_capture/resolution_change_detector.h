/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_CHANGE_DETECTOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_CHANGE_DETECTOR_H_

#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

class ResolutionChangeDetector {
 public:
  bool IsChanged(DesktopSize size);
  void Reset();

 private:
  DesktopSize last_size_;
  bool initialized_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_CHANGE_DETECTOR_H_
