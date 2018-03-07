/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_
#define MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_

#include <vector>

#include "media/base/mediachannel.h"
#include "media/base/mediaengine.h"

namespace webrtc {

class Call;

}  // namespace webrtc


namespace cricket {

class VideoMediaChannel;
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;

// Video engine implementation that does nothing and can be used in
// CompositeMediaEngine.
class NullWebRtcVideoEngine {
 public:
  std::vector<VideoCodec> codecs() const { return std::vector<VideoCodec>(); }

  RtpCapabilities GetCapabilities() const { return RtpCapabilities(); }

  VideoMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const VideoOptions& options) {
    return nullptr;
  }
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_
