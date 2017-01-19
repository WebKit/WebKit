/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_encoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

namespace webrtc {
VideoEncoder* VideoEncoder::Create(VideoEncoder::EncoderType codec_type) {
  RTC_DCHECK(IsSupportedSoftware(codec_type));
  switch (codec_type) {
    case kH264:
      return H264Encoder::Create();
    case kVp8:
      return VP8Encoder::Create();
    case kVp9:
      return VP9Encoder::Create();
    case kUnsupportedCodec:
      RTC_NOTREACHED();
      return nullptr;
  }
  RTC_NOTREACHED();
  return nullptr;
}

bool VideoEncoder::IsSupportedSoftware(EncoderType codec_type) {
  switch (codec_type) {
    case kH264:
      return H264Encoder::IsSupported();
    case kVp8:
      return true;
    case kVp9:
      return VP9Encoder::IsSupported();
    case kUnsupportedCodec:
      RTC_NOTREACHED();
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

VideoEncoder::EncoderType VideoEncoder::CodecToEncoderType(
    VideoCodecType codec_type) {
  switch (codec_type) {
    case kVideoCodecH264:
      return VideoEncoder::kH264;
    case kVideoCodecVP8:
      return VideoEncoder::kVp8;
    case kVideoCodecVP9:
      return VideoEncoder::kVp9;
    default:
      return VideoEncoder::kUnsupportedCodec;
  }
}

}  // namespace webrtc
