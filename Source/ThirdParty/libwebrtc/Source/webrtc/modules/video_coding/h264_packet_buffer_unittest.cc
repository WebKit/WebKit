/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/h264_packet_buffer.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "api/video/render_resolution.h"
#include "common_video/h264/h264_common.h"
#include "rtc_base/system/unused.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

using H264::NaluType::kAud;
using H264::NaluType::kFuA;
using H264::NaluType::kIdr;
using H264::NaluType::kPps;
using H264::NaluType::kSlice;
using H264::NaluType::kSps;
using H264::NaluType::kStapA;

constexpr int kBufferSize = 2048;

std::vector<uint8_t> StartCode() {
  return {0, 0, 0, 1};
}

NaluInfo MakeNaluInfo(uint8_t type) {
  NaluInfo res;
  res.type = type;
  res.sps_id = -1;
  res.pps_id = -1;
  return res;
}

class Packet {
 public:
  explicit Packet(H264PacketizationTypes type);

  Packet& Idr(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Slice(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Sps(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& SpsWithResolution(RenderResolution resolution,
                            std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Pps(std::vector<uint8_t> payload = {9, 9, 9});
  Packet& Aud();
  Packet& Marker();
  Packet& AsFirstFragment();
  Packet& Time(uint32_t rtp_timestamp);
  Packet& SeqNum(uint16_t rtp_seq_num);

  std::unique_ptr<H264PacketBuffer::Packet> Build();

 private:
  rtc::CopyOnWriteBuffer BuildFuaPayload() const;
  rtc::CopyOnWriteBuffer BuildSingleNaluPayload() const;
  rtc::CopyOnWriteBuffer BuildStapAPayload() const;

  RTPVideoHeaderH264& H264Header() {
    return absl::get<RTPVideoHeaderH264>(video_header_.video_type_header);
  }
  const RTPVideoHeaderH264& H264Header() const {
    return absl::get<RTPVideoHeaderH264>(video_header_.video_type_header);
  }

  H264PacketizationTypes type_;
  RTPVideoHeader video_header_;
  bool first_fragment_ = false;
  bool marker_bit_ = false;
  uint32_t rtp_timestamp_ = 0;
  uint16_t rtp_seq_num_ = 0;
  std::vector<std::vector<uint8_t>> nalu_payloads_;
};

Packet::Packet(H264PacketizationTypes type) : type_(type) {
  video_header_.video_type_header.emplace<RTPVideoHeaderH264>();
}

Packet& Packet::Idr(std::vector<uint8_t> payload) {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kIdr);
  nalu_payloads_.push_back(std::move(payload));
  return *this;
}

Packet& Packet::Slice(std::vector<uint8_t> payload) {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kSlice);
  nalu_payloads_.push_back(std::move(payload));
  return *this;
}

Packet& Packet::Sps(std::vector<uint8_t> payload) {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kSps);
  nalu_payloads_.push_back(std::move(payload));
  return *this;
}

Packet& Packet::SpsWithResolution(RenderResolution resolution,
                                  std::vector<uint8_t> payload) {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kSps);
  video_header_.width = resolution.Width();
  video_header_.height = resolution.Height();
  nalu_payloads_.push_back(std::move(payload));
  return *this;
}

Packet& Packet::Pps(std::vector<uint8_t> payload) {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kPps);
  nalu_payloads_.push_back(std::move(payload));
  return *this;
}

Packet& Packet::Aud() {
  auto& h264_header = H264Header();
  h264_header.nalus[h264_header.nalus_length++] = MakeNaluInfo(kAud);
  nalu_payloads_.push_back({});
  return *this;
}

Packet& Packet::Marker() {
  marker_bit_ = true;
  return *this;
}

Packet& Packet::AsFirstFragment() {
  first_fragment_ = true;
  return *this;
}

Packet& Packet::Time(uint32_t rtp_timestamp) {
  rtp_timestamp_ = rtp_timestamp;
  return *this;
}

Packet& Packet::SeqNum(uint16_t rtp_seq_num) {
  rtp_seq_num_ = rtp_seq_num;
  return *this;
}

std::unique_ptr<H264PacketBuffer::Packet> Packet::Build() {
  auto res = std::make_unique<H264PacketBuffer::Packet>();

  auto& h264_header = H264Header();
  switch (type_) {
    case kH264FuA: {
      RTC_CHECK_EQ(h264_header.nalus_length, 1);
      res->video_payload = BuildFuaPayload();
      break;
    }
    case kH264SingleNalu: {
      RTC_CHECK_EQ(h264_header.nalus_length, 1);
      res->video_payload = BuildSingleNaluPayload();
      break;
    }
    case kH264StapA: {
      RTC_CHECK_GT(h264_header.nalus_length, 1);
      RTC_CHECK_LE(h264_header.nalus_length, kMaxNalusPerPacket);
      res->video_payload = BuildStapAPayload();
      break;
    }
  }

  if (type_ == kH264FuA && !first_fragment_) {
    h264_header.nalus_length = 0;
  }

  h264_header.packetization_type = type_;
  res->marker_bit = marker_bit_;
  res->video_header = video_header_;
  res->timestamp = rtp_timestamp_;
  res->seq_num = rtp_seq_num_;
  res->video_header.codec = kVideoCodecH264;

  return res;
}

rtc::CopyOnWriteBuffer Packet::BuildFuaPayload() const {
  return rtc::CopyOnWriteBuffer(nalu_payloads_[0]);
}

rtc::CopyOnWriteBuffer Packet::BuildSingleNaluPayload() const {
  rtc::CopyOnWriteBuffer res;
  auto& h264_header = H264Header();
  res.AppendData(&h264_header.nalus[0].type, 1);
  res.AppendData(nalu_payloads_[0]);
  return res;
}

rtc::CopyOnWriteBuffer Packet::BuildStapAPayload() const {
  rtc::CopyOnWriteBuffer res;

  const uint8_t indicator = H264::NaluType::kStapA;
  res.AppendData(&indicator, 1);

  auto& h264_header = H264Header();
  for (size_t i = 0; i < h264_header.nalus_length; ++i) {
    // The two first bytes indicates the nalu segment size.
    uint8_t length_as_array[2] = {
        0, static_cast<uint8_t>(nalu_payloads_[i].size() + 1)};
    res.AppendData(length_as_array);

    res.AppendData(&h264_header.nalus[i].type, 1);
    res.AppendData(nalu_payloads_[i]);
  }
  return res;
}

rtc::ArrayView<const uint8_t> PacketPayload(
    const std::unique_ptr<H264PacketBuffer::Packet>& packet) {
  return packet->video_payload;
}

std::vector<uint8_t> FlatVector(
    const std::vector<std::vector<uint8_t>>& elems) {
  std::vector<uint8_t> res;
  for (const auto& elem : elems) {
    res.insert(res.end(), elem.begin(), elem.end());
  }
  return res;
}

TEST(H264PacketBufferTest, IdrIsKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/true);

  EXPECT_THAT(
      packet_buffer.InsertPacket(Packet(kH264SingleNalu).Idr().Marker().Build())
          .packets,
      SizeIs(1));
}

TEST(H264PacketBufferTest, IdrIsNotKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer.InsertPacket(Packet(kH264SingleNalu).Idr().Marker().Build())
          .packets,
      IsEmpty());
}

TEST(H264PacketBufferTest, IdrIsKeyframeFuaRequiresFirstFragmet) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/true);

  // Not marked as the first fragment
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Idr().SeqNum(0).Time(0).Build())
          .packets,
      IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264FuA).Idr().SeqNum(1).Time(0).Marker().Build())
                  .packets,
              IsEmpty());

  // Marked as first fragment
  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264FuA)
                                    .Idr()
                                    .SeqNum(2)
                                    .Time(1)
                                    .AsFirstFragment()
                                    .Build())
                  .packets,
              IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264FuA).Idr().SeqNum(3).Time(1).Marker().Build())
                  .packets,
              SizeIs(2));
}

TEST(H264PacketBufferTest, SpsPpsIdrIsKeyframeSingleNalus) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps().SeqNum(0).Time(0).Build()));
  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps().SeqNum(1).Time(0).Build()));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264SingleNalu).Idr().SeqNum(2).Time(0).Marker().Build())
          .packets,
      SizeIs(3));
}

TEST(H264PacketBufferTest, PpsIdrIsNotKeyframeSingleNalus) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps().SeqNum(0).Time(0).Build()));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264SingleNalu).Idr().SeqNum(1).Time(0).Marker().Build())
          .packets,
      IsEmpty());
}

TEST(H264PacketBufferTest, SpsIdrIsNotKeyframeSingleNalus) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps().SeqNum(0).Time(0).Build()));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264SingleNalu).Idr().SeqNum(1).Time(0).Marker().Build())
          .packets,
      IsEmpty());
}

TEST(H264PacketBufferTest, SpsPpsIdrIsKeyframeStapA) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(0)
                                    .Time(0)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, PpsIdrIsNotKeyframeStapA) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264StapA).Pps().Idr().SeqNum(0).Time(0).Marker().Build())
          .packets,
      IsEmpty());
}

TEST(H264PacketBufferTest, SpsIdrIsNotKeyframeStapA) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264StapA).Sps().Idr().SeqNum(2).Time(2).Marker().Build())
          .packets,
      IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(3)
                                    .Time(3)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, InsertingSpsPpsLastCompletesKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Idr().SeqNum(2).Time(1).Marker().Build()));

  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264StapA).Sps().Pps().SeqNum(1).Time(1).Build())
                  .packets,
              SizeIs(2));
}

TEST(H264PacketBufferTest, InsertingMidFuaCompletesFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(0)
                                    .Time(0)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(1).Time(1).AsFirstFragment().Build()));
  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(3).Time(1).Marker().Build()));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Slice().SeqNum(2).Time(1).Build())
          .packets,
      SizeIs(3));
}

TEST(H264PacketBufferTest, SeqNumJumpDoesNotCompleteFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(0)
                                    .Time(0)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Slice().SeqNum(1).Time(1).Build())
          .packets,
      IsEmpty());

  // Add `kBufferSize` to make the index of the sequence number wrap and end up
  // where the packet with sequence number 2 would have ended up.
  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264FuA)
                                    .Slice()
                                    .SeqNum(2 + kBufferSize)
                                    .Time(3)
                                    .Marker()
                                    .Build())
                  .packets,
              IsEmpty());
}

TEST(H264PacketBufferTest, OldFramesAreNotCompletedAfterBufferWrap) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264SingleNalu)
                                    .Slice()
                                    .SeqNum(1)
                                    .Time(1)
                                    .Marker()
                                    .Build())
                  .packets,
              IsEmpty());

  // New keyframe, preceedes packet with sequence number 1 in the buffer.
  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(kBufferSize)
                                    .Time(kBufferSize)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, OldPacketsDontBlockNewPackets) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);
  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(kBufferSize)
                                    .Time(kBufferSize)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));

  RTC_UNUSED(packet_buffer.InsertPacket(Packet(kH264FuA)
                                            .Slice()
                                            .SeqNum(kBufferSize + 1)
                                            .Time(kBufferSize + 1)
                                            .AsFirstFragment()
                                            .Build()));

  RTC_UNUSED(packet_buffer.InsertPacket(Packet(kH264FuA)
                                            .Slice()
                                            .SeqNum(kBufferSize + 3)
                                            .Time(kBufferSize + 1)
                                            .Marker()
                                            .Build()));
  EXPECT_THAT(
      packet_buffer
          .InsertPacket(Packet(kH264FuA).Slice().SeqNum(2).Time(2).Build())
          .packets,
      IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264FuA)
                                    .Slice()
                                    .SeqNum(kBufferSize + 2)
                                    .Time(kBufferSize + 1)
                                    .Build())
                  .packets,
              SizeIs(3));
}

TEST(H264PacketBufferTest, OldPacketDoesntCompleteFrame) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(kBufferSize)
                                    .Time(kBufferSize)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264FuA)
                                    .Slice()
                                    .SeqNum(kBufferSize + 3)
                                    .Time(kBufferSize + 1)
                                    .Marker()
                                    .Build())
                  .packets,
              IsEmpty());

  EXPECT_THAT(
      packet_buffer
          .InsertPacket(
              Packet(kH264FuA).Slice().SeqNum(2).Time(2).Marker().Build())
          .packets,
      IsEmpty());

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264FuA)
                                    .Slice()
                                    .SeqNum(kBufferSize + 1)
                                    .Time(kBufferSize + 1)
                                    .AsFirstFragment()
                                    .Build())
                  .packets,
              IsEmpty());
}

TEST(H264PacketBufferTest, FrameBoundariesAreSet) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  auto key = packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(1).Time(1).Marker().Build());

  ASSERT_THAT(key.packets, SizeIs(1));
  EXPECT_TRUE(key.packets[0]->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(key.packets[0]->video_header.is_last_packet_in_frame);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(2).Time(2).Build()));
  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(3).Time(2).Build()));
  auto delta = packet_buffer.InsertPacket(
      Packet(kH264FuA).Slice().SeqNum(4).Time(2).Marker().Build());

  ASSERT_THAT(delta.packets, SizeIs(3));
  EXPECT_TRUE(delta.packets[0]->video_header.is_first_packet_in_frame);
  EXPECT_FALSE(delta.packets[0]->video_header.is_last_packet_in_frame);

  EXPECT_FALSE(delta.packets[1]->video_header.is_first_packet_in_frame);
  EXPECT_FALSE(delta.packets[1]->video_header.is_last_packet_in_frame);

  EXPECT_FALSE(delta.packets[2]->video_header.is_first_packet_in_frame);
  EXPECT_TRUE(delta.packets[2]->video_header.is_last_packet_in_frame);
}

TEST(H264PacketBufferTest, ResolutionSetOnFirstPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Aud().SeqNum(1).Time(1).Build()));
  auto res = packet_buffer.InsertPacket(Packet(kH264StapA)
                                            .SpsWithResolution({320, 240})
                                            .Pps()
                                            .Idr()
                                            .SeqNum(2)
                                            .Time(1)
                                            .Marker()
                                            .Build());

  ASSERT_THAT(res.packets, SizeIs(2));
  EXPECT_THAT(res.packets[0]->video_header.width, Eq(320));
  EXPECT_THAT(res.packets[0]->video_header.height, Eq(240));
}

TEST(H264PacketBufferTest, KeyframeAndDeltaFrameSetOnFirstPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Aud().SeqNum(1).Time(1).Build()));
  auto key = packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(2).Time(1).Marker().Build());

  auto delta = packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Slice().SeqNum(3).Time(2).Marker().Build());

  ASSERT_THAT(key.packets, SizeIs(2));
  EXPECT_THAT(key.packets[0]->video_header.frame_type,
              Eq(VideoFrameType::kVideoFrameKey));
  ASSERT_THAT(delta.packets, SizeIs(1));
  EXPECT_THAT(delta.packets[0]->video_header.frame_type,
              Eq(VideoFrameType::kVideoFrameDelta));
}

TEST(H264PacketBufferTest, RtpSeqNumWrap) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264StapA).Sps().Pps().SeqNum(0xffff).Time(0).Build()));

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264FuA).Idr().SeqNum(0).Time(0).Build()));
  EXPECT_THAT(packet_buffer
                  .InsertPacket(
                      Packet(kH264FuA).Idr().SeqNum(1).Time(0).Marker().Build())
                  .packets,
              SizeIs(3));
}

TEST(H264PacketBufferTest, StapAFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  auto packets = packet_buffer
                     .InsertPacket(Packet(kH264StapA)
                                       .Sps({1, 2, 3})
                                       .Pps({4, 5, 6})
                                       .Idr({7, 8, 9})
                                       .SeqNum(0)
                                       .Time(0)
                                       .Marker()
                                       .Build())
                     .packets;

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_THAT(PacketPayload(packets[0]),
              ElementsAreArray(FlatVector({StartCode(),
                                           {kSps, 1, 2, 3},
                                           StartCode(),
                                           {kPps, 4, 5, 6},
                                           StartCode(),
                                           {kIdr, 7, 8, 9}})));
}

TEST(H264PacketBufferTest, SingleNaluFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Sps({1, 2, 3}).SeqNum(0).Time(0).Build()));
  RTC_UNUSED(packet_buffer.InsertPacket(
      Packet(kH264SingleNalu).Pps({4, 5, 6}).SeqNum(1).Time(0).Build()));
  auto packets = packet_buffer
                     .InsertPacket(Packet(kH264SingleNalu)
                                       .Idr({7, 8, 9})
                                       .SeqNum(2)
                                       .Time(0)
                                       .Marker()
                                       .Build())
                     .packets;

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(PacketPayload(packets[0]),
              ElementsAreArray(FlatVector({StartCode(), {kSps, 1, 2, 3}})));
  EXPECT_THAT(PacketPayload(packets[1]),
              ElementsAreArray(FlatVector({StartCode(), {kPps, 4, 5, 6}})));
  EXPECT_THAT(PacketPayload(packets[2]),
              ElementsAreArray(FlatVector({StartCode(), {kIdr, 7, 8, 9}})));
}

TEST(H264PacketBufferTest, StapaAndFuaFixedBitstream) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  RTC_UNUSED(packet_buffer.InsertPacket(Packet(kH264StapA)
                                            .Sps({1, 2, 3})
                                            .Pps({4, 5, 6})
                                            .SeqNum(0)
                                            .Time(0)
                                            .Build()));
  RTC_UNUSED(packet_buffer.InsertPacket(Packet(kH264FuA)
                                            .Idr({8, 8, 8})
                                            .SeqNum(1)
                                            .Time(0)
                                            .AsFirstFragment()
                                            .Build()));
  auto packets = packet_buffer
                     .InsertPacket(Packet(kH264FuA)
                                       .Idr({9, 9, 9})
                                       .SeqNum(2)
                                       .Time(0)
                                       .Marker()
                                       .Build())
                     .packets;

  ASSERT_THAT(packets, SizeIs(3));
  EXPECT_THAT(
      PacketPayload(packets[0]),
      ElementsAreArray(FlatVector(
          {StartCode(), {kSps, 1, 2, 3}, StartCode(), {kPps, 4, 5, 6}})));
  EXPECT_THAT(PacketPayload(packets[1]),
              ElementsAreArray(FlatVector({StartCode(), {8, 8, 8}})));
  // Third is a continuation of second, so only the payload is expected.
  EXPECT_THAT(PacketPayload(packets[2]),
              ElementsAreArray(FlatVector({{9, 9, 9}})));
}

TEST(H264PacketBufferTest, FullPacketBufferDoesNotBlockKeyframe) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  for (int i = 0; i < kBufferSize; ++i) {
    EXPECT_THAT(
        packet_buffer
            .InsertPacket(
                Packet(kH264SingleNalu).Slice().SeqNum(i).Time(0).Build())
            .packets,
        IsEmpty());
  }

  EXPECT_THAT(packet_buffer
                  .InsertPacket(Packet(kH264StapA)
                                    .Sps()
                                    .Pps()
                                    .Idr()
                                    .SeqNum(kBufferSize)
                                    .Time(1)
                                    .Marker()
                                    .Build())
                  .packets,
              SizeIs(1));
}

TEST(H264PacketBufferTest, TooManyNalusInPacket) {
  H264PacketBuffer packet_buffer(/*allow_idr_only_keyframes=*/false);

  std::unique_ptr<H264PacketBuffer::Packet> packet(
      Packet(kH264StapA).Sps().Pps().Idr().SeqNum(1).Time(1).Marker().Build());
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet->video_header.video_type_header);
  h264_header.nalus_length = kMaxNalusPerPacket + 1;

  EXPECT_THAT(packet_buffer.InsertPacket(std::move(packet)).packets, IsEmpty());
}

}  // namespace
}  // namespace webrtc
