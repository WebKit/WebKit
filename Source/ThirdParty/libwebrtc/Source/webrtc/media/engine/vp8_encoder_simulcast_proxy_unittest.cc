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

#include "media/engine/vp8_encoder_simulcast_proxy.h"

#include <string>

#include "api/test/mock_video_encoder_factory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {

using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;

class MockEncoder : public VideoEncoder {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  MockEncoder() {}
  virtual ~MockEncoder() {}

  MOCK_METHOD3(InitEncode,
               int32_t(const VideoCodec* codec_settings,
                       int32_t number_of_cores,
                       size_t max_payload_size));

  MOCK_METHOD1(RegisterEncodeCompleteCallback, int32_t(EncodedImageCallback*));

  MOCK_METHOD0(Release, int32_t());

  MOCK_METHOD3(
      Encode,
      int32_t(const VideoFrame& inputImage,
              const CodecSpecificInfo* codecSpecificInfo,
              const std::vector<FrameType>* frame_types) /* override */);

  MOCK_METHOD2(SetChannelParameters, int32_t(uint32_t packetLoss, int64_t rtt));

  MOCK_CONST_METHOD0(ImplementationName, const char*());
};

TEST(VP8EncoderSimulcastProxy, ChoosesCorrectImplementation) {
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

  NiceMock<MockEncoder>* mock_encoder = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder, ImplementationName())
      .WillRepeatedly(Return(kImplementationName.c_str()));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoderProxy(_))
      .Times(1)
      .WillOnce(Return(mock_encoder));

  VP8EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                   SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kImplementationName, simulcast_enabled_proxy.ImplementationName());

  NiceMock<MockEncoder>* mock_encoder1 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder2 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder3 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder4 = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> nonsimulcast_factory;

  EXPECT_CALL(*mock_encoder1, InitEncode(_, _, _))
      .WillOnce(
          Return(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED));
  EXPECT_CALL(*mock_encoder1, ImplementationName())
      .WillRepeatedly(Return(kImplementationName.c_str()));

  EXPECT_CALL(*mock_encoder2, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder2, ImplementationName())
      .WillRepeatedly(Return(kImplementationName.c_str()));

  EXPECT_CALL(*mock_encoder3, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder3, ImplementationName())
      .WillRepeatedly(Return(kImplementationName.c_str()));

  EXPECT_CALL(*mock_encoder4, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder4, ImplementationName())
      .WillRepeatedly(Return(kImplementationName.c_str()));

  EXPECT_CALL(nonsimulcast_factory, CreateVideoEncoderProxy(_))
      .Times(4)
      .WillOnce(Return(mock_encoder1))
      .WillOnce(Return(mock_encoder2))
      .WillOnce(Return(mock_encoder3))
      .WillOnce(Return(mock_encoder4));

  VP8EncoderSimulcastProxy simulcast_disabled_proxy(&nonsimulcast_factory,
                                                    SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_disabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastAdaptedImplementationName,
            simulcast_disabled_proxy.ImplementationName());

  // Cleanup.
  simulcast_enabled_proxy.Release();
  simulcast_disabled_proxy.Release();
}

}  // namespace testing
}  // namespace webrtc
