/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/video_coding/include/mock/mock_vcm_callbacks.h"
#include "webrtc/modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/test/test_util.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using ::testing::Return;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::AllOf;
using ::testing::Args;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::NiceMock;
using ::testing::Sequence;

class VCMRobustnessTest : public ::testing::Test {
 protected:
  static const size_t kPayloadLen = 10;

  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    ASSERT_TRUE(clock_.get() != NULL);
    vcm_.reset(VideoCodingModule::Create(clock_.get(), &event_factory_));
    ASSERT_TRUE(vcm_ != NULL);
    const size_t kMaxNackListSize = 250;
    const int kMaxPacketAgeToNack = 450;
    vcm_->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);
    ASSERT_EQ(0, vcm_->RegisterFrameTypeCallback(&frame_type_callback_));
    ASSERT_EQ(0, vcm_->RegisterPacketRequestCallback(&request_callback_));
    VideoCodingModule::Codec(kVideoCodecVP8, &video_codec_);
    ASSERT_EQ(VCM_OK, vcm_->RegisterReceiveCodec(&video_codec_, 1));
    vcm_->RegisterExternalDecoder(&decoder_, video_codec_.plType);
  }

  virtual void TearDown() { vcm_.reset(); }

  void InsertPacket(uint32_t timestamp,
                    uint16_t seq_no,
                    bool first,
                    bool marker_bit,
                    FrameType frame_type) {
    const uint8_t payload[kPayloadLen] = {0};
    WebRtcRTPHeader rtp_info;
    memset(&rtp_info, 0, sizeof(rtp_info));
    rtp_info.frameType = frame_type;
    rtp_info.header.timestamp = timestamp;
    rtp_info.header.sequenceNumber = seq_no;
    rtp_info.header.markerBit = marker_bit;
    rtp_info.header.payloadType = video_codec_.plType;
    rtp_info.type.Video.codec = kRtpVideoVp8;
    rtp_info.type.Video.codecHeader.VP8.InitRTPVideoHeaderVP8();
    rtp_info.type.Video.is_first_packet_in_frame = first;

    ASSERT_EQ(VCM_OK, vcm_->IncomingPacket(payload, kPayloadLen, rtp_info));
  }

  std::unique_ptr<VideoCodingModule> vcm_;
  VideoCodec video_codec_;
  MockVCMFrameTypeCallback frame_type_callback_;
  MockPacketRequestCallback request_callback_;
  NiceMock<MockVideoDecoder> decoder_;
  NiceMock<MockVideoDecoder> decoderCopy_;
  std::unique_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
};

TEST_F(VCMRobustnessTest, TestHardNack) {
  Sequence s;
  EXPECT_CALL(request_callback_, ResendPackets(_, 2))
      .With(Args<0, 1>(ElementsAre(6, 7)))
      .Times(1);
  for (int ts = 0; ts <= 6000; ts += 3000) {
    EXPECT_CALL(decoder_,
                Decode(AllOf(Field(&EncodedImage::_timeStamp, ts),
                             Field(&EncodedImage::_length, kPayloadLen * 3),
                             Field(&EncodedImage::_completeFrame, true)),
                       false, _, _, _))
        .Times(1)
        .InSequence(s);
  }

  ASSERT_EQ(VCM_OK, vcm_->SetReceiverRobustnessMode(
                        VideoCodingModule::kHardNack, kNoErrors));

  InsertPacket(0, 0, true, false, kVideoFrameKey);
  InsertPacket(0, 1, false, false, kVideoFrameKey);
  InsertPacket(0, 2, false, true, kVideoFrameKey);
  clock_->AdvanceTimeMilliseconds(1000 / 30);

  InsertPacket(3000, 3, true, false, kVideoFrameDelta);
  InsertPacket(3000, 4, false, false, kVideoFrameDelta);
  InsertPacket(3000, 5, false, true, kVideoFrameDelta);
  clock_->AdvanceTimeMilliseconds(1000 / 30);

  ASSERT_EQ(VCM_OK, vcm_->Decode(0));
  ASSERT_EQ(VCM_OK, vcm_->Decode(0));
  ASSERT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));

  clock_->AdvanceTimeMilliseconds(10);

  vcm_->Process();

  ASSERT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));

  InsertPacket(6000, 8, false, true, kVideoFrameDelta);
  clock_->AdvanceTimeMilliseconds(10);
  vcm_->Process();

  ASSERT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));

  InsertPacket(6000, 6, true, false, kVideoFrameDelta);
  InsertPacket(6000, 7, false, false, kVideoFrameDelta);
  clock_->AdvanceTimeMilliseconds(10);
  vcm_->Process();

  ASSERT_EQ(VCM_OK, vcm_->Decode(0));
}

TEST_F(VCMRobustnessTest, TestHardNackNoneDecoded) {
  EXPECT_CALL(request_callback_, ResendPackets(_, _)).Times(0);
  EXPECT_CALL(frame_type_callback_, RequestKeyFrame()).Times(1);

  ASSERT_EQ(VCM_OK, vcm_->SetReceiverRobustnessMode(
                        VideoCodingModule::kHardNack, kNoErrors));

  InsertPacket(3000, 3, true, false, kVideoFrameDelta);
  InsertPacket(3000, 4, false, false, kVideoFrameDelta);
  InsertPacket(3000, 5, false, true, kVideoFrameDelta);

  EXPECT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));
  vcm_->Process();

  clock_->AdvanceTimeMilliseconds(10);

  EXPECT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));
  vcm_->Process();
}

TEST_F(VCMRobustnessTest, TestModeNoneWithErrors) {
  EXPECT_CALL(decoder_, InitDecode(_, _)).Times(1);
  EXPECT_CALL(decoder_, Release()).Times(1);
  Sequence s1;
  EXPECT_CALL(request_callback_, ResendPackets(_, 1))
      .With(Args<0, 1>(ElementsAre(4)))
      .Times(0);

  EXPECT_CALL(decoder_, Copy()).Times(0);
  EXPECT_CALL(decoderCopy_, Copy()).Times(0);

  // Decode operations
  EXPECT_CALL(decoder_,
              Decode(AllOf(Field(&EncodedImage::_timeStamp, 0),
                           Field(&EncodedImage::_completeFrame, true)),
                     false, _, _, _))
      .Times(1)
      .InSequence(s1);
  EXPECT_CALL(decoder_,
              Decode(AllOf(Field(&EncodedImage::_timeStamp, 3000),
                           Field(&EncodedImage::_completeFrame, false)),
                     false, _, _, _))
      .Times(1)
      .InSequence(s1);
  EXPECT_CALL(decoder_,
              Decode(AllOf(Field(&EncodedImage::_timeStamp, 6000),
                           Field(&EncodedImage::_completeFrame, true)),
                     false, _, _, _))
      .Times(1)
      .InSequence(s1);
  EXPECT_CALL(decoder_,
              Decode(AllOf(Field(&EncodedImage::_timeStamp, 9000),
                           Field(&EncodedImage::_completeFrame, true)),
                     false, _, _, _))
      .Times(1)
      .InSequence(s1);

  ASSERT_EQ(VCM_OK, vcm_->SetReceiverRobustnessMode(VideoCodingModule::kNone,
                                                    kWithErrors));

  InsertPacket(0, 0, true, false, kVideoFrameKey);
  InsertPacket(0, 1, false, false, kVideoFrameKey);
  InsertPacket(0, 2, false, true, kVideoFrameKey);
  EXPECT_EQ(VCM_OK, vcm_->Decode(33));  // Decode timestamp 0.
  vcm_->Process();

  clock_->AdvanceTimeMilliseconds(33);
  InsertPacket(3000, 3, true, false, kVideoFrameDelta);
  // Packet 4 missing
  InsertPacket(3000, 5, false, true, kVideoFrameDelta);
  EXPECT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));
  vcm_->Process();

  clock_->AdvanceTimeMilliseconds(33);
  InsertPacket(6000, 6, true, false, kVideoFrameDelta);
  InsertPacket(6000, 7, false, false, kVideoFrameDelta);
  InsertPacket(6000, 8, false, true, kVideoFrameDelta);
  EXPECT_EQ(VCM_OK, vcm_->Decode(0));  // Decode timestamp 3000 incomplete.
  vcm_->Process();

  clock_->AdvanceTimeMilliseconds(10);
  EXPECT_EQ(VCM_OK, vcm_->Decode(23));  // Decode timestamp 6000 complete.
  vcm_->Process();

  clock_->AdvanceTimeMilliseconds(23);
  InsertPacket(3000, 4, false, false, kVideoFrameDelta);

  InsertPacket(9000, 9, true, false, kVideoFrameDelta);
  InsertPacket(9000, 10, false, false, kVideoFrameDelta);
  InsertPacket(9000, 11, false, true, kVideoFrameDelta);
  EXPECT_EQ(VCM_OK, vcm_->Decode(33));  // Decode timestamp 9000 complete.
}
}  // namespace webrtc
