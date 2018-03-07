/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_
#define MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_

#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "media/base/codec.h"

namespace webrtc {
class VideoEncoder;
}

namespace cricket {

// Deprecated. Use webrtc::VideoEncoderFactory instead.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7925
class WebRtcVideoEncoderFactory {
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
  // frames to be delivered via webrtc::VideoEncoder::Encode.
  virtual bool EncoderTypeHasInternalSource(webrtc::VideoCodecType type) const;

  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) = 0;
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_
