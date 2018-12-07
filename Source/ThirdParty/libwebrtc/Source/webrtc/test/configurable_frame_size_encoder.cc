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

#include <string.h>

#include "api/video/encoded_image.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    size_t max_frame_size)
    : callback_(NULL),
      max_frame_size_(max_frame_size),
      current_frame_size_(max_frame_size),
      buffer_(new uint8_t[max_frame_size]),
      codec_type_(kVideoCodecGeneric) {
  memset(buffer_.get(), 0, max_frame_size);
}

ConfigurableFrameSizeEncoder::~ConfigurableFrameSizeEncoder() {}

int32_t ConfigurableFrameSizeEncoder::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Encode(
    const VideoFrame& inputImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const std::vector<FrameType>* frame_types) {
  EncodedImage encodedImage(buffer_.get(), current_frame_size_,
                            max_frame_size_);
  encodedImage._completeFrame = true;
  encodedImage._encodedHeight = inputImage.height();
  encodedImage._encodedWidth = inputImage.width();
  encodedImage._frameType = kVideoFrameKey;
  encodedImage.SetTimestamp(inputImage.timestamp());
  encodedImage.capture_time_ms_ = inputImage.render_time_ms();
  RTPFragmentationHeader* fragmentation = NULL;
  CodecSpecificInfo specific{};
  specific.codecType = codec_type_;
  callback_->OnEncodedImage(encodedImage, &specific, fragmentation);
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

int32_t ConfigurableFrameSizeEncoder::SetRateAllocation(
    const VideoBitrateAllocation& allocation,
    uint32_t framerate) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::SetFrameSize(size_t size) {
  RTC_DCHECK_LE(size, max_frame_size_);
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
