/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOCAPTURERFACTORY_H_
#define WEBRTC_MEDIA_BASE_VIDEOCAPTURERFACTORY_H_

#include "webrtc/media/base/device.h"

namespace cricket {

class VideoCapturer;

class VideoDeviceCapturerFactory {
 public:
  VideoDeviceCapturerFactory() {}
  virtual ~VideoDeviceCapturerFactory() {}

  virtual VideoCapturer* Create(const Device& device) = 0;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOCAPTURERFACTORY_H_
