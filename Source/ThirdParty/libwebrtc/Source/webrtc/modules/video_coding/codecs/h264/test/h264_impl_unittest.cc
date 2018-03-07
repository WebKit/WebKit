/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/test/video_codec_test.h"

namespace webrtc {

class TestH264Impl : public VideoCodecTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return H264Encoder::Create(cricket::VideoCodec(cricket::kH264CodecName));
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return H264Decoder::Create();
  }

  VideoCodec codec_settings() override {
    VideoCodec codec_inst;
    codec_inst.codecType = webrtc::kVideoCodecH264;
    // If frame dropping is false, we get a warning that bitrate can't
    // be controlled for RC_QUALITY_MODE; RC_BITRATE_MODE and RC_TIMESTAMP_MODE
    codec_inst.H264()->frameDroppingOn = true;
    return codec_inst;
  }
};

#ifdef WEBRTC_USE_H264
#define MAYBE_EncodeDecode EncodeDecode
#define MAYBE_DecodedQpEqualsEncodedQp DecodedQpEqualsEncodedQp
#else
#define MAYBE_EncodeDecode DISABLED_EncodeDecode
#define MAYBE_DecodedQpEqualsEncodedQp DISABLED_DecodedQpEqualsEncodedQp
#endif

TEST_F(TestH264Impl, MAYBE_EncodeDecode) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame_.get(), decoded_frame.get()), 36);
}

TEST_F(TestH264Impl, MAYBE_DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

}  // namespace webrtc
