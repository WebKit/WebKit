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
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_decoder.h"
#include "webrtc/sdk/objc/Framework/Classes/h264_video_toolbox_encoder.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
const char kHighProfileExperiment[] = "WebRTC-H264HighProfile";

bool IsHighProfileEnabled() {
  return field_trial::IsEnabled(kHighProfileExperiment);
}
}

// VideoToolboxVideoEncoderFactory

VideoToolboxVideoEncoderFactory::VideoToolboxVideoEncoderFactory() {
}

VideoToolboxVideoEncoderFactory::~VideoToolboxVideoEncoderFactory() {}

VideoEncoder* VideoToolboxVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  if (FindMatchingCodec(supported_codecs_, codec)) {
    LOG(LS_INFO) << "Creating HW encoder for " << codec.name;
    return new H264VideoToolboxEncoder(codec);
  }
  LOG(LS_INFO) << "No HW encoder found for codec " << codec.name;
  return nullptr;
}

void VideoToolboxVideoEncoderFactory::DestroyVideoEncoder(
    VideoEncoder* encoder) {
  delete encoder;
  encoder = nullptr;
}

const std::vector<cricket::VideoCodec>&
VideoToolboxVideoEncoderFactory::supported_codecs() const {
  supported_codecs_.clear();

  // TODO(magjed): Enumerate actual level instead of using hardcoded level 3.1.
  // Level 3.1 is 1280x720@30fps which is enough for now.
  const H264::Level level = H264::kLevel3_1;

  if (IsHighProfileEnabled()) {
    cricket::VideoCodec constrained_high(cricket::kH264CodecName);
    const H264::ProfileLevelId constrained_high_profile(
        H264::kProfileConstrainedHigh, level);
    constrained_high.SetParam(
        cricket::kH264FmtpProfileLevelId,
        *H264::ProfileLevelIdToString(constrained_high_profile));
    constrained_high.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
    constrained_high.SetParam(cricket::kH264FmtpPacketizationMode, "1");
    supported_codecs_.push_back(constrained_high);
  }

  cricket::VideoCodec constrained_baseline(cricket::kH264CodecName);
  const H264::ProfileLevelId constrained_baseline_profile(
      H264::kProfileConstrainedBaseline, level);
  constrained_baseline.SetParam(
      cricket::kH264FmtpProfileLevelId,
      *H264::ProfileLevelIdToString(constrained_baseline_profile));
  constrained_baseline.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
  constrained_baseline.SetParam(cricket::kH264FmtpPacketizationMode, "1");
  supported_codecs_.push_back(constrained_baseline);

  return supported_codecs_;
}

// VideoToolboxVideoDecoderFactory

VideoToolboxVideoDecoderFactory::VideoToolboxVideoDecoderFactory() {
  supported_codecs_.push_back(cricket::VideoCodec("H264"));
}

VideoToolboxVideoDecoderFactory::~VideoToolboxVideoDecoderFactory() {}

VideoDecoder* VideoToolboxVideoDecoderFactory::CreateVideoDecoder(
    VideoCodecType type) {
  const rtc::Optional<const char*> codec_name = CodecTypeToPayloadName(type);
  if (!codec_name) {
    LOG(LS_ERROR) << "Invalid codec type: " << type;
    return nullptr;
  }
  const cricket::VideoCodec codec(*codec_name);
  if (FindMatchingCodec(supported_codecs_, codec)) {
    LOG(LS_INFO) << "Creating HW decoder for " << codec.name;
    return new H264VideoToolboxDecoder();
  }
  LOG(LS_INFO) << "No HW decoder found for codec " << codec.name;
  return nullptr;
}

void VideoToolboxVideoDecoderFactory::DestroyVideoDecoder(
    VideoDecoder* decoder) {
  delete decoder;
  decoder = nullptr;
}

}  // namespace webrtc
