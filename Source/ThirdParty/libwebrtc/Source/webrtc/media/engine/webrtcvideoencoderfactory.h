/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/media/base/codec.h"

namespace webrtc {
class VideoEncoder;
}

namespace cricket {

class WEBRTC_DYLIB_EXPORT WebRtcVideoEncoderFactory {
 public:
  virtual ~WebRtcVideoEncoderFactory() {}

  // Caller takes the ownership of the returned object and it should be released
  // by calling DestroyVideoEncoder().
  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) = 0;

  // Returns a list of supported codecs in order of preference.
  virtual const std::vector<cricket::VideoCodec>& supported_codecs() const = 0;

  // Returns true if encoders created by this factory of the given codec type
  // will use internal camera sources, meaning that they don't require/expect
  // frames to be delivered via webrtc::VideoEncoder::Encode. This flag is used
  // as the internal_source parameter to
  // webrtc::ViEExternalCodec::RegisterExternalSendCodec.
  virtual bool EncoderTypeHasInternalSource(webrtc::VideoCodecType) const {
    return false;
  }

  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) = 0;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_
