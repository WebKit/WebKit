/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t2.h"
#include "modules/video_coding/codecs/test/encoded_video_frame_producer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::SizeIs;

VideoCodec DefaultCodecSettings() {
  VideoCodec codec_settings;
  codec_settings.width = 320;
  codec_settings.height = 180;
  codec_settings.maxFramerate = 30;
  codec_settings.maxBitrate = 1000;
  codec_settings.qpMax = 63;
  return codec_settings;
}

VideoEncoder::Settings DefaultEncoderSettings() {
  return VideoEncoder::Settings(
      VideoEncoder::Capabilities(/*loss_notification=*/false),
      /*number_of_cores=*/1, /*max_payload_size=*/1200);
}

TEST(LibaomAv1EncoderTest, CanCreate) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  EXPECT_TRUE(encoder);
}

TEST(LibaomAv1EncoderTest, InitAndRelease) {
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  ASSERT_TRUE(encoder);
  VideoCodec codec_settings = DefaultCodecSettings();
  EXPECT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST(LibaomAv1EncoderTest, NoBitrateOnTopLayerRefecltedInActiveDecodeTargets) {
  // Configure encoder with 2 temporal layers.
  std::unique_ptr<VideoEncoder> encoder =
      CreateLibaomAv1Encoder(std::make_unique<ScalabilityStructureL1T2>());
  VideoCodec codec_settings = DefaultCodecSettings();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  VideoEncoder::RateControlParameters rate_parameters;
  rate_parameters.framerate_fps = 30;
  rate_parameters.bitrate.SetBitrate(0, /*temporal_index=*/0, 300'000);
  rate_parameters.bitrate.SetBitrate(0, /*temporal_index=*/1, 0);
  encoder->SetRates(rate_parameters);

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(1).Encode();
  ASSERT_THAT(encoded_frames, SizeIs(1));
  ASSERT_NE(encoded_frames[0].codec_specific_info.generic_frame_info,
            absl::nullopt);
  // Assuming L1T2 structure uses 1st decode target for T0 and 2nd decode target
  // for T0+T1 frames, expect only 1st decode target is active.
  EXPECT_EQ(encoded_frames[0]
                .codec_specific_info.generic_frame_info->active_decode_targets,
            0b01);
}

}  // namespace
}  // namespace webrtc
