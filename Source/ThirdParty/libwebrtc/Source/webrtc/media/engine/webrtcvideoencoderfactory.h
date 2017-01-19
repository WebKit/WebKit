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

class WebRtcVideoEncoderFactory {
 public:
  // This VideoCodec class is deprecated. Use cricket::VideoCodec directly
  // instead and the corresponding factory function. See
  // http://crbug/webrtc/6402 for more info.
  struct VideoCodec {
    webrtc::VideoCodecType type;
    std::string name;

    VideoCodec(webrtc::VideoCodecType t, const std::string& nm)
        : type(t), name(nm) {}

    VideoCodec(webrtc::VideoCodecType t,
               const std::string& nm,
               int w,
               int h,
               int fr)
        : type(t), name(nm) {}
  };

  virtual ~WebRtcVideoEncoderFactory() {}

  // TODO(magjed): Make these functions pure virtual when every external client
  // implements it. See http://crbug/webrtc/6402 for more info.
  // Caller takes the ownership of the returned object and it should be released
  // by calling DestroyVideoEncoder().
  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec);

  // Returns a list of supported codecs in order of preference.
  virtual const std::vector<cricket::VideoCodec>& supported_codecs() const;

  // Caller takes the ownership of the returned object and it should be released
  // by calling DestroyVideoEncoder().
  // Deprecated: Use cricket::VideoCodec as argument instead. See
  // http://crbug/webrtc/6402 for more info.
  virtual webrtc::VideoEncoder* CreateVideoEncoder(webrtc::VideoCodecType type);

  // Returns a list of supported codecs in order of preference.
  // Deprecated: Return cricket::VideoCodecs instead. See
  // http://crbug/webrtc/6402 for more info.
  virtual const std::vector<VideoCodec>& codecs() const;

  // Returns true if encoders created by this factory of the given codec type
  // will use internal camera sources, meaning that they don't require/expect
  // frames to be delivered via webrtc::VideoEncoder::Encode. This flag is used
  // as the internal_source parameter to
  // webrtc::ViEExternalCodec::RegisterExternalSendCodec.
  virtual bool EncoderTypeHasInternalSource(webrtc::VideoCodecType type) const {
    return false;
  }

  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) = 0;

 private:
  // TODO(magjed): Remove these. They are necessary in order to return a const
  // reference to a std::vector in the default implementations of codecs() and
  // supported_codecs(). See http://crbug/webrtc/6402 for more info.
  mutable std::vector<VideoCodec> encoder_codecs_;
  mutable std::vector<cricket::VideoCodec> codecs_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOENCODERFACTORY_H_
