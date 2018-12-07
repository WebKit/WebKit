/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_vp8_decoder.h"

#include "api/video/i420_buffer.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace test {

namespace {
// Read width and height from the payload of the frame if it is a key frame the
// same way as the real VP8 decoder.
// FakeEncoder writes width, height and frame type.
void ParseFakeVp8(const unsigned char* data, int* width, int* height) {
  bool key_frame = data[0] == 0;
  if (key_frame) {
    *width = ((data[7] << 8) + data[6]) & 0x3FFF;
    *height = ((data[9] << 8) + data[8]) & 0x3FFF;
  }
}
}  // namespace

FakeVp8Decoder::FakeVp8Decoder() : callback_(nullptr), width_(0), height_(0) {}

int32_t FakeVp8Decoder::InitDecode(const VideoCodec* config,
                                   int32_t number_of_cores) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVp8Decoder::Decode(const EncodedImage& input,
                               bool missing_frames,
                               const CodecSpecificInfo* codec_specific_info,
                               int64_t render_time_ms) {
  constexpr size_t kMinPayLoadHeaderLength = 10;
  if (input._length < kMinPayLoadHeaderLength) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ParseFakeVp8(input._buffer, &width_, &height_);

  VideoFrame frame(I420Buffer::Create(width_, height_),
                   webrtc::kVideoRotation_0,
                   render_time_ms * rtc::kNumMicrosecsPerMillisec);
  frame.set_timestamp(input.Timestamp());
  frame.set_ntp_time_ms(input.ntp_time_ms_);

  callback_->Decoded(frame, /*decode_time_ms=*/absl::nullopt,
                     /*qp=*/absl::nullopt);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVp8Decoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVp8Decoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* FakeVp8Decoder::kImplementationName = "fake_vp8_decoder";
const char* FakeVp8Decoder::ImplementationName() const {
  return kImplementationName;
}

}  // namespace test
}  // namespace webrtc
