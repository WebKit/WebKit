/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/internaldecoderfactory.h"

#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

namespace cricket {

namespace {

// Video decoder class to be used for unknown codecs. Doesn't support decoding
// but logs messages to LS_ERROR.
class NullVideoDecoder : public webrtc::VideoDecoder {
 public:
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    LOG(LS_ERROR)
        << "Can't register decode complete callback on NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  const char* ImplementationName() const override { return "NullVideoDecoder"; }
};

}  // anonymous namespace

InternalDecoderFactory::InternalDecoderFactory() {}

InternalDecoderFactory::~InternalDecoderFactory() {}

// WebRtcVideoDecoderFactory implementation.
webrtc::VideoDecoder* InternalDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  switch (type) {
    case webrtc::kVideoCodecH264:
      if (webrtc::H264Decoder::IsSupported())
        return webrtc::H264Decoder::Create();
      // This could happen in a software-fallback for a codec type only
      // supported externally (e.g. H.264 on iOS or Android) or in current usage
      // in WebRtcVideoEngine2 if the external decoder fails to be created.
      LOG(LS_ERROR) << "Unable to create an H.264 decoder fallback. "
                    << "Decoding of this stream will be broken.";
      return new NullVideoDecoder();
    case webrtc::kVideoCodecVP8:
      return webrtc::VP8Decoder::Create();
    case webrtc::kVideoCodecVP9:
      RTC_DCHECK(webrtc::VP9Decoder::IsSupported());
      return webrtc::VP9Decoder::Create();
    default:
      LOG(LS_ERROR) << "Creating NullVideoDecoder for unsupported codec.";
      return new NullVideoDecoder();
  }
}

void InternalDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  delete decoder;
}

}  // namespace cricket
