/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/stereo/include/stereo_decoder_adapter.h"
#include "modules/video_coding/codecs/stereo/include/stereo_encoder_adapter.h"
#include "modules/video_coding/codecs/test/video_codec_test.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/ptr_util.h"

using testing::_;
using testing::Return;

namespace webrtc {

constexpr const char* kStereoAssociatedCodecName = cricket::kVp9CodecName;
const VideoCodecType kStereoAssociatedCodecType =
    PayloadStringToCodecType(kStereoAssociatedCodecName);

class TestStereoAdapter : public VideoCodecTest {
 public:
  TestStereoAdapter()
      : decoder_factory_(new webrtc::MockVideoDecoderFactory),
        encoder_factory_(new webrtc::MockVideoEncoderFactory) {}

 protected:
  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return rtc::MakeUnique<StereoDecoderAdapter>(
        decoder_factory_.get(), SdpVideoFormat(kStereoAssociatedCodecName));
  }

  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return rtc::MakeUnique<StereoEncoderAdapter>(
        encoder_factory_.get(), SdpVideoFormat(kStereoAssociatedCodecName));
  }

  VideoCodec codec_settings() override {
    VideoCodec codec_settings;
    codec_settings.codecType = kStereoAssociatedCodecType;
    codec_settings.VP9()->numberOfTemporalLayers = 1;
    codec_settings.VP9()->numberOfSpatialLayers = 1;
    codec_settings.codecType = webrtc::kVideoCodecStereo;
    return codec_settings;
  }

  std::unique_ptr<VideoFrame> CreateI420AInputFrame() {
    rtc::scoped_refptr<webrtc::I420BufferInterface> yuv_buffer =
        input_frame_->video_frame_buffer()->ToI420();
    rtc::scoped_refptr<I420ABufferInterface> yuva_buffer = WrapI420ABuffer(
        yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
        yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
        yuv_buffer->DataV(), yuv_buffer->StrideV(), yuv_buffer->DataY(),
        yuv_buffer->StrideY(), rtc::KeepRefUntilDone(yuv_buffer));
    return rtc::WrapUnique<VideoFrame>(
        new VideoFrame(yuva_buffer, kVideoRotation_0, 0));
  }

 private:
  void SetUp() override {
    EXPECT_CALL(*decoder_factory_, Die());
    // The decoders/encoders will be owned by the caller of
    // CreateVideoDecoder()/CreateVideoEncoder().
    VideoDecoder* decoder1 = VP9Decoder::Create().release();
    VideoDecoder* decoder2 = VP9Decoder::Create().release();
    EXPECT_CALL(*decoder_factory_, CreateVideoDecoderProxy(_))
        .WillOnce(Return(decoder1))
        .WillOnce(Return(decoder2));

    EXPECT_CALL(*encoder_factory_, Die());
    VideoEncoder* encoder1 = VP9Encoder::Create().release();
    VideoEncoder* encoder2 = VP9Encoder::Create().release();
    EXPECT_CALL(*encoder_factory_, CreateVideoEncoderProxy(_))
        .WillOnce(Return(encoder1))
        .WillOnce(Return(encoder2));

    VideoCodecTest::SetUp();
  }

  const std::unique_ptr<webrtc::MockVideoDecoderFactory> decoder_factory_;
  const std::unique_ptr<webrtc::MockVideoEncoderFactory> encoder_factory_;
};

// TODO(emircan): Currently VideoCodecTest tests do a complete setup
// step that goes beyond constructing |decoder_|. Simplify these tests to do
// less.
TEST_F(TestStereoAdapter, ConstructAndDestructDecoder) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
}

TEST_F(TestStereoAdapter, ConstructAndDestructEncoder) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
}

TEST_F(TestStereoAdapter, EncodeDecodeI420Frame) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

  EXPECT_EQ(kVideoCodecStereo, codec_specific_info.codecType);
  EXPECT_EQ(kStereoAssociatedCodecType,
            codec_specific_info.codecSpecific.stereo.associated_codec_type);
  EXPECT_EQ(0, codec_specific_info.codecSpecific.stereo.indices.frame_index);
  EXPECT_EQ(1, codec_specific_info.codecSpecific.stereo.indices.frame_count);
  EXPECT_EQ(0ull,
            codec_specific_info.codecSpecific.stereo.indices.picture_index);

  EXPECT_EQ(
      WEBRTC_VIDEO_CODEC_OK,
      decoder_->Decode(encoded_frame, false, nullptr, &codec_specific_info));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame_.get(), decoded_frame.get()), 36);
}

TEST_F(TestStereoAdapter, EncodeDecodeI420AFrame) {
  std::unique_ptr<VideoFrame> yuva_frame = CreateI420AInputFrame();
  const size_t expected_num_encoded_frames = 2;
  SetWaitForEncodedFramesThreshold(expected_num_encoded_frames);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*yuva_frame, nullptr, nullptr));
  std::vector<EncodedImage> encoded_frames;
  std::vector<CodecSpecificInfo> codec_specific_infos;
  ASSERT_TRUE(WaitForEncodedFrames(&encoded_frames, &codec_specific_infos));
  EXPECT_EQ(expected_num_encoded_frames, encoded_frames.size());
  EXPECT_EQ(expected_num_encoded_frames, codec_specific_infos.size());

  const CodecSpecificInfo& yuv_info = codec_specific_infos[kYUVStream];
  EXPECT_EQ(kVideoCodecStereo, yuv_info.codecType);
  EXPECT_EQ(kStereoAssociatedCodecType,
            yuv_info.codecSpecific.stereo.associated_codec_type);
  EXPECT_EQ(kYUVStream, yuv_info.codecSpecific.stereo.indices.frame_index);
  EXPECT_EQ(kAlphaCodecStreams,
            yuv_info.codecSpecific.stereo.indices.frame_count);
  EXPECT_EQ(0ull, yuv_info.codecSpecific.stereo.indices.picture_index);

  const CodecSpecificInfo& axx_info = codec_specific_infos[kAXXStream];
  EXPECT_EQ(kVideoCodecStereo, axx_info.codecType);
  EXPECT_EQ(kStereoAssociatedCodecType,
            axx_info.codecSpecific.stereo.associated_codec_type);
  EXPECT_EQ(kAXXStream, axx_info.codecSpecific.stereo.indices.frame_index);
  EXPECT_EQ(kAlphaCodecStreams,
            axx_info.codecSpecific.stereo.indices.frame_count);
  EXPECT_EQ(0ull, axx_info.codecSpecific.stereo.indices.picture_index);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frames[kYUVStream],
                                                    false, nullptr, &yuv_info));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encoded_frames[kAXXStream],
                                                    false, nullptr, &axx_info));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(yuva_frame.get(), decoded_frame.get()), 36);
}

}  // namespace webrtc
