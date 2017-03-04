/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_decoder.h"

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {

FakeDecoder::FakeDecoder() : callback_(NULL) {}

int32_t FakeDecoder::InitDecode(const VideoCodec* config,
                                int32_t number_of_cores) {
  config_ = *config;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Decode(const EncodedImage& input,
                            bool missing_frames,
                            const RTPFragmentationHeader* fragmentation,
                            const CodecSpecificInfo* codec_specific_info,
                            int64_t render_time_ms) {
  VideoFrame frame(I420Buffer::Create(config_.width, config_.height),
                   webrtc::kVideoRotation_0,
                   render_time_ms * rtc::kNumMicrosecsPerMillisec);
  frame.set_timestamp(input._timeStamp);
  frame.set_ntp_time_ms(input.ntp_time_ms_);

  callback_->Decoded(frame);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* FakeDecoder::kImplementationName = "fake_decoder";
const char* FakeDecoder::ImplementationName() const {
  return kImplementationName;
}

int32_t FakeH264Decoder::Decode(const EncodedImage& input,
                                bool missing_frames,
                                const RTPFragmentationHeader* fragmentation,
                                const CodecSpecificInfo* codec_specific_info,
                                int64_t render_time_ms) {
  uint8_t value = 0;
  for (size_t i = 0; i < input._length; ++i) {
    uint8_t kStartCode[] = {0, 0, 0, 1};
    if (i < input._length - sizeof(kStartCode) &&
        !memcmp(&input._buffer[i], kStartCode, sizeof(kStartCode))) {
      i += sizeof(kStartCode) + 1;  // Skip start code and NAL header.
    }
    if (input._buffer[i] != value) {
      EXPECT_EQ(value, input._buffer[i])
          << "Bitstream mismatch between sender and receiver.";
      return -1;
    }
    ++value;
  }
  return FakeDecoder::Decode(input,
                             missing_frames,
                             fragmentation,
                             codec_specific_info,
                             render_time_ms);
}

}  // namespace test
}  // namespace webrtc
