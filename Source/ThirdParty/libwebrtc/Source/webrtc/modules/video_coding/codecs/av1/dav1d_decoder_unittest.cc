/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/dav1d_decoder.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "api/array_view.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/video/color_space.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Eq;
using ::testing::Not;
using ::testing::NotNull;

constexpr uint8_t kAv1FrameWith36x20EncodededAnd32x16RenderResolution[] = {
    0x12, 0x00, 0x0a, 0x06, 0x18, 0x15, 0x23, 0x9f, 0x60, 0x10, 0x32, 0x18,
    0x20, 0x03, 0xe0, 0x01, 0xf2, 0xb0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    0x00, 0xf2, 0x44, 0xd6, 0xa5, 0x3b, 0x7c, 0x8b, 0x7c, 0x8c, 0x6b, 0x9a};

constexpr uint8_t kAv1FrameWithBT709FullRangeColorSpace[] = {
    0x12, 0x00, 0x0a, 0x0d, 0x00, 0x00, 0x00, 0x02, 0xaf, 0xff, 0x89, 0x5f,
    0x22, 0x02, 0x02, 0x03, 0x08, 0x32, 0x0e, 0x10, 0x00, 0xac, 0x02, 0x05,
    0x14, 0x20, 0x81, 0x00, 0x02, 0x00, 0x95, 0xe1, 0xe0};

EncodedImage CreateEncodedImage(ArrayView<const uint8_t> data) {
  EncodedImage image;
  image.SetEncodedData(EncodedImageBuffer::Create(data.data(), data.size()));
  return image;
}

class TestAv1Decoder : public DecodedImageCallback {
 public:
  explicit TestAv1Decoder(const Environment& env)
      : decoder_(CreateDav1dDecoder(env)) {
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create decoder";
      return;
    }
    EXPECT_TRUE(decoder_->Configure({}));
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(this),
              WEBRTC_VIDEO_CODEC_OK);
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Decoder(const TestAv1Decoder&) = delete;
  TestAv1Decoder& operator=(const TestAv1Decoder&) = delete;

  int32_t Decode(const EncodedImage& image) {
    decoded_frame_ = std::nullopt;
    return decoder_->Decode(image, /*render_time_ms=*/image.capture_time_ms_);
  }

  VideoFrame& decoded_frame() { return *decoded_frame_; }

 private:
  int32_t Decoded(VideoFrame& decoded_frame) override {
    decoded_frame_ = std::move(decoded_frame);
    return 0;
  }
  void Decoded(VideoFrame& decoded_frame,
               std::optional<int32_t> /*decode_time_ms*/,
               std::optional<uint8_t> /*qp*/) override {
    Decoded(decoded_frame);
  }

  const std::unique_ptr<VideoDecoder> decoder_;
  std::optional<VideoFrame> decoded_frame_;
};

TEST(Dav1dDecoderTest, KeepsDecodedResolutionByDefault) {
  TestAv1Decoder decoder(CreateEnvironment());
  EXPECT_EQ(decoder.Decode(CreateEncodedImage(
                kAv1FrameWith36x20EncodededAnd32x16RenderResolution)),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(decoder.decoded_frame().width(), 36);
  EXPECT_EQ(decoder.decoded_frame().height(), 20);
}

TEST(Dav1dDecoderTest, CropsToRenderResolutionWhenCropIsEnabled) {
  TestAv1Decoder decoder(CreateEnvironment(CreateTestFieldTrialsPtr(
      "WebRTC-Dav1dDecoder-CropToRenderResolution/Enabled/")));
  EXPECT_EQ(decoder.Decode(CreateEncodedImage(
                kAv1FrameWith36x20EncodededAnd32x16RenderResolution)),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(decoder.decoded_frame().width(), 32);
  EXPECT_EQ(decoder.decoded_frame().height(), 16);
}

TEST(Dav1dDecoderTest, DoesNotCropToRenderResolutionWhenCropIsDisabled) {
  TestAv1Decoder decoder(CreateEnvironment(CreateTestFieldTrialsPtr(
      "WebRTC-Dav1dDecoder-CropToRenderResolution/Disabled/")));
  EXPECT_EQ(decoder.Decode(CreateEncodedImage(
                kAv1FrameWith36x20EncodededAnd32x16RenderResolution)),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(decoder.decoded_frame().width(), 36);
  EXPECT_EQ(decoder.decoded_frame().height(), 20);
}

TEST(Dav1dDecoderTest, SetsColorSpaceOnDecodedFrame) {
  TestAv1Decoder decoder(CreateEnvironment());
  EXPECT_EQ(
      decoder.Decode(CreateEncodedImage(kAv1FrameWithBT709FullRangeColorSpace)),
      WEBRTC_VIDEO_CODEC_OK);
  auto color_space = decoder.decoded_frame().color_space();
  EXPECT_TRUE(color_space.has_value());
  EXPECT_EQ(color_space->primaries(), ColorSpace::PrimaryID::kBT709);
  EXPECT_EQ(color_space->transfer(), ColorSpace::TransferID::kBT709);
  EXPECT_EQ(color_space->matrix(), ColorSpace::MatrixID::kBT709);
  EXPECT_EQ(color_space->range(), ColorSpace::RangeID::kFull);
}

TEST(Dav1dDecoderTest, FailDecoderOnTiles) {
  // This test tests buffer management, any failures would be detected
  // by running this test under ASAN.
  static constexpr uint8_t kTile0[] = {
      0x0a, 0x0a, 0x00, 0x00, 0x00, 0x04, 0x3c, 0xff, 0xbc, 0x01, 0xa0, 0x08,
      0x32, 0xbd, 0x01, 0x10, 0x60, 0xb0, 0x01, 0xa6, 0x9a, 0x68, 0x50, 0x91,
      0xb0, 0x80, 0xb0, 0xd9, 0x8a, 0x40, 0xe4, 0x3e, 0xfb, 0x06, 0x1f, 0x0e,
      0xcc, 0xdc, 0xcb, 0x8c, 0x2d, 0x5c, 0xb0, 0x52, 0xe0, 0xb3, 0x89, 0x2b,
      0x6b, 0x8b, 0x56, 0xd3, 0x0a, 0x70, 0xad, 0x26, 0xf0, 0xca, 0xfa, 0xad,
      0x27, 0xcc, 0x5b, 0x8c, 0x0c, 0x72, 0x76, 0xa7, 0x42, 0x69, 0x4c, 0x47,
      0xf4, 0x2b, 0xa9, 0x5e, 0x13, 0x81, 0x69, 0x47, 0x40, 0x13, 0x3e, 0xac,
      0xef, 0x0f, 0x74, 0x02, 0x79, 0xa7, 0x50, 0xf4, 0x50, 0x82, 0x4d, 0x4f,
      0x70, 0x02, 0x9c, 0x0f, 0x61, 0xbc, 0x1c, 0x22, 0xc2, 0xac, 0x9f, 0x88,
      0xbd, 0x2a, 0xdc, 0xdb, 0xf0, 0xbe, 0x95, 0x54, 0x6e, 0xda, 0x08, 0x76,
      0x2c, 0x86, 0xd2, 0x0f, 0x86, 0xc1, 0x86, 0xcb, 0x98, 0x35, 0xfc, 0x2c,
      0xe7, 0x51, 0x44, 0x62, 0xfe, 0xf1, 0x97, 0x1f, 0xb0, 0x7f, 0x14, 0xc4,
      0xef, 0xb0, 0x01, 0x7c, 0x07, 0x22, 0x3e, 0xda, 0x2b, 0xef, 0x59, 0xcc,
      0x57, 0x65, 0xc5, 0x22, 0xe9, 0x61, 0x99, 0x0c, 0xd6, 0x07, 0x68, 0x1a,
      0x26, 0xad, 0x9b, 0x1e, 0x91, 0x2c, 0xee, 0xe2, 0xe4, 0x7c, 0x4d, 0x07,
      0x51, 0x72, 0x89, 0x51, 0xbf, 0x17, 0x67, 0xc0, 0x6d, 0x02, 0x69, 0x92,
      0x20, 0x7b, 0xeb, 0x11, 0xea, 0xc1, 0x43, 0x59, 0x4f, 0x75, 0xbb, 0x20};
  static constexpr uint8_t kTile1[] = {
      0x22, 0x1e, 0xe0, 0xc1, 0xb6, 0x2f, 0x07, 0xd0, 0x0b, 0xaf, 0x14,
      0xbf, 0x51, 0xa3, 0x0d, 0xf4, 0xb6, 0x45, 0xd8, 0x16, 0xda, 0x8c,
      0xb0, 0xbf, 0x29, 0x39, 0xdf, 0x9a, 0x9e, 0x9c, 0x69, 0x80};

  TestAv1Decoder decoder(CreateEnvironment());
  decoder.Decode(CreateEncodedImage(kTile0));
  decoder.Decode(CreateEncodedImage(kTile1));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
