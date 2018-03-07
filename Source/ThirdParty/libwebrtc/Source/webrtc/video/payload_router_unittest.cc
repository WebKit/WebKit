/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "call/video_config.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/payload_router.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Unused;

namespace webrtc {
namespace {
const int8_t kPayloadType = 96;
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const int16_t kPictureId = 123;
const int16_t kTl0PicIdx = 20;
const uint8_t kTemporalIdx = 1;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialPictureId2 = 44;
}  // namespace

TEST(PayloadRouterTest, SendOnOneModule) {
  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules(1, &rtp);

  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image._timeStamp = 1;
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;

  PayloadRouter payload_router(modules, {kSsrc1}, kPayloadType, {});

  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(0);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(true);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(false);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(0);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);

  payload_router.SetActive(true);
  EXPECT_CALL(rtp, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                    encoded_image._timeStamp,
                                    encoded_image.capture_time_ms_, &payload,
                                    encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, nullptr, nullptr).error);
}

TEST(PayloadRouterTest, SendSimulcast) {
  NiceMock<MockRtpRtcp> rtp_1;
  NiceMock<MockRtpRtcp> rtp_2;
  std::vector<RtpRtcp*> modules = {&rtp_1, &rtp_2};

  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image._timeStamp = 1;
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;

  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});

  CodecSpecificInfo codec_info_1;
  memset(&codec_info_1, 0, sizeof(CodecSpecificInfo));
  codec_info_1.codecType = kVideoCodecVP8;
  codec_info_1.codecSpecific.VP8.simulcastIdx = 0;

  payload_router.SetActive(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                      encoded_image._timeStamp,
                                      encoded_image.capture_time_ms_, &payload,
                                      encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);

  CodecSpecificInfo codec_info_2;
  memset(&codec_info_2, 0, sizeof(CodecSpecificInfo));
  codec_info_2.codecType = kVideoCodecVP8;
  codec_info_2.codecSpecific.VP8.simulcastIdx = 1;

  EXPECT_CALL(rtp_2, SendOutgoingData(encoded_image._frameType, kPayloadType,
                                      encoded_image._timeStamp,
                                      encoded_image.capture_time_ms_, &payload,
                                      encoded_image._length, nullptr, _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_2, nullptr)
                .error);

  // Inactive.
  payload_router.SetActive(false);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_1, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            payload_router.OnEncodedImage(encoded_image, &codec_info_2, nullptr)
                .error);
}

TEST(PayloadRouterTest, SimulcastTargetBitrate) {
  NiceMock<MockRtpRtcp> rtp_1;
  NiceMock<MockRtpRtcp> rtp_2;
  std::vector<RtpRtcp*> modules = {&rtp_1, &rtp_2};

  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  payload_router.SetActive(true);

  BitrateAllocation bitrate;
  bitrate.SetBitrate(0, 0, 10000);
  bitrate.SetBitrate(0, 1, 20000);
  bitrate.SetBitrate(1, 0, 40000);
  bitrate.SetBitrate(1, 1, 80000);

  BitrateAllocation layer0_bitrate;
  layer0_bitrate.SetBitrate(0, 0, 10000);
  layer0_bitrate.SetBitrate(0, 1, 20000);

  BitrateAllocation layer1_bitrate;
  layer1_bitrate.SetBitrate(0, 0, 40000);
  layer1_bitrate.SetBitrate(0, 1, 80000);

  EXPECT_CALL(rtp_1, SetVideoBitrateAllocation(layer0_bitrate)).Times(1);
  EXPECT_CALL(rtp_2, SetVideoBitrateAllocation(layer1_bitrate)).Times(1);

  payload_router.OnBitrateAllocationUpdated(bitrate);
}

TEST(PayloadRouterTest, SimulcastTargetBitrateWithInactiveStream) {
  // Set up two active rtp modules.
  NiceMock<MockRtpRtcp> rtp_1;
  NiceMock<MockRtpRtcp> rtp_2;
  std::vector<RtpRtcp*> modules = {&rtp_1, &rtp_2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  payload_router.SetActive(true);

  // Create bitrate allocation with bitrate only for the first stream.
  BitrateAllocation bitrate;
  bitrate.SetBitrate(0, 0, 10000);
  bitrate.SetBitrate(0, 1, 20000);

  // Expect only the first rtp module to be asked to send a TargetBitrate
  // message. (No target bitrate with 0bps sent from the second one.)
  EXPECT_CALL(rtp_1, SetVideoBitrateAllocation(bitrate)).Times(1);
  EXPECT_CALL(rtp_2, SetVideoBitrateAllocation(_)).Times(0);

  payload_router.OnBitrateAllocationUpdated(bitrate);
}

TEST(PayloadRouterTest, SvcTargetBitrate) {
  NiceMock<MockRtpRtcp> rtp_1;
  std::vector<RtpRtcp*> modules = {&rtp_1};
  PayloadRouter payload_router(modules, {kSsrc1}, kPayloadType, {});
  payload_router.SetActive(true);

  BitrateAllocation bitrate;
  bitrate.SetBitrate(0, 0, 10000);
  bitrate.SetBitrate(0, 1, 20000);
  bitrate.SetBitrate(1, 0, 40000);
  bitrate.SetBitrate(1, 1, 80000);

  EXPECT_CALL(rtp_1, SetVideoBitrateAllocation(bitrate)).Times(1);

  payload_router.OnBitrateAllocationUpdated(bitrate);
}

TEST(PayloadRouterTest, InfoMappedToRtpVideoHeader_Vp8) {
  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  payload_router.SetActive(true);

  EncodedImage encoded_image;
  encoded_image.rotation_ = kVideoRotation_90;
  encoded_image.content_type_ = VideoContentType::SCREENSHARE;

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.simulcastIdx = 1;
  codec_info.codecSpecific.VP8.pictureId = kPictureId;
  codec_info.codecSpecific.VP8.temporalIdx = kTemporalIdx;
  codec_info.codecSpecific.VP8.tl0PicIdx = kTl0PicIdx;
  codec_info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
  codec_info.codecSpecific.VP8.layerSync = true;
  codec_info.codecSpecific.VP8.nonReference = true;

  EXPECT_CALL(rtp2, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kVideoRotation_90, header->rotation);
        EXPECT_EQ(VideoContentType::SCREENSHARE, header->content_type);
        EXPECT_EQ(1, header->simulcastIdx);
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kPictureId, header->codecHeader.VP8.pictureId);
        EXPECT_EQ(kTemporalIdx, header->codecHeader.VP8.temporalIdx);
        EXPECT_EQ(kTl0PicIdx, header->codecHeader.VP8.tl0PicIdx);
        EXPECT_EQ(kNoKeyIdx, header->codecHeader.VP8.keyIdx);
        EXPECT_TRUE(header->codecHeader.VP8.layerSync);
        EXPECT_TRUE(header->codecHeader.VP8.nonReference);
        return true;
      }));

  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, &codec_info, nullptr).error);
}

TEST(PayloadRouterTest, InfoMappedToRtpVideoHeader_H264) {
  NiceMock<MockRtpRtcp> rtp1;
  std::vector<RtpRtcp*> modules = {&rtp1};
  PayloadRouter payload_router(modules, {kSsrc1}, kPayloadType, {});
  payload_router.SetActive(true);

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecH264;
  codec_info.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::SingleNalUnit;

  EXPECT_CALL(rtp1, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(0, header->simulcastIdx);
        EXPECT_EQ(kRtpVideoH264, header->codec);
        EXPECT_EQ(H264PacketizationMode::SingleNalUnit,
                  header->codecHeader.H264.packetization_mode);
        return true;
      }));

  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      payload_router.OnEncodedImage(encoded_image, &codec_info, nullptr).error);
}

TEST(PayloadRouterTest, CreateWithNoPreviousStates) {
  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, {});
  payload_router.SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      payload_router.GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_NE(initial_states.find(kSsrc1), initial_states.end());
  EXPECT_NE(initial_states.find(kSsrc2), initial_states.end());
}

TEST(PayloadRouterTest, CreateWithPreviousStates) {
  RtpPayloadState state1;
  state1.picture_id = kInitialPictureId1;
  RtpPayloadState state2;
  state2.picture_id = kInitialPictureId2;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc1, state1},
                                                {kSsrc2, state2}};

  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter payload_router(modules, {kSsrc1, kSsrc2}, kPayloadType, states);
  payload_router.SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      payload_router.GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_EQ(kInitialPictureId1, initial_states[kSsrc1].picture_id);
  EXPECT_EQ(kInitialPictureId2, initial_states[kSsrc2].picture_id);
}

class PayloadRouterTest : public ::testing::Test {
 public:
  explicit PayloadRouterTest(const std::string& field_trials)
      : override_field_trials_(field_trials) {}
  virtual ~PayloadRouterTest() {}

 protected:
  virtual void SetUp() { memset(&codec_info_, 0, sizeof(CodecSpecificInfo)); }

  test::ScopedFieldTrials override_field_trials_;
  EncodedImage image_;
  CodecSpecificInfo codec_info_;
};

class TestWithForcedFallbackDisabled : public PayloadRouterTest {
 public:
  TestWithForcedFallbackDisabled()
      : PayloadRouterTest("WebRTC-VP8-Forced-Fallback-Encoder-v2/Disabled/") {}
};

class TestWithForcedFallbackEnabled : public PayloadRouterTest {
 public:
  TestWithForcedFallbackEnabled()
      : PayloadRouterTest(
            "WebRTC-VP8-Forced-Fallback-Encoder-v2/Enabled-1,2,3/") {}
};

TEST_F(TestWithForcedFallbackDisabled, PictureIdIsNotChangedForVp8) {
  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules = {&rtp};
  PayloadRouter router(modules, {kSsrc1}, kPayloadType, {});
  router.SetActive(true);

  codec_info_.codecType = kVideoCodecVP8;
  codec_info_.codecSpecific.VP8.pictureId = kPictureId;

  EXPECT_CALL(rtp, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kPictureId, header->codecHeader.VP8.pictureId);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);
}

TEST_F(TestWithForcedFallbackEnabled, PictureIdIsSetForVp8) {
  RtpPayloadState state1;
  state1.picture_id = kInitialPictureId1;
  RtpPayloadState state2;
  state2.picture_id = kInitialPictureId2;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc1, state1},
                                                {kSsrc2, state2}};

  NiceMock<MockRtpRtcp> rtp1;
  NiceMock<MockRtpRtcp> rtp2;
  std::vector<RtpRtcp*> modules = {&rtp1, &rtp2};
  PayloadRouter router(modules, {kSsrc1, kSsrc2}, kPayloadType, states);
  router.SetActive(true);

  // OnEncodedImage, simulcastIdx: 0.
  codec_info_.codecType = kVideoCodecVP8;
  codec_info_.codecSpecific.VP8.pictureId = kPictureId;
  codec_info_.codecSpecific.VP8.simulcastIdx = 0;

  EXPECT_CALL(rtp1, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kInitialPictureId1, header->codecHeader.VP8.pictureId);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);

  // OnEncodedImage, simulcastIdx: 1.
  codec_info_.codecSpecific.VP8.pictureId = kPictureId;
  codec_info_.codecSpecific.VP8.simulcastIdx = 1;

  EXPECT_CALL(rtp2, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kInitialPictureId2, header->codecHeader.VP8.pictureId);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);

  // State should hold next picture id to use.
  states = router.GetRtpPayloadStates();
  EXPECT_EQ(2u, states.size());
  EXPECT_EQ(kInitialPictureId1 + 1, states[kSsrc1].picture_id);
  EXPECT_EQ(kInitialPictureId2 + 1, states[kSsrc2].picture_id);
}

TEST_F(TestWithForcedFallbackEnabled, PictureIdWraps) {
  RtpPayloadState state1;
  state1.picture_id = kMaxTwoBytePictureId;

  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules = {&rtp};
  PayloadRouter router(modules, {kSsrc1}, kPayloadType, {{kSsrc1, state1}});
  router.SetActive(true);

  codec_info_.codecType = kVideoCodecVP8;
  codec_info_.codecSpecific.VP8.pictureId = kPictureId;

  EXPECT_CALL(rtp, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kMaxTwoBytePictureId, header->codecHeader.VP8.pictureId);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);

  // State should hold next picture id to use.
  std::map<uint32_t, RtpPayloadState> states = router.GetRtpPayloadStates();
  EXPECT_EQ(1u, states.size());
  EXPECT_EQ(0, states[kSsrc1].picture_id);  // Wrapped.
}

TEST_F(TestWithForcedFallbackEnabled, PictureIdIsNotSetIfNoPictureId) {
  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules = {&rtp};
  PayloadRouter router(modules, {kSsrc1}, kPayloadType, {});
  router.SetActive(true);

  codec_info_.codecType = kVideoCodecVP8;
  codec_info_.codecSpecific.VP8.pictureId = kNoPictureId;

  EXPECT_CALL(rtp, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp8, header->codec);
        EXPECT_EQ(kNoPictureId, header->codecHeader.VP8.pictureId);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);
}

TEST_F(TestWithForcedFallbackEnabled, PictureIdIsNotSetForVp9) {
  NiceMock<MockRtpRtcp> rtp;
  std::vector<RtpRtcp*> modules = {&rtp};
  PayloadRouter router(modules, {kSsrc1}, kPayloadType, {});
  router.SetActive(true);

  codec_info_.codecType = kVideoCodecVP9;
  codec_info_.codecSpecific.VP9.picture_id = kPictureId;

  EXPECT_CALL(rtp, SendOutgoingData(_, _, _, _, _, _, nullptr, _, _))
      .WillOnce(Invoke([](Unused, Unused, Unused, Unused, Unused, Unused,
                          Unused, const RTPVideoHeader* header, Unused) {
        EXPECT_EQ(kRtpVideoVp9, header->codec);
        EXPECT_EQ(kPictureId, header->codecHeader.VP9.picture_id);
        return true;
      }));

  EXPECT_EQ(EncodedImageCallback::Result::OK,
            router.OnEncodedImage(image_, &codec_info_, nullptr).error);
}

}  // namespace webrtc
