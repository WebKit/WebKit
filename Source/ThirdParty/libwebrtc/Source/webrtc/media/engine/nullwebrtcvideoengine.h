/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_
#define WEBRTC_MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_

#include <vector>

#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/mediaengine.h"

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
  NullWebRtcVideoEngine() {}
  ~NullWebRtcVideoEngine() {}

  void SetExternalDecoderFactory(WebRtcVideoDecoderFactory* decoder_factory) {}
  void SetExternalEncoderFactory(WebRtcVideoEncoderFactory* encoder_factory) {}

  void Init() {}

  const std::vector<VideoCodec>& codecs() {
    return codecs_;
  }

  RtpCapabilities GetCapabilities() {
    RtpCapabilities capabilities;
    return capabilities;
  }

  VideoMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const VideoOptions& options) {
    return nullptr;
  }

 private:
  std::vector<VideoCodec> codecs_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_NULLWEBRTCVIDEOENGINE_H_
