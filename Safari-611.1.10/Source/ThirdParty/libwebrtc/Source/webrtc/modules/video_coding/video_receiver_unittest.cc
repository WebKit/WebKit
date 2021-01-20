/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/mock_video_decoder.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/timing.h"
#include "modules/video_coding/video_coding_impl.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

namespace webrtc {
namespace vcm {
namespace {

class MockPacketRequestCallback : public VCMPacketRequestCallback {
 public:
  MOCK_METHOD(int32_t,
              ResendPackets,
              (const uint16_t* sequenceNumbers, uint16_t length),
              (override));
};

class MockVCMReceiveCallback : public VCMReceiveCallback {
 public:
  MockVCMReceiveCallback() {}
  virtual ~MockVCMReceiveCallback() {}

  MOCK_METHOD(int32_t,
              FrameToRender,
              (VideoFrame&, absl::optional<uint8_t>, int32_t, VideoContentType),
              (override));
  MOCK_METHOD(void, OnIncomingPayloadType, (int), (override));
  MOCK_METHOD(void, OnDecoderImplementationName, (const char*), (override));
};

class TestVideoReceiver : public ::testing::Test {
 protected:
  static const int kUnusedPayloadType = 10;
  static const uint16_t kMaxWaitTimeMs = 100;

  TestVideoReceiver()
      : clock_(0), timing_(&clock_), receiver_(&clock_, &timing_) {}

  virtual void SetUp() {
    // Register decoder.
    receiver_.RegisterExternalDecoder(&decoder_, kUnusedPayloadType);
    webrtc::test::CodecSettings(kVideoCodecVP8, &settings_);
    EXPECT_EQ(
        0, receiver_.RegisterReceiveCodec(kUnusedPayloadType, &settings_, 1));

    // Set protection mode.
    const size_t kMaxNackListSize = 250;
    const int kMaxPacketAgeToNack = 450;
    receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);
    EXPECT_EQ(
        0, receiver_.RegisterPacketRequestCallback(&packet_request_callback_));

    // Since we call Decode, we need to provide a valid receive callback.
    // However, for the purposes of these tests, we ignore the callbacks.
    EXPECT_CALL(receive_callback_, OnIncomingPayloadType(_)).Times(AnyNumber());
    EXPECT_CALL(receive_callback_, OnDecoderImplementationName(_))
        .Times(AnyNumber());
    receiver_.RegisterReceiveCallback(&receive_callback_);
  }

  RTPHeader GetDefaultRTPHeader() const {
    RTPHeader header;
    header.markerBit = false;
    header.payloadType = kUnusedPayloadType;
    header.ssrc = 1;
    header.headerLength = 12;
    return header;
  }

  RTPVideoHeader GetDefaultVp8Header() const {
    RTPVideoHeader video_header = {};
    video_header.frame_type = VideoFrameType::kEmptyFrame;
    video_header.codec = kVideoCodecVP8;
    return video_header;
  }

  void InsertAndVerifyPaddingFrame(const uint8_t* payload,
                                   RTPHeader* header,
                                   const RTPVideoHeader& video_header) {
    for (int j = 0; j < 5; ++j) {
      // Padding only packets are passed to the VCM with payload size 0.
      EXPECT_EQ(0, receiver_.IncomingPacket(payload, 0, *header, video_header));
      ++header->sequenceNumber;
    }
    receiver_.Process();
    EXPECT_CALL(decoder_, Decode(_, _, _)).Times(0);
    EXPECT_EQ(VCM_FRAME_NOT_READY, receiver_.Decode(kMaxWaitTimeMs));
  }

  void InsertAndVerifyDecodableFrame(const uint8_t* payload,
                                     size_t length,
                                     RTPHeader* header,
                                     const RTPVideoHeader& video_header) {
    EXPECT_EQ(0,
              receiver_.IncomingPacket(payload, length, *header, video_header));
    ++header->sequenceNumber;
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);

    receiver_.Process();
    EXPECT_CALL(decoder_, Decode(_, _, _)).Times(1);
    EXPECT_EQ(0, receiver_.Decode(kMaxWaitTimeMs));
  }

  SimulatedClock clock_;
  VideoCodec settings_;
  NiceMock<MockVideoDecoder> decoder_;
  NiceMock<MockPacketRequestCallback> packet_request_callback_;
  VCMTiming timing_;
  MockVCMReceiveCallback receive_callback_;
  VideoReceiver receiver_;
};

TEST_F(TestVideoReceiver, PaddingOnlyFrames) {
  const size_t kPaddingSize = 220;
  const uint8_t kPayload[kPaddingSize] = {0};
  RTPHeader header = GetDefaultRTPHeader();
  RTPVideoHeader video_header = GetDefaultVp8Header();
  header.paddingLength = kPaddingSize;
  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);
    InsertAndVerifyPaddingFrame(kPayload, &header, video_header);
    clock_.AdvanceTimeMilliseconds(33);
    header.timestamp += 3000;
  }
}

TEST_F(TestVideoReceiver, PaddingOnlyFramesWithLosses) {
  const size_t kFrameSize = 1200;
  const size_t kPaddingSize = 220;
  const uint8_t kPayload[kFrameSize] = {0};
  RTPHeader header = GetDefaultRTPHeader();
  RTPVideoHeader video_header = GetDefaultVp8Header();
  header.paddingLength = kPaddingSize;
  video_header.video_type_header.emplace<RTPVideoHeaderVP8>();

  // Insert one video frame to get one frame decoded.
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  video_header.is_first_packet_in_frame = true;
  header.markerBit = true;
  InsertAndVerifyDecodableFrame(kPayload, kFrameSize, &header, video_header);

  clock_.AdvanceTimeMilliseconds(33);
  header.timestamp += 3000;
  video_header.frame_type = VideoFrameType::kEmptyFrame;
  video_header.is_first_packet_in_frame = false;
  header.markerBit = false;
  // Insert padding frames.
  for (int i = 0; i < 10; ++i) {
    // Lose one packet from the 6th frame.
    if (i == 5) {
      ++header.sequenceNumber;
    }
    // Lose the 4th frame.
    if (i == 3) {
      header.sequenceNumber += 5;
    } else {
      if (i > 3 && i < 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 5)).Times(1);
      } else if (i >= 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 6)).Times(1);
      } else {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);
      }
      InsertAndVerifyPaddingFrame(kPayload, &header, video_header);
    }
    clock_.AdvanceTimeMilliseconds(33);
    header.timestamp += 3000;
  }
}

TEST_F(TestVideoReceiver, PaddingOnlyAndVideo) {
  const size_t kFrameSize = 1200;
  const size_t kPaddingSize = 220;
  const uint8_t kPayload[kFrameSize] = {0};
  RTPHeader header = GetDefaultRTPHeader();
  RTPVideoHeader video_header = GetDefaultVp8Header();
  video_header.is_first_packet_in_frame = false;
  header.paddingLength = kPaddingSize;
  auto& vp8_header =
      video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.pictureId = -1;
  vp8_header.tl0PicIdx = -1;

  for (int i = 0; i < 3; ++i) {
    // Insert 2 video frames.
    for (int j = 0; j < 2; ++j) {
      if (i == 0 && j == 0)  // First frame should be a key frame.
        video_header.frame_type = VideoFrameType::kVideoFrameKey;
      else
        video_header.frame_type = VideoFrameType::kVideoFrameDelta;
      video_header.is_first_packet_in_frame = true;
      header.markerBit = true;
      InsertAndVerifyDecodableFrame(kPayload, kFrameSize, &header,
                                    video_header);
      clock_.AdvanceTimeMilliseconds(33);
      header.timestamp += 3000;
    }

    // Insert 2 padding only frames.
    video_header.frame_type = VideoFrameType::kEmptyFrame;
    video_header.is_first_packet_in_frame = false;
    header.markerBit = false;
    for (int j = 0; j < 2; ++j) {
      // InsertAndVerifyPaddingFrame(kPayload, &header);
      clock_.AdvanceTimeMilliseconds(33);
      header.timestamp += 3000;
    }
  }
}

}  // namespace
}  // namespace vcm
}  // namespace webrtc
