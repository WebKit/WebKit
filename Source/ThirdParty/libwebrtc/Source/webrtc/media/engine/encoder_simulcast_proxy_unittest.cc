/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "media/engine/encoder_simulcast_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {
namespace {
const VideoEncoder::Capabilities kCapabilities(false);
const VideoEncoder::Settings kSettings(kCapabilities, 4, 1200);
}  // namespace

using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

TEST(EncoderSimulcastProxy, ChoosesCorrectImplementation) {
  const std::string kImplementationName = "Fake";
  const std::string kSimulcastAdaptedImplementationName =
      "SimulcastEncoderAdapter (Fake, Fake, Fake)";
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  codec_settings.simulcastStream[0] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       2000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.simulcastStream[1] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       3000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.simulcastStream[2] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       5000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.numberOfSimulcastStreams = 3;

  auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.implementation_name = kImplementationName;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(kImplementationName,
            simulcast_enabled_proxy.GetEncoderInfo().implementation_name);

  NiceMock<MockVideoEncoderFactory> nonsimulcast_factory;

  EXPECT_CALL(nonsimulcast_factory, CreateVideoEncoder)
      .Times(4)
      .WillOnce([&] {
        auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
        EXPECT_CALL(*mock_encoder, InitEncode(_, _))
            .WillOnce(Return(
                WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED));
        ON_CALL(*mock_encoder, GetEncoderInfo)
            .WillByDefault(Return(encoder_info));
        return mock_encoder;
      })
      .WillRepeatedly([&] {
        auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
        EXPECT_CALL(*mock_encoder, InitEncode(_, _))
            .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
        ON_CALL(*mock_encoder, GetEncoderInfo)
            .WillByDefault(Return(encoder_info));
        return mock_encoder;
      });

  EncoderSimulcastProxy simulcast_disabled_proxy(&nonsimulcast_factory,
                                                 SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_disabled_proxy.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(kSimulcastAdaptedImplementationName,
            simulcast_disabled_proxy.GetEncoderInfo().implementation_name);

  // Cleanup.
  simulcast_enabled_proxy.Release();
  simulcast_disabled_proxy.Release();
}

TEST(EncoderSimulcastProxy, ForwardsTrustedSetting) {
  auto mock_encoder_owned = std::make_unique<NiceMock<MockVideoEncoder>>();
  auto* mock_encoder = mock_encoder_owned.get();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder_owned))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));

  VideoEncoder::EncoderInfo info;
  info.has_trusted_rate_controller = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillRepeatedly(Return(info));

  EXPECT_TRUE(
      simulcast_enabled_proxy.GetEncoderInfo().has_trusted_rate_controller);
}

TEST(EncoderSimulcastProxy, ForwardsHardwareAccelerated) {
  auto mock_encoder_owned = std::make_unique<NiceMock<MockVideoEncoder>>();
  NiceMock<MockVideoEncoder>* mock_encoder = mock_encoder_owned.get();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder_owned))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));

  VideoEncoder::EncoderInfo info;

  info.is_hardware_accelerated = false;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_FALSE(
      simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);

  info.is_hardware_accelerated = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_TRUE(simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);
}

TEST(EncoderSimulcastProxy, ForwardsInternalSource) {
  auto mock_encoder_owned = std::make_unique<NiceMock<MockVideoEncoder>>();
  NiceMock<MockVideoEncoder>* mock_encoder = mock_encoder_owned.get();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder_owned))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));

  VideoEncoder::EncoderInfo info;

  info.has_internal_source = false;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_FALSE(simulcast_enabled_proxy.GetEncoderInfo().has_internal_source);

  info.has_internal_source = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_TRUE(simulcast_enabled_proxy.GetEncoderInfo().has_internal_source);
}

}  // namespace testing
}  // namespace webrtc
