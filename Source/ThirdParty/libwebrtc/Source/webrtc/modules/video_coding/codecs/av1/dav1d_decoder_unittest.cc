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
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/explicit_key_value_config.h"
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

  void Decode(const EncodedImage& image) {
    ASSERT_THAT(decoder_, NotNull());
    decoded_frame_ = std::nullopt;
    int32_t error =
        decoder_->Decode(image, /*render_time_ms=*/image.capture_time_ms_);
    ASSERT_EQ(error, WEBRTC_VIDEO_CODEC_OK);
    ASSERT_THAT(decoded_frame_, Not(Eq(std::nullopt)));
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
  decoder.Decode(
      CreateEncodedImage(kAv1FrameWith36x20EncodededAnd32x16RenderResolution));
  EXPECT_EQ(decoder.decoded_frame().width(), 36);
  EXPECT_EQ(decoder.decoded_frame().height(), 20);
}

TEST(Dav1dDecoderTest, CropsToRenderResolutionWhenCropIsEnabled) {
  TestAv1Decoder decoder(
      CreateEnvironment(std::make_unique<ExplicitKeyValueConfig>(
          "WebRTC-Dav1dDecoder-CropToRenderResolution/Enabled/")));
  decoder.Decode(
      CreateEncodedImage(kAv1FrameWith36x20EncodededAnd32x16RenderResolution));
  EXPECT_EQ(decoder.decoded_frame().width(), 32);
  EXPECT_EQ(decoder.decoded_frame().height(), 16);
}

TEST(Dav1dDecoderTest, DoesNotCropToRenderResolutionWhenCropIsDisabled) {
  TestAv1Decoder decoder(
      CreateEnvironment(std::make_unique<ExplicitKeyValueConfig>(
          "WebRTC-Dav1dDecoder-CropToRenderResolution/Disabled/")));
  decoder.Decode(
      CreateEncodedImage(kAv1FrameWith36x20EncodededAnd32x16RenderResolution));
  EXPECT_EQ(decoder.decoded_frame().width(), 36);
  EXPECT_EQ(decoder.decoded_frame().height(), 20);
}

}  // namespace
}  // namespace test
}  // namespace webrtc
