/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/packet_buffer.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Pointee;
using ::testing::SizeIs;

constexpr int kStartSize = 16;
constexpr int kMaxSize = 64;

void IgnoreResult(PacketBuffer::InsertResult /*result*/) {}

// Validates frame boundaries are valid and returns first sequence_number for
// each frame.
std::vector<uint16_t> StartSeqNums(
    rtc::ArrayView<const std::unique_ptr<PacketBuffer::Packet>> packets) {
  std::vector<uint16_t> result;
  bool frame_boundary = true;
  for (const auto& packet : packets) {
    EXPECT_EQ(frame_boundary, packet->is_first_packet_in_frame());
    if (packet->is_first_packet_in_frame()) {
      result.push_back(packet->seq_num);
    }
    frame_boundary = packet->is_last_packet_in_frame();
  }
  EXPECT_TRUE(frame_boundary);
  return result;
}

MATCHER_P(StartSeqNumsAre, seq_num, "") {
  return Matches(ElementsAre(seq_num))(StartSeqNums(arg.packets));
}

MATCHER_P2(StartSeqNumsAre, seq_num1, seq_num2, "") {
  return Matches(ElementsAre(seq_num1, seq_num2))(StartSeqNums(arg.packets));
}

MATCHER(KeyFrame, "") {
  return arg->is_first_packet_in_frame() &&
         arg->video_header.frame_type == VideoFrameType::kVideoFrameKey;
}

MATCHER(DeltaFrame, "") {
  return arg->is_first_packet_in_frame() &&
         arg->video_header.frame_type == VideoFrameType::kVideoFrameDelta;
}

struct PacketBufferInsertResult : public PacketBuffer::InsertResult {
  explicit PacketBufferInsertResult(PacketBuffer::InsertResult result)
      : InsertResult(std::move(result)) {}
};

void PrintTo(const PacketBufferInsertResult& result, std::ostream* os) {
  *os << "frames: { ";
  for (const auto& packet : result.packets) {
    if (packet->is_first_packet_in_frame() &&
        packet->is_last_packet_in_frame()) {
      *os << "{sn: " << packet->seq_num << " }";
    } else if (packet->is_first_packet_in_frame()) {
      *os << "{sn: [" << packet->seq_num << "-";
    } else if (packet->is_last_packet_in_frame()) {
      *os << packet->seq_num << "] }, ";
    }
  }
  *os << " }";
  if (result.buffer_cleared) {
    *os << ", buffer_cleared";
  }
}

class PacketBufferTest : public ::testing::Test {
 protected:
  PacketBufferTest()
      : rand_(0x7732213),
        clock_(0),
        packet_buffer_(&clock_, kStartSize, kMaxSize) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  enum IsKeyFrame { kKeyFrame, kDeltaFrame };
  enum IsFirst { kFirst, kNotFirst };
  enum IsLast { kLast, kNotLast };

  PacketBufferInsertResult Insert(uint16_t seq_num,  // packet sequence number
                                  IsKeyFrame keyframe,  // is keyframe
                                  IsFirst first,  // is first packet of frame
                                  IsLast last,    // is last packet of frame
                                  rtc::ArrayView<const uint8_t> data = {},
                                  uint32_t timestamp = 123u) {  // rtp timestamp
    auto packet = std::make_unique<PacketBuffer::Packet>();
    packet->video_header.codec = kVideoCodecGeneric;
    packet->timestamp = timestamp;
    packet->seq_num = seq_num;
    packet->video_header.frame_type = keyframe == kKeyFrame
                                          ? VideoFrameType::kVideoFrameKey
                                          : VideoFrameType::kVideoFrameDelta;
    packet->video_header.is_first_packet_in_frame = first == kFirst;
    packet->video_header.is_last_packet_in_frame = last == kLast;
    packet->video_payload.SetData(data.data(), data.size());

    return PacketBufferInsertResult(
        packet_buffer_.InsertPacket(std::move(packet)));
  }

  Random rand_;
  SimulatedClock clock_;
  PacketBuffer packet_buffer_;
};

TEST_F(PacketBufferTest, InsertOnePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).packets, SizeIs(1));
}

TEST_F(PacketBufferTest, InsertMultiplePackets) {
  const uint16_t seq_num = Rand();
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast).packets, SizeIs(1));
  EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast).packets, SizeIs(1));
  EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kFirst, kLast).packets, SizeIs(1));
  EXPECT_THAT(Insert(seq_num + 3, kKeyFrame, kFirst, kLast).packets, SizeIs(1));
}

TEST_F(PacketBufferTest, InsertDuplicatePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast).packets,
              SizeIs(2));
}

TEST_F(PacketBufferTest, SeqNumWrapOneFrame) {
  Insert(0xFFFF, kKeyFrame, kFirst, kNotLast);
  EXPECT_THAT(Insert(0x0, kKeyFrame, kNotFirst, kLast),
              StartSeqNumsAre(0xFFFF));
}

TEST_F(PacketBufferTest, SeqNumWrapTwoFrames) {
  EXPECT_THAT(Insert(0xFFFF, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(0xFFFF));
  EXPECT_THAT(Insert(0x0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0x0));
}

TEST_F(PacketBufferTest, InsertOldPackets) {
  EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).packets, SizeIs(1));
  EXPECT_THAT(Insert(101, kKeyFrame, kNotFirst, kLast).packets, SizeIs(2));

  EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(100, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).packets, SizeIs(1));

  packet_buffer_.ClearTo(102);
  EXPECT_THAT(Insert(102, kDeltaFrame, kFirst, kLast).packets, IsEmpty());
  EXPECT_THAT(Insert(103, kDeltaFrame, kFirst, kLast).packets, SizeIs(1));
}

TEST_F(PacketBufferTest, FrameSize) {
  const uint16_t seq_num = Rand();
  uint8_t data1[5] = {};
  uint8_t data2[5] = {};
  uint8_t data3[5] = {};
  uint8_t data4[5] = {};

  Insert(seq_num, kKeyFrame, kFirst, kNotLast, data1);
  Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, data2);
  Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, data3);
  auto packets =
      Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, data4).packets;
  // Expect one frame of 4 packets.
  EXPECT_THAT(StartSeqNums(packets), ElementsAre(seq_num));
  EXPECT_THAT(packets, SizeIs(4));
}

TEST_F(PacketBufferTest, ExpandBuffer) {
  const uint16_t seq_num = Rand();

  Insert(seq_num, kKeyFrame, kFirst, kNotLast);
  for (int i = 1; i < kStartSize; ++i)
    EXPECT_FALSE(
        Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).buffer_cleared);

  // Already inserted kStartSize number of packets, inserting the last packet
  // should increase the buffer size and also result in an assembled frame.
  EXPECT_FALSE(
      Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast).buffer_cleared);
}

TEST_F(PacketBufferTest, SingleFrameExpandsBuffer) {
  const uint16_t seq_num = Rand();

  Insert(seq_num, kKeyFrame, kFirst, kNotLast);
  for (int i = 1; i < kStartSize; ++i)
    Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num));
}

TEST_F(PacketBufferTest, ExpandBufferOverflow) {
  const uint16_t seq_num = Rand();

  EXPECT_FALSE(Insert(seq_num, kKeyFrame, kFirst, kNotLast).buffer_cleared);
  for (int i = 1; i < kMaxSize; ++i)
    EXPECT_FALSE(
        Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast).buffer_cleared);

  // Already inserted kMaxSize number of packets, inserting the last packet
  // should overflow the buffer and result in false being returned.
  EXPECT_TRUE(
      Insert(seq_num + kMaxSize, kKeyFrame, kNotFirst, kLast).buffer_cleared);
}

TEST_F(PacketBufferTest, OnePacketOneFrame) {
  const uint16_t seq_num = Rand();
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num));
}

TEST_F(PacketBufferTest, TwoPacketsTwoFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num));
  EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 1));
}

TEST_F(PacketBufferTest, TwoPacketsOneFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num));
}

TEST_F(PacketBufferTest, ThreePacketReorderingOneFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kNotLast).packets, IsEmpty());
  EXPECT_THAT(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast).packets,
              IsEmpty());
  EXPECT_THAT(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast),
              StartSeqNumsAre(seq_num));
}

TEST_F(PacketBufferTest, Frames) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num));
  EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 1));
  EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 2));
  EXPECT_THAT(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 3));
}

TEST_F(PacketBufferTest, ClearSinglePacket) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i)
    Insert(seq_num + i, kDeltaFrame, kFirst, kLast);

  packet_buffer_.ClearTo(seq_num);
  EXPECT_FALSE(
      Insert(seq_num + kMaxSize, kDeltaFrame, kFirst, kLast).buffer_cleared);
}

TEST_F(PacketBufferTest, ClearFullBuffer) {
  for (int i = 0; i < kMaxSize; ++i)
    Insert(i, kDeltaFrame, kFirst, kLast);

  packet_buffer_.ClearTo(kMaxSize - 1);

  for (int i = kMaxSize; i < 2 * kMaxSize; ++i)
    EXPECT_FALSE(Insert(i, kDeltaFrame, kFirst, kLast).buffer_cleared);
}

TEST_F(PacketBufferTest, DontClearNewerPacket) {
  EXPECT_THAT(Insert(0, kKeyFrame, kFirst, kLast), StartSeqNumsAre(0));
  packet_buffer_.ClearTo(0);
  EXPECT_THAT(Insert(2 * kStartSize, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(2 * kStartSize));
  EXPECT_THAT(Insert(3 * kStartSize + 1, kKeyFrame, kFirst, kNotLast).packets,
              IsEmpty());
  packet_buffer_.ClearTo(2 * kStartSize);
  EXPECT_THAT(Insert(3 * kStartSize + 2, kKeyFrame, kNotFirst, kLast),
              StartSeqNumsAre(3 * kStartSize + 1));
}

TEST_F(PacketBufferTest, OneIncompleteFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).packets,
              IsEmpty());
  EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num));
  EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).packets,
              IsEmpty());
}

TEST_F(PacketBufferTest, TwoIncompleteFramesFullBuffer) {
  const uint16_t seq_num = Rand();

  for (int i = 1; i < kMaxSize - 1; ++i)
    Insert(seq_num + i, kDeltaFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num, kDeltaFrame, kFirst, kNotLast).packets,
              IsEmpty());
  EXPECT_THAT(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast).packets,
              IsEmpty());
}

TEST_F(PacketBufferTest, FramesReordered) {
  const uint16_t seq_num = Rand();

  EXPECT_THAT(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 1));
  EXPECT_THAT(Insert(seq_num, kKeyFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num));
  EXPECT_THAT(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 3));
  EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast),
              StartSeqNumsAre(seq_num + 2));
}

TEST_F(PacketBufferTest, InsertPacketAfterSequenceNumberWrapAround) {
  uint16_t kFirstSeqNum = 0;
  uint32_t kTimestampDelta = 100;
  uint32_t timestamp = 10000;
  uint16_t seq_num = kFirstSeqNum;

  // Loop until seq_num wraps around.
  SeqNumUnwrapper<uint16_t> unwrapper;
  while (unwrapper.Unwrap(seq_num) < std::numeric_limits<uint16_t>::max()) {
    Insert(seq_num++, kKeyFrame, kFirst, kNotLast, {}, timestamp);
    for (int i = 0; i < 5; ++i) {
      Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, {}, timestamp);
    }
    Insert(seq_num++, kKeyFrame, kNotFirst, kLast, {}, timestamp);
    timestamp += kTimestampDelta;
  }

  // Receive frame with overlapping sequence numbers.
  Insert(seq_num++, kKeyFrame, kFirst, kNotLast, {}, timestamp);
  for (int i = 0; i < 5; ++i) {
    Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, {}, timestamp);
  }
  auto packets =
      Insert(seq_num++, kKeyFrame, kNotFirst, kLast, {}, timestamp).packets;
  // One frame of 7 packets.
  EXPECT_THAT(StartSeqNums(packets), SizeIs(1));
  EXPECT_THAT(packets, SizeIs(7));
}

// If |sps_pps_idr_is_keyframe| is true, we require keyframes to contain
// SPS/PPS/IDR and the keyframes we create as part of the test do contain
// SPS/PPS/IDR. If |sps_pps_idr_is_keyframe| is false, we only require and
// create keyframes containing only IDR.
class PacketBufferH264Test : public PacketBufferTest {
 protected:
  explicit PacketBufferH264Test(bool sps_pps_idr_is_keyframe)
      : PacketBufferTest(), sps_pps_idr_is_keyframe_(sps_pps_idr_is_keyframe) {
    if (sps_pps_idr_is_keyframe) {
      packet_buffer_.ForceSpsPpsIdrIsH264Keyframe();
    }
  }

  PacketBufferInsertResult InsertH264(
      uint16_t seq_num,     // packet sequence number
      IsKeyFrame keyframe,  // is keyframe
      IsFirst first,        // is first packet of frame
      IsLast last,          // is last packet of frame
      uint32_t timestamp,   // rtp timestamp
      rtc::ArrayView<const uint8_t> data = {},
      uint32_t width = 0,     // width of frame (SPS/IDR)
      uint32_t height = 0) {  // height of frame (SPS/IDR)
    auto packet = std::make_unique<PacketBuffer::Packet>();
    packet->video_header.codec = kVideoCodecH264;
    auto& h264_header =
        packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
    packet->seq_num = seq_num;
    packet->timestamp = timestamp;
    if (keyframe == kKeyFrame) {
      if (sps_pps_idr_is_keyframe_) {
        h264_header.nalus[0].type = H264::NaluType::kSps;
        h264_header.nalus[1].type = H264::NaluType::kPps;
        h264_header.nalus[2].type = H264::NaluType::kIdr;
        h264_header.nalus_length = 3;
      } else {
        h264_header.nalus[0].type = H264::NaluType::kIdr;
        h264_header.nalus_length = 1;
      }
    }
    packet->video_header.width = width;
    packet->video_header.height = height;
    packet->video_header.is_first_packet_in_frame = first == kFirst;
    packet->video_header.is_last_packet_in_frame = last == kLast;
    packet->video_payload.SetData(data.data(), data.size());

    return PacketBufferInsertResult(
        packet_buffer_.InsertPacket(std::move(packet)));
  }

  PacketBufferInsertResult InsertH264KeyFrameWithAud(
      uint16_t seq_num,     // packet sequence number
      IsKeyFrame keyframe,  // is keyframe
      IsFirst first,        // is first packet of frame
      IsLast last,          // is last packet of frame
      uint32_t timestamp,   // rtp timestamp
      rtc::ArrayView<const uint8_t> data = {},
      uint32_t width = 0,     // width of frame (SPS/IDR)
      uint32_t height = 0) {  // height of frame (SPS/IDR)
    auto packet = std::make_unique<PacketBuffer::Packet>();
    packet->video_header.codec = kVideoCodecH264;
    auto& h264_header =
        packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
    packet->seq_num = seq_num;
    packet->timestamp = timestamp;

    // this should be the start of frame.
    RTC_CHECK(first == kFirst);

    // Insert a AUD NALU / packet without width/height.
    h264_header.nalus[0].type = H264::NaluType::kAud;
    h264_header.nalus_length = 1;
    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = false;
    IgnoreResult(packet_buffer_.InsertPacket(std::move(packet)));
    // insert IDR
    return InsertH264(seq_num + 1, keyframe, kNotFirst, last, timestamp, data,
                      width, height);
  }

  const bool sps_pps_idr_is_keyframe_;
};

// This fixture is used to test the general behaviour of the packet buffer
// in both configurations.
class PacketBufferH264ParameterizedTest
    : public ::testing::WithParamInterface<bool>,
      public PacketBufferH264Test {
 protected:
  PacketBufferH264ParameterizedTest() : PacketBufferH264Test(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(SpsPpsIdrIsKeyframe,
                         PacketBufferH264ParameterizedTest,
                         ::testing::Bool());

TEST_P(PacketBufferH264ParameterizedTest, DontRemoveMissingPacketOnClearTo) {
  InsertH264(0, kKeyFrame, kFirst, kLast, 0);
  InsertH264(2, kDeltaFrame, kFirst, kNotLast, 2);
  packet_buffer_.ClearTo(0);
  // Expect no frame because of missing of packet #1
  EXPECT_THAT(InsertH264(3, kDeltaFrame, kNotFirst, kLast, 2).packets,
              IsEmpty());
}

TEST_P(PacketBufferH264ParameterizedTest, GetBitstreamOneFrameFullBuffer) {
  uint8_t data_arr[kStartSize][1];
  uint8_t expected[kStartSize];

  for (uint8_t i = 0; i < kStartSize; ++i) {
    data_arr[i][0] = i;
    expected[i] = i;
  }

  InsertH264(0, kKeyFrame, kFirst, kNotLast, 1, data_arr[0]);
  for (uint8_t i = 1; i < kStartSize - 1; ++i) {
    InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1, data_arr[i]);
  }

  auto packets = InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1,
                            data_arr[kStartSize - 1])
                     .packets;
  ASSERT_THAT(StartSeqNums(packets), ElementsAre(0));
  EXPECT_THAT(packets, SizeIs(kStartSize));
  for (size_t i = 0; i < packets.size(); ++i) {
    EXPECT_THAT(packets[i]->video_payload, SizeIs(1)) << "Packet #" << i;
  }
}

TEST_P(PacketBufferH264ParameterizedTest, GetBitstreamBufferPadding) {
  uint16_t seq_num = Rand();
  rtc::CopyOnWriteBuffer data = "some plain old data";

  auto packet = std::make_unique<PacketBuffer::Packet>();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = 1;
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.packetization_type = kH264SingleNalu;
  packet->seq_num = seq_num;
  packet->video_header.codec = kVideoCodecH264;
  packet->video_payload = data;
  packet->video_header.is_first_packet_in_frame = true;
  packet->video_header.is_last_packet_in_frame = true;
  auto frames = packet_buffer_.InsertPacket(std::move(packet)).packets;

  ASSERT_THAT(frames, SizeIs(1));
  EXPECT_EQ(frames[0]->seq_num, seq_num);
  EXPECT_EQ(frames[0]->video_payload, data);
}

TEST_P(PacketBufferH264ParameterizedTest, FrameResolution) {
  uint16_t seq_num = 100;
  uint8_t data[] = "some plain old data";
  uint32_t width = 640;
  uint32_t height = 360;
  uint32_t timestamp = 1000;

  auto packets = InsertH264(seq_num, kKeyFrame, kFirst, kLast, timestamp, data,
                            width, height)
                     .packets;

  ASSERT_THAT(packets, SizeIs(1));
  EXPECT_EQ(packets[0]->video_header.width, width);
  EXPECT_EQ(packets[0]->video_header.height, height);
}

TEST_P(PacketBufferH264ParameterizedTest, FrameResolutionNaluBeforeSPS) {
  uint16_t seq_num = 100;
  uint8_t data[] = "some plain old data";
  uint32_t width = 640;
  uint32_t height = 360;
  uint32_t timestamp = 1000;

  auto packets = InsertH264KeyFrameWithAud(seq_num, kKeyFrame, kFirst, kLast,
                                           timestamp, data, width, height)
                     .packets;

  ASSERT_THAT(StartSeqNums(packets), ElementsAre(seq_num));
  EXPECT_EQ(packets[0]->video_header.width, width);
  EXPECT_EQ(packets[0]->video_header.height, height);
}

TEST_F(PacketBufferTest, FreeSlotsOnFrameCreation) {
  const uint16_t seq_num = Rand();

  Insert(seq_num, kKeyFrame, kFirst, kNotLast);
  Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num));

  // Insert frame that fills the whole buffer.
  Insert(seq_num + 3, kKeyFrame, kFirst, kNotLast);
  for (int i = 0; i < kMaxSize - 2; ++i)
    Insert(seq_num + i + 4, kDeltaFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num + kMaxSize + 2, kKeyFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num + 3));
}

TEST_F(PacketBufferTest, Clear) {
  const uint16_t seq_num = Rand();

  Insert(seq_num, kKeyFrame, kFirst, kNotLast);
  Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num));

  packet_buffer_.Clear();

  Insert(seq_num + kStartSize, kKeyFrame, kFirst, kNotLast);
  Insert(seq_num + kStartSize + 1, kDeltaFrame, kNotFirst, kNotLast);
  EXPECT_THAT(Insert(seq_num + kStartSize + 2, kDeltaFrame, kNotFirst, kLast),
              StartSeqNumsAre(seq_num + kStartSize));
}

TEST_F(PacketBufferTest, FramesAfterClear) {
  Insert(9025, kDeltaFrame, kFirst, kLast);
  Insert(9024, kKeyFrame, kFirst, kLast);
  packet_buffer_.ClearTo(9025);
  EXPECT_THAT(Insert(9057, kDeltaFrame, kFirst, kLast).packets, SizeIs(1));
  EXPECT_THAT(Insert(9026, kDeltaFrame, kFirst, kLast).packets, SizeIs(1));
}

TEST_F(PacketBufferTest, SameFrameDifferentTimestamps) {
  Insert(0, kKeyFrame, kFirst, kNotLast, {}, 1000);
  EXPECT_THAT(Insert(1, kKeyFrame, kNotFirst, kLast, {}, 1001).packets,
              IsEmpty());
}

TEST_F(PacketBufferTest, ContinuousSeqNumDoubleMarkerBit) {
  Insert(2, kKeyFrame, kNotFirst, kNotLast);
  Insert(1, kKeyFrame, kFirst, kLast);
  EXPECT_THAT(Insert(3, kKeyFrame, kNotFirst, kLast).packets, IsEmpty());
}

TEST_F(PacketBufferTest, PacketTimestamps) {
  absl::optional<int64_t> packet_ms;
  absl::optional<int64_t> packet_keyframe_ms;

  packet_ms = packet_buffer_.LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_.LastReceivedKeyframePacketMs();
  EXPECT_FALSE(packet_ms);
  EXPECT_FALSE(packet_keyframe_ms);

  int64_t keyframe_ms = clock_.TimeInMilliseconds();
  Insert(100, kKeyFrame, kFirst, kLast, {}, /*timestamp=*/1000);
  packet_ms = packet_buffer_.LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_.LastReceivedKeyframePacketMs();
  EXPECT_TRUE(packet_ms);
  EXPECT_TRUE(packet_keyframe_ms);
  EXPECT_EQ(keyframe_ms, *packet_ms);
  EXPECT_EQ(keyframe_ms, *packet_keyframe_ms);

  clock_.AdvanceTimeMilliseconds(100);
  int64_t delta_ms = clock_.TimeInMilliseconds();
  Insert(101, kDeltaFrame, kFirst, kLast, {}, /*timestamp=*/2000);
  packet_ms = packet_buffer_.LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_.LastReceivedKeyframePacketMs();
  EXPECT_TRUE(packet_ms);
  EXPECT_TRUE(packet_keyframe_ms);
  EXPECT_EQ(delta_ms, *packet_ms);
  EXPECT_EQ(keyframe_ms, *packet_keyframe_ms);

  packet_buffer_.Clear();
  packet_ms = packet_buffer_.LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_.LastReceivedKeyframePacketMs();
  EXPECT_FALSE(packet_ms);
  EXPECT_FALSE(packet_keyframe_ms);
}

TEST_F(PacketBufferTest,
       LastReceivedKeyFrameReturnsReceiveTimeOfALastReceivedPacketOfAKeyFrame) {
  clock_.AdvanceTimeMilliseconds(100);
  Insert(/*seq_num=*/100, kKeyFrame, kFirst, kNotLast, {}, /*timestamp=*/1000);
  EXPECT_EQ(packet_buffer_.LastReceivedKeyframePacketMs(),
            clock_.TimeInMilliseconds());

  clock_.AdvanceTimeMilliseconds(100);
  Insert(/*seq_num=*/102, kDeltaFrame, kNotFirst, kLast, {},
         /*timestamp=*/1000);
  EXPECT_EQ(packet_buffer_.LastReceivedKeyframePacketMs(),
            clock_.TimeInMilliseconds());

  clock_.AdvanceTimeMilliseconds(100);
  Insert(/*seq_num=*/101, kDeltaFrame, kNotFirst, kNotLast, {},
         /*timestamp=*/1000);
  EXPECT_EQ(packet_buffer_.LastReceivedKeyframePacketMs(),
            clock_.TimeInMilliseconds());

  clock_.AdvanceTimeMilliseconds(100);
  Insert(/*seq_num=*/103, kDeltaFrame, kFirst, kNotLast, {},
         /*timestamp=*/2000);
  EXPECT_EQ(packet_buffer_.LastReceivedKeyframePacketMs(),
            clock_.TimeInMilliseconds() - 100);
}

TEST_F(PacketBufferTest, IncomingCodecChange) {
  auto packet = std::make_unique<PacketBuffer::Packet>();
  packet->video_header.is_first_packet_in_frame = true;
  packet->video_header.is_last_packet_in_frame = true;
  packet->video_header.codec = kVideoCodecVP8;
  packet->video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  packet->timestamp = 1;
  packet->seq_num = 1;
  packet->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              SizeIs(1));

  packet = std::make_unique<PacketBuffer::Packet>();
  packet->video_header.is_first_packet_in_frame = true;
  packet->video_header.is_last_packet_in_frame = true;
  packet->video_header.codec = kVideoCodecH264;
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = 1;
  packet->timestamp = 3;
  packet->seq_num = 3;
  packet->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              IsEmpty());

  packet = std::make_unique<PacketBuffer::Packet>();
  packet->video_header.is_first_packet_in_frame = true;
  packet->video_header.is_last_packet_in_frame = true;
  packet->video_header.codec = kVideoCodecVP8;
  packet->video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  packet->timestamp = 2;
  packet->seq_num = 2;
  packet->video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              SizeIs(2));
}

TEST_F(PacketBufferTest, TooManyNalusInPacket) {
  auto packet = std::make_unique<PacketBuffer::Packet>();
  packet->video_header.codec = kVideoCodecH264;
  packet->timestamp = 1;
  packet->seq_num = 1;
  packet->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  packet->video_header.is_first_packet_in_frame = true;
  packet->video_header.is_last_packet_in_frame = true;
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = kMaxNalusPerPacket;
  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              IsEmpty());
}

TEST_P(PacketBufferH264ParameterizedTest, OneFrameFillBuffer) {
  InsertH264(0, kKeyFrame, kFirst, kNotLast, 1000);
  for (int i = 1; i < kStartSize - 1; ++i)
    InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1000);
  EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1000),
              StartSeqNumsAre(0));
}

TEST_P(PacketBufferH264ParameterizedTest, CreateFramesAfterFilledBuffer) {
  EXPECT_THAT(InsertH264(kStartSize - 2, kKeyFrame, kFirst, kLast, 0).packets,
              SizeIs(1));

  InsertH264(kStartSize, kDeltaFrame, kFirst, kNotLast, 2000);
  for (int i = 1; i < kStartSize; ++i)
    InsertH264(kStartSize + i, kDeltaFrame, kNotFirst, kNotLast, 2000);
  EXPECT_THAT(
      InsertH264(kStartSize + kStartSize, kDeltaFrame, kNotFirst, kLast, 2000)
          .packets,
      IsEmpty());

  EXPECT_THAT(InsertH264(kStartSize - 1, kKeyFrame, kFirst, kLast, 1000),
              StartSeqNumsAre(kStartSize - 1, kStartSize));
}

TEST_P(PacketBufferH264ParameterizedTest, OneFrameMaxSeqNum) {
  InsertH264(65534, kKeyFrame, kFirst, kNotLast, 1000);
  EXPECT_THAT(InsertH264(65535, kKeyFrame, kNotFirst, kLast, 1000),
              StartSeqNumsAre(65534));
}

TEST_P(PacketBufferH264ParameterizedTest, ClearMissingPacketsOnKeyframe) {
  InsertH264(0, kKeyFrame, kFirst, kLast, 1000);
  InsertH264(2, kKeyFrame, kFirst, kLast, 3000);
  InsertH264(3, kDeltaFrame, kFirst, kNotLast, 4000);
  InsertH264(4, kDeltaFrame, kNotFirst, kLast, 4000);

  EXPECT_THAT(InsertH264(kStartSize + 1, kKeyFrame, kFirst, kLast, 18000),
              StartSeqNumsAre(kStartSize + 1));
}

TEST_P(PacketBufferH264ParameterizedTest, FindFramesOnPadding) {
  EXPECT_THAT(InsertH264(0, kKeyFrame, kFirst, kLast, 1000),
              StartSeqNumsAre(0));
  EXPECT_THAT(InsertH264(2, kDeltaFrame, kFirst, kLast, 1000).packets,
              IsEmpty());

  EXPECT_THAT(packet_buffer_.InsertPadding(1), StartSeqNumsAre(2));
}

class PacketBufferH264XIsKeyframeTest : public PacketBufferH264Test {
 protected:
  const uint16_t kSeqNum = 5;

  explicit PacketBufferH264XIsKeyframeTest(bool sps_pps_idr_is_keyframe)
      : PacketBufferH264Test(sps_pps_idr_is_keyframe) {}

  std::unique_ptr<PacketBuffer::Packet> CreatePacket() {
    auto packet = std::make_unique<PacketBuffer::Packet>();
    packet->video_header.codec = kVideoCodecH264;
    packet->seq_num = kSeqNum;

    packet->video_header.is_first_packet_in_frame = true;
    packet->video_header.is_last_packet_in_frame = true;
    return packet;
  }
};

class PacketBufferH264IdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
 protected:
  PacketBufferH264IdrIsKeyframeTest()
      : PacketBufferH264XIsKeyframeTest(false) {}
};

TEST_F(PacketBufferH264IdrIsKeyframeTest, IdrIsKeyframe) {
  auto packet = CreatePacket();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 1;
  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              ElementsAre(KeyFrame()));
}

TEST_F(PacketBufferH264IdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
  auto packet = CreatePacket();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus[2].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 3;

  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              ElementsAre(KeyFrame()));
}

class PacketBufferH264SpsPpsIdrIsKeyframeTest
    : public PacketBufferH264XIsKeyframeTest {
 protected:
  PacketBufferH264SpsPpsIdrIsKeyframeTest()
      : PacketBufferH264XIsKeyframeTest(true) {}
};

TEST_F(PacketBufferH264SpsPpsIdrIsKeyframeTest, IdrIsNotKeyframe) {
  auto packet = CreatePacket();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 1;

  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              ElementsAre(DeltaFrame()));
}

TEST_F(PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIsNotKeyframe) {
  auto packet = CreatePacket();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus_length = 2;

  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              ElementsAre(DeltaFrame()));
}

TEST_F(PacketBufferH264SpsPpsIdrIsKeyframeTest, SpsPpsIdrIsKeyframe) {
  auto packet = CreatePacket();
  auto& h264_header =
      packet->video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus[2].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 3;

  EXPECT_THAT(packet_buffer_.InsertPacket(std::move(packet)).packets,
              ElementsAre(KeyFrame()));
}

}  // namespace
}  // namespace video_coding
}  // namespace webrtc
