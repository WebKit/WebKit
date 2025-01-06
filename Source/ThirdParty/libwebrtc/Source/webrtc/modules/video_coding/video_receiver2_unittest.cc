/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_receiver2.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "api/test/mock_video_decoder.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "common_video/test/utilities.h"
#include "modules/video_coding/decoder_database.h"
#include "modules/video_coding/timing/timing.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockVCMReceiveCallback : public VCMReceiveCallback {
 public:
  MockVCMReceiveCallback() = default;

  MOCK_METHOD(int32_t,
              FrameToRender,
              (VideoFrame&,
               std::optional<uint8_t>,
               TimeDelta,
               VideoContentType,
               VideoFrameType),
              (override));
  MOCK_METHOD(int32_t,
              OnFrameToRender,
              (const struct FrameToRender&),
              (override));
  MOCK_METHOD(void, OnIncomingPayloadType, (int), (override));
  MOCK_METHOD(void,
              OnDecoderInfoChanged,
              (const VideoDecoder::DecoderInfo&),
              (override));
};

class TestEncodedFrame : public EncodedFrame {
 public:
  explicit TestEncodedFrame(int payload_type) {
    _payloadType = payload_type;
    SetPacketInfos(CreatePacketInfos(3));
  }

  void SetReceivedTime(webrtc::Timestamp received_time) {
    received_time_ = received_time;
  }

  int64_t ReceivedTime() const override { return received_time_.ms(); }

  int64_t RenderTime() const override { return _renderTimeMs; }

 private:
  webrtc::Timestamp received_time_ = webrtc::Timestamp::Millis(0);
};

class VideoReceiver2Test : public ::testing::Test {
 protected:
  VideoReceiver2Test() {
    receiver_.RegisterReceiveCallback(&receive_callback_);
  }

  void RegisterReceiveCodecSettings(
      int payload_type,
      VideoCodecType codec_type = kVideoCodecVP8) {
    VideoDecoder::Settings settings;
    settings.set_codec_type(codec_type);
    settings.set_max_render_resolution({10, 10});
    settings.set_number_of_cores(4);
    receiver_.RegisterReceiveCodec(payload_type, settings);
  }

  test::ScopedKeyValueConfig field_trials_;
  SimulatedClock clock_{Timestamp::Millis(1337)};
  VCMTiming timing_{&clock_, field_trials_};
  NiceMock<MockVCMReceiveCallback> receive_callback_;
  VideoReceiver2 receiver_{&clock_, &timing_, field_trials_,
                           /*corruption_score_calculator=*/nullptr};
};

TEST_F(VideoReceiver2Test, RegisterExternalDecoder) {
  constexpr int kPayloadType = 1;
  ASSERT_FALSE(receiver_.IsExternalDecoderRegistered(kPayloadType));

  // Register a decoder, check for correctness, then unregister and check again.
  auto decoder = std::make_unique<NiceMock<MockVideoDecoder>>();
  bool decoder_deleted = false;
  EXPECT_CALL(*decoder, Destruct).WillOnce([&decoder_deleted] {
    decoder_deleted = true;
  });
  receiver_.RegisterExternalDecoder(std::move(decoder), kPayloadType);
  EXPECT_TRUE(receiver_.IsExternalDecoderRegistered(kPayloadType));
  receiver_.RegisterExternalDecoder(nullptr, kPayloadType);
  EXPECT_TRUE(decoder_deleted);
  EXPECT_FALSE(receiver_.IsExternalDecoderRegistered(kPayloadType));
}

TEST_F(VideoReceiver2Test, RegisterReceiveCodecs) {
  constexpr int kPayloadType = 1;

  RegisterReceiveCodecSettings(kPayloadType);

  TestEncodedFrame frame(kPayloadType);

  // A decoder has not been registered yet, so an attempt to decode should fail.
  EXPECT_EQ(receiver_.Decode(&frame), VCM_NO_CODEC_REGISTERED);

  // Register a decoder that will accept the Decode operation.
  auto decoder = std::make_unique<NiceMock<MockVideoDecoder>>();
  EXPECT_CALL(*decoder, RegisterDecodeCompleteCallback)
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*decoder, Decode(_, _)).WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*decoder, Release).WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  // Register the decoder. Note that this moves ownership of the mock object
  // to the `receiver_`.
  receiver_.RegisterExternalDecoder(std::move(decoder), kPayloadType);
  EXPECT_TRUE(receiver_.IsExternalDecoderRegistered(kPayloadType));

  EXPECT_CALL(receive_callback_, OnIncomingPayloadType(kPayloadType));
  EXPECT_CALL(receive_callback_, OnDecoderInfoChanged);

  // Call `Decode`. This triggers the above call expectations.
  EXPECT_EQ(receiver_.Decode(&frame), VCM_OK);

  // Unregister the decoder and verify.
  receiver_.RegisterExternalDecoder(nullptr, kPayloadType);
  EXPECT_FALSE(receiver_.IsExternalDecoderRegistered(kPayloadType));

  receiver_.DeregisterReceiveCodec(kPayloadType);
}

}  // namespace
}  // namespace webrtc
