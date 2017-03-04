/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/webrtcvideoencoderfactory.h"

#include "webrtc/common_types.h"

namespace cricket {

webrtc::VideoEncoder* WebRtcVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  return CreateVideoEncoder(webrtc::PayloadNameToCodecType(codec.name)
                                .value_or(webrtc::kVideoCodecUnknown));
}

const std::vector<cricket::VideoCodec>&
WebRtcVideoEncoderFactory::supported_codecs() const {
  codecs_.clear();
  const std::vector<VideoCodec>& encoder_codecs = codecs();
  for (const VideoCodec& encoder_codec : encoder_codecs) {
    codecs_.push_back(cricket::VideoCodec(encoder_codec.name));
  }
  return codecs_;
}

webrtc::VideoEncoder* WebRtcVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  const cricket::VideoCodec codec(
      webrtc::CodecTypeToPayloadName(type).value_or("Unknown codec"));
  return CreateVideoEncoder(codec);
}

const std::vector<WebRtcVideoEncoderFactory::VideoCodec>&
WebRtcVideoEncoderFactory::codecs() const {
  encoder_codecs_.clear();
  const std::vector<cricket::VideoCodec>& codecs = supported_codecs();
  for (const cricket::VideoCodec& codec : codecs) {
    encoder_codecs_.push_back(
        VideoCodec(webrtc::PayloadNameToCodecType(codec.name)
                       .value_or(webrtc::kVideoCodecUnknown),
                   codec.name));
  }
  return encoder_codecs_;
}

}  // namespace cricket
