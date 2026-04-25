/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/configurable_frame_size_encoder.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

#include "api/fec_controller_override.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {
namespace test {

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    size_t max_frame_size)
    : callback_(nullptr),
      current_frame_size_(max_frame_size),
      codec_type_(kVideoCodecGeneric) {}

ConfigurableFrameSizeEncoder::~ConfigurableFrameSizeEncoder() {}

void ConfigurableFrameSizeEncoder::SetFecControllerOverride(
    FecControllerOverride* fec_controller_override) {
  // Ignored.
}

int32_t ConfigurableFrameSizeEncoder::InitEncode(
    const VideoCodec* codec_settings,
    const Settings& settings) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Encode(
    const VideoFrame& inputImage,
    const std::vector<VideoFrameType>* frame_types) {
  EncodedImage encodedImage;
  auto buffer = EncodedImageBuffer::Create(current_frame_size_);
  memset(buffer->data(), 0, current_frame_size_);
  encodedImage.SetEncodedData(buffer);
  encodedImage._encodedHeight = inputImage.height();
  encodedImage._encodedWidth = inputImage.width();
  encodedImage.set_frame_type(VideoFrameType::kVideoFrameKey);
  encodedImage.SetRtpTimestamp(inputImage.rtp_timestamp());
  encodedImage.capture_time_ms_ = inputImage.render_time_ms();
  CodecSpecificInfo specific{};
  specific.codecType = codec_type_;
  callback_->OnEncodedImage(encodedImage, &specific);
  if (post_encode_callback_) {
    (*post_encode_callback_)();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

void ConfigurableFrameSizeEncoder::SetRates(
    const RateControlParameters& parameters) {}

VideoEncoder::EncoderInfo ConfigurableFrameSizeEncoder::GetEncoderInfo() const {
  return EncoderInfo();
}

int32_t ConfigurableFrameSizeEncoder::SetFrameSize(size_t size) {
  current_frame_size_ = size;
  return WEBRTC_VIDEO_CODEC_OK;
}

void ConfigurableFrameSizeEncoder::SetCodecType(VideoCodecType codec_type) {
  codec_type_ = codec_type;
}

void ConfigurableFrameSizeEncoder::RegisterPostEncodeCallback(
    std::function<void(void)> post_encode_callback) {
  post_encode_callback_ = std::move(post_encode_callback);
}

}  // namespace test
}  // namespace webrtc
