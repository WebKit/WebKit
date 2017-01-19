/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/sdk/objc/Framework/Classes/videotoolboxvideocodecfactory.h"

#include "webrtc/base/logging.h"
#include "webrtc/common_video/h264/profile_level_id.h"
#include "webrtc/media/base/codec.h"
#if defined(WEBRTC_IOS)
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_encoder.h"
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_decoder.h"
#endif

// TODO(kthelgason): delete this when CreateVideoDecoder takes
// a cricket::VideoCodec instead of webrtc::VideoCodecType.
static const char* NameFromCodecType(webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::kVideoCodecVP8:
      return cricket::kVp8CodecName;
    case webrtc::kVideoCodecVP9:
      return cricket::kVp9CodecName;
    case webrtc::kVideoCodecH264:
      return cricket::kH264CodecName;
    default:
      return "Unknown codec";
  }
}

namespace webrtc {

// VideoToolboxVideoEncoderFactory

VideoToolboxVideoEncoderFactory::VideoToolboxVideoEncoderFactory() {
// Hardware H264 encoding only supported on iOS for now.
#if defined(WEBRTC_IOS)
  // TODO(magjed): Push Constrained High profile as well when negotiation is
  // ready, http://crbug/webrtc/6337.
  cricket::VideoCodec constrained_baseline(cricket::kH264CodecName);
  // TODO(magjed): Enumerate actual level instead of using hardcoded level 3.1.
  // Level 3.1 is 1280x720@30fps which is enough for now.
  const H264::ProfileLevelId constrained_baseline_profile(
      H264::kProfileConstrainedBaseline, H264::kLevel3_1);
  constrained_baseline.SetParam(
      cricket::kH264FmtpProfileLevelId,
      *H264::ProfileLevelIdToString(constrained_baseline_profile));
  constrained_baseline.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
  constrained_baseline.SetParam(cricket::kH264FmtpPacketizationMode, "1");
  supported_codecs_.push_back(constrained_baseline);
#endif
}

VideoToolboxVideoEncoderFactory::~VideoToolboxVideoEncoderFactory() {}

VideoEncoder* VideoToolboxVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
#if defined(WEBRTC_IOS)
  if (FindMatchingCodec(supported_codecs_, codec)) {
    LOG(LS_INFO) << "Creating HW encoder for " << codec.name;
    return new H264VideoToolboxEncoder(codec);
  }
#endif
  LOG(LS_INFO) << "No HW encoder found for codec " << codec.name;
  return nullptr;
}

void VideoToolboxVideoEncoderFactory::DestroyVideoEncoder(
    VideoEncoder* encoder) {
#if defined(WEBRTC_IOS)
  delete encoder;
  encoder = nullptr;
#endif
}

const std::vector<cricket::VideoCodec>&
VideoToolboxVideoEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

// VideoToolboxVideoDecoderFactory

VideoToolboxVideoDecoderFactory::VideoToolboxVideoDecoderFactory() {
#if defined(WEBRTC_IOS)
  supported_codecs_.push_back(cricket::VideoCodec("H264"));
#endif
}

VideoToolboxVideoDecoderFactory::~VideoToolboxVideoDecoderFactory() {}

VideoDecoder* VideoToolboxVideoDecoderFactory::CreateVideoDecoder(
    VideoCodecType type) {
  const auto codec = cricket::VideoCodec(NameFromCodecType(type));
#if defined(WEBRTC_IOS)
  if (FindMatchingCodec(supported_codecs_, codec)) {
    LOG(LS_INFO) << "Creating HW decoder for " << codec.name;
    return new H264VideoToolboxDecoder();
  }
#endif
  LOG(LS_INFO) << "No HW decoder found for codec " << codec.name;
  return nullptr;
}

void VideoToolboxVideoDecoderFactory::DestroyVideoDecoder(
    VideoDecoder* decoder) {
#if defined(WEBRTC_IOS)
  delete decoder;
  decoder = nullptr;
#endif
}

}  // namespace webrtc
