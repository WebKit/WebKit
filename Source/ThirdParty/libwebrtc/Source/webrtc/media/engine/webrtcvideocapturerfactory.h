/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(pthatcher): Rename file to match class name.
#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOCAPTURERFACTORY_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOCAPTURERFACTORY_H_

#include "webrtc/media/base/videocapturerfactory.h"

namespace cricket {

// Creates instances of cricket::WebRtcVideoCapturer.
class WebRtcVideoDeviceCapturerFactory : public VideoDeviceCapturerFactory {
 public:
  std::unique_ptr<VideoCapturer> Create(const Device& device) override;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOCAPTURERFACTORY_H_
