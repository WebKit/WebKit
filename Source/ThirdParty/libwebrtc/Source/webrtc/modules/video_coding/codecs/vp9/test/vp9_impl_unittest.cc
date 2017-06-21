/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_coding/codecs/test/video_codec_test.h"

namespace webrtc {

namespace {
constexpr uint32_t kTimestampIncrementPerFrame = 3000;
}  // namespace

class TestVp9Impl : public VideoCodecTest {
 protected:
  VideoEncoder* CreateEncoder() override { return VP9Encoder::Create(); }

  VideoDecoder* CreateDecoder() override { return VP9Decoder::Create(); }

  VideoCodec codec_settings() override {
    VideoCodec codec_settings;
    codec_settings.codecType = webrtc::kVideoCodecVP9;
    codec_settings.VP9()->numberOfTemporalLayers = 1;
    codec_settings.VP9()->numberOfSpatialLayers = 1;
    return codec_settings;
  }

  void ExpectFrameWith(int16_t picture_id,
                       int tl0_pic_idx,
                       uint8_t temporal_idx) {
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
    EXPECT_EQ(picture_id, codec_specific_info.codecSpecific.VP9.picture_id);
    EXPECT_EQ(tl0_pic_idx, codec_specific_info.codecSpecific.VP9.tl0_pic_idx);
    EXPECT_EQ(temporal_idx, codec_specific_info.codecSpecific.VP9.temporal_idx);
  }
};

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST_F(TestVp9Impl, DISABLED_EncodeDecode) {
#else
TEST_F(TestVp9Impl, EncodeDecode) {
#endif
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

// We only test the encoder here, since the decoded frame rotation is set based
// on the CVO RTP header extension in VCMDecodedFrameCallback::Decoded.
// TODO(brandtr): Consider passing through the rotation flag through the decoder
// in the same way as done in the encoder.
TEST_F(TestVp9Impl, EncodedRotationEqualsInputRotation) {
  input_frame_->set_rotation(kVideoRotation_0);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_0, encoded_frame.rotation_);

  input_frame_->set_rotation(kVideoRotation_90);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoRotation_90, encoded_frame.rotation_);
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
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

TEST_F(TestVp9Impl, ParserQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  int qp = 0;
  ASSERT_TRUE(vp9::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));

  EXPECT_EQ(encoded_frame.qp_, qp);
}

TEST_F(TestVp9Impl, EncoderRetainsRtpStateAfterRelease) {
  // Override default settings.
  codec_settings_.VP9()->numberOfTemporalLayers = 2;
  // Tl0PidIdx is only used in non-flexible mode.
  codec_settings_.VP9()->flexibleMode = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Temporal layer 0.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  int16_t picture_id = codec_specific_info.codecSpecific.VP9.picture_id;
  int tl0_pic_idx = codec_specific_info.codecSpecific.VP9.tl0_pic_idx;
  EXPECT_EQ(0, codec_specific_info.codecSpecific.VP9.temporal_idx);

  // Temporal layer 1.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 1) % (1 << 15), tl0_pic_idx, 1);

  // Temporal layer 0.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 2) % (1 << 15), (tl0_pic_idx + 1) % (1 << 8),
                  0);

  // Temporal layer 1.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 3) % (1 << 15), (tl0_pic_idx + 1) % (1 << 8),
                  1);

  // Reinit.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));

  // Temporal layer 0.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 4) % (1 << 15), (tl0_pic_idx + 2) % (1 << 8),
                  0);

  // Temporal layer 1.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 5) % (1 << 15), (tl0_pic_idx + 2) % (1 << 8),
                  1);

  // Temporal layer 0.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 6) % (1 << 15), (tl0_pic_idx + 3) % (1 << 8),
                  0);

  // Temporal layer 1.
  input_frame_->set_timestamp(input_frame_->timestamp() +
                              kTimestampIncrementPerFrame);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  ExpectFrameWith((picture_id + 7) % (1 << 15), (tl0_pic_idx + 3) % (1 << 8),
                  1);
}

}  // namespace webrtc
