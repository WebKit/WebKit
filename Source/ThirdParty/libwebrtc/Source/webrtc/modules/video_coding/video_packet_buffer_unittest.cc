/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <map>
#include <set>
#include <utility>

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {

class TestPacketBuffer : public ::testing::Test,
                         public OnReceivedFrameCallback {
 protected:
  TestPacketBuffer() : TestPacketBuffer("") {}
  explicit TestPacketBuffer(std::string field_trials)
      : scoped_field_trials_(field_trials),
        rand_(0x7732213),
        clock_(new SimulatedClock(0)),
        packet_buffer_(
            PacketBuffer::Create(clock_.get(), kStartSize, kMaxSize, this)) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnReceivedFrame(std::unique_ptr<RtpFrameObject> frame) override {
    uint16_t first_seq_num = frame->first_seq_num();
    if (frames_from_callback_.find(first_seq_num) !=
        frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with first sequence number "
                    << first_seq_num << ".";
      return;
    }

    frames_from_callback_.insert(
        std::make_pair(frame->first_seq_num(), std::move(frame)));
  }

  enum IsKeyFrame { kKeyFrame, kDeltaFrame };
  enum IsFirst { kFirst, kNotFirst };
  enum IsLast { kLast, kNotLast };

  bool Insert(uint16_t seq_num,             // packet sequence number
              IsKeyFrame keyframe,          // is keyframe
              IsFirst first,                // is first packet of frame
              IsLast last,                  // is last packet of frame
              int data_size = 0,            // size of data
              uint8_t* data = nullptr,      // data pointer
              uint32_t timestamp = 123u) {  // rtp timestamp
    VCMPacket packet;
    packet.codec = kVideoCodecGeneric;
    packet.timestamp = timestamp;
    packet.seqNum = seq_num;
    packet.frameType =
        keyframe == kKeyFrame ? kVideoFrameKey : kVideoFrameDelta;
    packet.is_first_packet_in_frame = first == kFirst;
    packet.is_last_packet_in_frame = last == kLast;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;

    return packet_buffer_->InsertPacket(&packet);
  }

  void CheckFrame(uint16_t first_seq_num) {
    auto frame_it = frames_from_callback_.find(first_seq_num);
    ASSERT_FALSE(frame_it == frames_from_callback_.end())
        << "Could not find frame with first sequence number " << first_seq_num
        << ".";
  }

  void DeleteFrame(uint16_t first_seq_num) {
    auto frame_it = frames_from_callback_.find(first_seq_num);
    ASSERT_FALSE(frame_it == frames_from_callback_.end())
        << "Could not find frame with first sequence number " << first_seq_num
        << ".";
    frames_from_callback_.erase(frame_it);
  }

  static constexpr int kStartSize = 16;
  static constexpr int kMaxSize = 64;

  const test::ScopedFieldTrials scoped_field_trials_;

  Random rand_;
  std::unique_ptr<SimulatedClock> clock_;
  rtc::scoped_refptr<PacketBuffer> packet_buffer_;
  std::map<uint16_t, std::unique_ptr<RtpFrameObject>> frames_from_callback_;
};

TEST_F(TestPacketBuffer, InsertOnePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, SeqNumWrapOneFrame) {
  EXPECT_TRUE(Insert(0xFFFF, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(0x0, kKeyFrame, kNotFirst, kLast));

  CheckFrame(0xFFFF);
}

TEST_F(TestPacketBuffer, SeqNumWrapTwoFrames) {
  EXPECT_TRUE(Insert(0xFFFF, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(0x0, kKeyFrame, kFirst, kLast));

  CheckFrame(0xFFFF);
  CheckFrame(0x0);
}

TEST_F(TestPacketBuffer, InsertOldPackets) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast));
  ASSERT_EQ(2UL, frames_from_callback_.size());

  frames_from_callback_.erase(seq_num + 2);
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  frames_from_callback_.erase(frames_from_callback_.find(seq_num));
  ASSERT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));

  packet_buffer_->ClearTo(seq_num + 2);
  EXPECT_FALSE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
  ASSERT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, NackCount) {
  const uint16_t seq_num = Rand();

  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.seqNum = seq_num;
  packet.frameType = kVideoFrameKey;
  packet.is_first_packet_in_frame = true;
  packet.is_last_packet_in_frame = false;
  packet.timesNacked = 0;

  packet_buffer_->InsertPacket(&packet);

  packet.seqNum++;
  packet.is_first_packet_in_frame = false;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(&packet);

  packet.seqNum++;
  packet.timesNacked = 3;
  packet_buffer_->InsertPacket(&packet);

  packet.seqNum++;
  packet.is_last_packet_in_frame = true;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(&packet);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  RtpFrameObject* frame = frames_from_callback_.begin()->second.get();
  EXPECT_EQ(3, frame->times_nacked());
}

TEST_F(TestPacketBuffer, FrameSize) {
  const uint16_t seq_num = Rand();
  uint8_t* data1 = new uint8_t[5]();
  uint8_t* data2 = new uint8_t[5]();
  uint8_t* data3 = new uint8_t[5]();
  uint8_t* data4 = new uint8_t[5]();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast, 5, data1));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, 5, data2));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, 5, data3));
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, 5, data4));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  EXPECT_EQ(20UL, frames_from_callback_.begin()->second->size());
}

TEST_F(TestPacketBuffer, CountsUniqueFrames) {
  const uint16_t seq_num = Rand();

  ASSERT_EQ(0, packet_buffer_->GetUniqueFramesSeen());

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast, 0, nullptr, 100));
  ASSERT_EQ(1, packet_buffer_->GetUniqueFramesSeen());
  // Still the same frame.
  EXPECT_TRUE(
      Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast, 0, nullptr, 100));
  ASSERT_EQ(1, packet_buffer_->GetUniqueFramesSeen());

  // Second frame.
  EXPECT_TRUE(
      Insert(seq_num + 2, kKeyFrame, kFirst, kNotLast, 0, nullptr, 200));
  ASSERT_EQ(2, packet_buffer_->GetUniqueFramesSeen());
  EXPECT_TRUE(
      Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, 0, nullptr, 200));
  ASSERT_EQ(2, packet_buffer_->GetUniqueFramesSeen());

  // Old packet.
  EXPECT_TRUE(
      Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast, 0, nullptr, 100));
  ASSERT_EQ(2, packet_buffer_->GetUniqueFramesSeen());

  // Missing middle packet.
  EXPECT_TRUE(
      Insert(seq_num + 4, kKeyFrame, kFirst, kNotLast, 0, nullptr, 300));
  EXPECT_TRUE(
      Insert(seq_num + 6, kKeyFrame, kNotFirst, kLast, 0, nullptr, 300));
  ASSERT_EQ(3, packet_buffer_->GetUniqueFramesSeen());
}

TEST_F(TestPacketBuffer, HasHistoryOfUniqueFrames) {
  const int kNumFrames = 1500;
  const int kRequiredHistoryLength = 1000;
  const uint16_t seq_num = Rand();
  const uint32_t timestamp = 0xFFFFFFF0;  // Large enough to cause wrap-around.

  for (int i = 0; i < kNumFrames; ++i) {
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kNotLast, 0, nullptr,
                       timestamp + 10 * i));
  }
  ASSERT_EQ(kNumFrames, packet_buffer_->GetUniqueFramesSeen());

  // Old packets within history should not affect number of seen unique frames.
  for (int i = kNumFrames - kRequiredHistoryLength; i < kNumFrames; ++i) {
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kNotLast, 0, nullptr,
                       timestamp + 10 * i));
  }
  ASSERT_EQ(kNumFrames, packet_buffer_->GetUniqueFramesSeen());

  // Very old packets should be treated as unique.
  EXPECT_TRUE(
      Insert(seq_num, kKeyFrame, kFirst, kNotLast, 0, nullptr, timestamp));
  ASSERT_EQ(kNumFrames + 1, packet_buffer_->GetUniqueFramesSeen());
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kLast));
  }
}

TEST_F(TestPacketBuffer, SingleFrameExpandsBuffer) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  for (int i = 1; i < kStartSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + kMaxSize + 1, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, OnePacketOneFrame) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, TwoPacketsTwoFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kFirst, kLast));

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
}

TEST_F(TestPacketBuffer, TwoPacketsOneFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast));

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, ThreePacketReorderingOneFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast));

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, Frames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, ClearSinglePacket) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kDeltaFrame, kFirst, kLast));

  packet_buffer_->ClearTo(seq_num);
  EXPECT_TRUE(Insert(seq_num + kMaxSize, kDeltaFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, ClearFullBuffer) {
  for (int i = 0; i < kMaxSize; ++i)
    EXPECT_TRUE(Insert(i, kDeltaFrame, kFirst, kLast));

  packet_buffer_->ClearTo(kMaxSize - 1);

  for (int i = kMaxSize; i < 2 * kMaxSize; ++i)
    EXPECT_TRUE(Insert(i, kDeltaFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, DontClearNewerPacket) {
  EXPECT_TRUE(Insert(0, kKeyFrame, kFirst, kLast));
  packet_buffer_->ClearTo(0);
  EXPECT_TRUE(Insert(2 * kStartSize, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(3 * kStartSize + 1, kKeyFrame, kFirst, kNotLast));
  packet_buffer_->ClearTo(2 * kStartSize);
  EXPECT_TRUE(Insert(3 * kStartSize + 2, kKeyFrame, kNotFirst, kLast));

  ASSERT_EQ(3UL, frames_from_callback_.size());
  CheckFrame(0);
  CheckFrame(2 * kStartSize);
  CheckFrame(3 * kStartSize + 1);
}

TEST_F(TestPacketBuffer, OneIncompleteFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kDeltaFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kLast));
  EXPECT_TRUE(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, TwoIncompleteFramesFullBuffer) {
  const uint16_t seq_num = Rand();

  for (int i = 1; i < kMaxSize - 1; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num, kDeltaFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast));

  ASSERT_EQ(0UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, FramesReordered) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, GetBitstream) {
  // "many bitstream, such data" with null termination.
  uint8_t many_data[] = {0x6d, 0x61, 0x6e, 0x79, 0x20};
  uint8_t bitstream_data[] = {0x62, 0x69, 0x74, 0x73, 0x74, 0x72,
                              0x65, 0x61, 0x6d, 0x2c, 0x20};
  uint8_t such_data[] = {0x73, 0x75, 0x63, 0x68, 0x20};
  uint8_t data_data[] = {0x64, 0x61, 0x74, 0x61, 0x0};

  uint8_t* many = new uint8_t[sizeof(many_data)];
  uint8_t* bitstream = new uint8_t[sizeof(bitstream_data)];
  uint8_t* such = new uint8_t[sizeof(such_data)];
  uint8_t* data = new uint8_t[sizeof(data_data)];

  memcpy(many, many_data, sizeof(many_data));
  memcpy(bitstream, bitstream_data, sizeof(bitstream_data));
  memcpy(such, such_data, sizeof(such_data));
  memcpy(data, data_data, sizeof(data_data));

  uint8_t result[sizeof(many_data) + sizeof(bitstream_data) +
                 sizeof(such_data) + sizeof(data_data)];

  const uint16_t seq_num = Rand();

  EXPECT_TRUE(
      Insert(seq_num, kKeyFrame, kFirst, kNotLast, sizeof(many_data), many));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast,
                     sizeof(bitstream_data), bitstream));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kNotLast,
                     sizeof(such_data), such));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kNotFirst, kLast,
                     sizeof(data_data), data));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  EXPECT_EQ(frames_from_callback_[seq_num]->size(), sizeof(result));
  EXPECT_TRUE(frames_from_callback_[seq_num]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, "many bitstream, such data", sizeof(result)), 0);
}

TEST_F(TestPacketBuffer, GetBitstreamOneFrameOnePacket) {
  uint8_t bitstream_data[] = "All the bitstream data for this frame!";
  uint8_t result[sizeof(bitstream_data)];
  uint8_t* data = new uint8_t[sizeof(bitstream_data)];
  memcpy(data, bitstream_data, sizeof(bitstream_data));

  EXPECT_TRUE(
      Insert(0, kKeyFrame, kFirst, kLast, sizeof(bitstream_data), data));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(0);
  EXPECT_EQ(frames_from_callback_[0]->size(), sizeof(bitstream_data));
  EXPECT_TRUE(frames_from_callback_[0]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, data, sizeof(bitstream_data)), 0);
}

TEST_F(TestPacketBuffer, GetBitstreamOneFrameFullBuffer) {
  uint8_t* data_arr[kStartSize];
  uint8_t expected[kStartSize];
  uint8_t result[kStartSize];

  for (uint8_t i = 0; i < kStartSize; ++i) {
    data_arr[i] = new uint8_t[1];
    data_arr[i][0] = i;
    expected[i] = i;
  }

  EXPECT_TRUE(Insert(0, kKeyFrame, kFirst, kNotLast, 1, data_arr[0]));
  for (uint8_t i = 1; i < kStartSize - 1; ++i)
    EXPECT_TRUE(Insert(i, kKeyFrame, kNotFirst, kNotLast, 1, data_arr[i]));
  EXPECT_TRUE(Insert(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1,
                     data_arr[kStartSize - 1]));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(0);
  EXPECT_EQ(frames_from_callback_[0]->size(), static_cast<size_t>(kStartSize));
  EXPECT_TRUE(frames_from_callback_[0]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, expected, kStartSize), 0);
}

TEST_F(TestPacketBuffer, InsertPacketAfterOldFrameObjectIsRemoved) {
  uint16_t kFirstSeqNum = 0;
  uint32_t kTimestampDelta = 100;
  uint32_t timestamp = 10000;
  uint16_t seq_num = kFirstSeqNum;

  // Loop until seq_num wraps around.
  SeqNumUnwrapper<uint16_t> unwrapper(0);
  while (unwrapper.Unwrap(seq_num) < std::numeric_limits<uint16_t>::max()) {
    Insert(seq_num++, kKeyFrame, kFirst, kNotLast, 0, nullptr, timestamp);
    for (int i = 0; i < 5; ++i) {
      Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, 0, nullptr, timestamp);
    }
    Insert(seq_num++, kKeyFrame, kNotFirst, kLast, 0, nullptr, timestamp);
    timestamp += kTimestampDelta;
  }

  size_t number_of_frames = frames_from_callback_.size();
  // Delete old frame object while receiving frame with overlapping sequence
  // numbers.
  Insert(seq_num++, kKeyFrame, kFirst, kNotLast, 0, nullptr, timestamp);
  for (int i = 0; i < 5; ++i) {
    Insert(seq_num++, kKeyFrame, kNotFirst, kNotLast, 0, nullptr, timestamp);
  }
  // Delete FrameObject connected to packets that have already been cleared.
  DeleteFrame(kFirstSeqNum);
  Insert(seq_num++, kKeyFrame, kNotFirst, kLast, 0, nullptr, timestamp);

  // Regardless of the initial size, the number of frames should be constant
  // after removing and then adding a new frame object.
  EXPECT_EQ(number_of_frames, frames_from_callback_.size());
}

// If |sps_pps_idr_is_keyframe| is true, we require keyframes to contain
// SPS/PPS/IDR and the keyframes we create as part of the test do contain
// SPS/PPS/IDR. If |sps_pps_idr_is_keyframe| is false, we only require and
// create keyframes containing only IDR.
class TestPacketBufferH264 : public TestPacketBuffer {
 protected:
  explicit TestPacketBufferH264(bool sps_pps_idr_is_keyframe)
      : TestPacketBuffer(sps_pps_idr_is_keyframe
                             ? "WebRTC-SpsPpsIdrIsH264Keyframe/Enabled/"
                             : ""),
        sps_pps_idr_is_keyframe_(sps_pps_idr_is_keyframe) {}

  bool InsertH264(uint16_t seq_num,           // packet sequence number
                  IsKeyFrame keyframe,        // is keyframe
                  IsFirst first,              // is first packet of frame
                  IsLast last,                // is last packet of frame
                  uint32_t timestamp,         // rtp timestamp
                  int data_size = 0,          // size of data
                  uint8_t* data = nullptr) {  // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecH264;
    auto& h264_header =
        packet.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
    packet.seqNum = seq_num;
    packet.timestamp = timestamp;
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
    packet.is_first_packet_in_frame = first == kFirst;
    packet.is_last_packet_in_frame = last == kLast;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;

    return packet_buffer_->InsertPacket(&packet);
  }

  const bool sps_pps_idr_is_keyframe_;
};

// This fixture is used to test the general behaviour of the packet buffer
// in both configurations.
class TestPacketBufferH264Parameterized
    : public ::testing::WithParamInterface<bool>,
      public TestPacketBufferH264 {
 protected:
  TestPacketBufferH264Parameterized() : TestPacketBufferH264(GetParam()) {}
};

INSTANTIATE_TEST_CASE_P(SpsPpsIdrIsKeyframe,
                        TestPacketBufferH264Parameterized,
                        ::testing::Values(false, true));

TEST_P(TestPacketBufferH264Parameterized, DontRemoveMissingPacketOnClearTo) {
  EXPECT_TRUE(InsertH264(0, kKeyFrame, kFirst, kLast, 0));
  EXPECT_TRUE(InsertH264(2, kDeltaFrame, kFirst, kNotLast, 2));
  packet_buffer_->ClearTo(0);
  EXPECT_TRUE(InsertH264(3, kDeltaFrame, kNotFirst, kLast, 2));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(0);
}

TEST_P(TestPacketBufferH264Parameterized, GetBitstreamOneFrameFullBuffer) {
  uint8_t* data_arr[kStartSize];
  uint8_t expected[kStartSize];
  uint8_t result[kStartSize];

  for (uint8_t i = 0; i < kStartSize; ++i) {
    data_arr[i] = new uint8_t[1];
    data_arr[i][0] = i;
    expected[i] = i;
  }

  EXPECT_TRUE(InsertH264(0, kKeyFrame, kFirst, kNotLast, 1, 1, data_arr[0]));
  for (uint8_t i = 1; i < kStartSize - 1; ++i) {
    EXPECT_TRUE(
        InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1, 1, data_arr[i]));
  }
  EXPECT_TRUE(InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1, 1,
                         data_arr[kStartSize - 1]));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(0);
  EXPECT_EQ(frames_from_callback_[0]->size(), static_cast<size_t>(kStartSize));
  EXPECT_TRUE(frames_from_callback_[0]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, expected, kStartSize), 0);
}

TEST_P(TestPacketBufferH264Parameterized, GetBitstreamBufferPadding) {
  uint16_t seq_num = Rand();
  uint8_t data_data[] = "some plain old data";
  uint8_t* data = new uint8_t[sizeof(data_data)];
  memcpy(data, data_data, sizeof(data_data));

  // EncodedImage::kBufferPaddingBytesH264 is unknown at compile time.
  std::unique_ptr<uint8_t[]> result(
      new uint8_t[sizeof(data_data) + EncodedImage::kBufferPaddingBytesH264]);

  VCMPacket packet;
  auto& h264_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = 1;
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.packetization_type = kH264SingleNalu;
  packet.seqNum = seq_num;
  packet.codec = kVideoCodecH264;
  packet.insertStartCode = true;
  packet.dataPtr = data;
  packet.sizeBytes = sizeof(data_data);
  packet.is_first_packet_in_frame = true;
  packet.is_last_packet_in_frame = true;
  packet_buffer_->InsertPacket(&packet);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  EXPECT_EQ(frames_from_callback_[seq_num]->EncodedImage()._length,
            sizeof(data_data));
  EXPECT_EQ(frames_from_callback_[seq_num]->EncodedImage()._size,
            sizeof(data_data) + EncodedImage::kBufferPaddingBytesH264);
  EXPECT_TRUE(frames_from_callback_[seq_num]->GetBitstream(result.get()));
  EXPECT_EQ(memcmp(result.get(), data, sizeof(data_data)), 0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  frames_from_callback_.clear();

  // Insert frame that fills the whole buffer.
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kFirst, kNotLast));
  for (int i = 0; i < kMaxSize - 2; ++i)
    EXPECT_TRUE(Insert(seq_num + i + 4, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kMaxSize + 2, kKeyFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, Clear) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  packet_buffer_->Clear();

  EXPECT_TRUE(Insert(seq_num + kStartSize, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(
      Insert(seq_num + kStartSize + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kStartSize + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num + kStartSize);
}

TEST_F(TestPacketBuffer, InvalidateFrameByClearing) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Clear();
  EXPECT_FALSE(frames_from_callback_.begin()->second->GetBitstream(nullptr));
}

TEST_F(TestPacketBuffer, FramesAfterClear) {
  Insert(9025, kDeltaFrame, kFirst, kLast);
  Insert(9024, kKeyFrame, kFirst, kLast);
  packet_buffer_->ClearTo(9025);
  Insert(9057, kDeltaFrame, kFirst, kLast);
  Insert(9026, kDeltaFrame, kFirst, kLast);

  CheckFrame(9024);
  CheckFrame(9025);
  CheckFrame(9026);
  CheckFrame(9057);
}

TEST_F(TestPacketBuffer, DontLeakPayloadData) {
  // NOTE! Any eventual leak is suppose to be detected by valgrind
  //       or any other similar tool.
  uint8_t* data1 = new uint8_t[5];
  uint8_t* data2 = new uint8_t[5];
  uint8_t* data3 = new uint8_t[5];
  uint8_t* data4 = new uint8_t[5];

  // Expected to free data1 upon PacketBuffer destruction.
  EXPECT_TRUE(Insert(2, kKeyFrame, kFirst, kNotLast, 5, data1));

  // Expect to free data2 upon insertion.
  EXPECT_TRUE(Insert(2, kKeyFrame, kFirst, kNotLast, 5, data2));

  // Expect to free data3 upon insertion (old packet).
  packet_buffer_->ClearTo(1);
  EXPECT_FALSE(Insert(1, kKeyFrame, kFirst, kNotLast, 5, data3));

  // Expect to free data4 upon insertion (packet buffer is full).
  EXPECT_TRUE(Insert(2 + kMaxSize, kKeyFrame, kFirst, kNotLast, 5, data4));
}

TEST_F(TestPacketBuffer, ContinuousSeqNumDoubleMarkerBit) {
  Insert(2, kKeyFrame, kNotFirst, kNotLast);
  Insert(1, kKeyFrame, kFirst, kLast);
  frames_from_callback_.clear();
  Insert(3, kKeyFrame, kNotFirst, kLast);

  EXPECT_EQ(0UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, PacketTimestamps) {
  absl::optional<int64_t> packet_ms;
  absl::optional<int64_t> packet_keyframe_ms;

  packet_ms = packet_buffer_->LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_->LastReceivedKeyframePacketMs();
  EXPECT_FALSE(packet_ms);
  EXPECT_FALSE(packet_keyframe_ms);

  int64_t keyframe_ms = clock_->TimeInMilliseconds();
  EXPECT_TRUE(Insert(100, kKeyFrame, kFirst, kLast));
  packet_ms = packet_buffer_->LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_->LastReceivedKeyframePacketMs();
  EXPECT_TRUE(packet_ms);
  EXPECT_TRUE(packet_keyframe_ms);
  EXPECT_EQ(keyframe_ms, *packet_ms);
  EXPECT_EQ(keyframe_ms, *packet_keyframe_ms);

  clock_->AdvanceTimeMilliseconds(100);
  int64_t delta_ms = clock_->TimeInMilliseconds();
  EXPECT_TRUE(Insert(101, kDeltaFrame, kFirst, kLast));
  packet_ms = packet_buffer_->LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_->LastReceivedKeyframePacketMs();
  EXPECT_TRUE(packet_ms);
  EXPECT_TRUE(packet_keyframe_ms);
  EXPECT_EQ(delta_ms, *packet_ms);
  EXPECT_EQ(keyframe_ms, *packet_keyframe_ms);

  packet_buffer_->Clear();
  packet_ms = packet_buffer_->LastReceivedPacketMs();
  packet_keyframe_ms = packet_buffer_->LastReceivedKeyframePacketMs();
  EXPECT_FALSE(packet_ms);
  EXPECT_FALSE(packet_keyframe_ms);
}

TEST_F(TestPacketBuffer, IncomingCodecChange) {
  VCMPacket packet;
  packet.is_first_packet_in_frame = true;
  packet.is_last_packet_in_frame = true;
  packet.sizeBytes = 0;
  packet.dataPtr = nullptr;

  packet.codec = kVideoCodecVP8;
  packet.timestamp = 1;
  packet.seqNum = 1;
  packet.frameType = kVideoFrameKey;
  EXPECT_TRUE(packet_buffer_->InsertPacket(&packet));

  packet.codec = kVideoCodecH264;
  auto& h264_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = 1;
  packet.timestamp = 3;
  packet.seqNum = 3;
  EXPECT_TRUE(packet_buffer_->InsertPacket(&packet));

  packet.codec = kVideoCodecVP8;
  packet.timestamp = 2;
  packet.seqNum = 2;
  packet.frameType = kVideoFrameDelta;

  EXPECT_TRUE(packet_buffer_->InsertPacket(&packet));

  EXPECT_EQ(3UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, TooManyNalusInPacket) {
  VCMPacket packet;
  packet.codec = kVideoCodecH264;
  packet.timestamp = 1;
  packet.seqNum = 1;
  packet.frameType = kVideoFrameKey;
  packet.is_first_packet_in_frame = true;
  packet.is_last_packet_in_frame = true;
  auto& h264_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus_length = kMaxNalusPerPacket;
  packet.sizeBytes = 0;
  packet.dataPtr = nullptr;
  EXPECT_TRUE(packet_buffer_->InsertPacket(&packet));

  EXPECT_EQ(0UL, frames_from_callback_.size());
}

TEST_P(TestPacketBufferH264Parameterized, OneFrameFillBuffer) {
  InsertH264(0, kKeyFrame, kFirst, kNotLast, 1000);
  for (int i = 1; i < kStartSize - 1; ++i)
    InsertH264(i, kKeyFrame, kNotFirst, kNotLast, 1000);
  InsertH264(kStartSize - 1, kKeyFrame, kNotFirst, kLast, 1000);

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(0);
}

TEST_P(TestPacketBufferH264Parameterized, CreateFramesAfterFilledBuffer) {
  InsertH264(kStartSize - 2, kKeyFrame, kFirst, kLast, 0);
  ASSERT_EQ(1UL, frames_from_callback_.size());
  frames_from_callback_.clear();

  InsertH264(kStartSize, kDeltaFrame, kFirst, kNotLast, 2000);
  for (int i = 1; i < kStartSize; ++i)
    InsertH264(kStartSize + i, kDeltaFrame, kNotFirst, kNotLast, 2000);
  InsertH264(kStartSize + kStartSize, kDeltaFrame, kNotFirst, kLast, 2000);
  ASSERT_EQ(0UL, frames_from_callback_.size());

  InsertH264(kStartSize - 1, kKeyFrame, kFirst, kLast, 1000);
  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(kStartSize - 1);
  CheckFrame(kStartSize);
}

TEST_P(TestPacketBufferH264Parameterized, OneFrameMaxSeqNum) {
  InsertH264(65534, kKeyFrame, kFirst, kNotLast, 1000);
  InsertH264(65535, kKeyFrame, kNotFirst, kLast, 1000);

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(65534);
}

TEST_P(TestPacketBufferH264Parameterized, ClearMissingPacketsOnKeyframe) {
  InsertH264(0, kKeyFrame, kFirst, kLast, 1000);
  InsertH264(2, kKeyFrame, kFirst, kLast, 3000);
  InsertH264(3, kDeltaFrame, kFirst, kNotLast, 4000);
  InsertH264(4, kDeltaFrame, kNotFirst, kLast, 4000);

  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertH264(kStartSize + 1, kKeyFrame, kFirst, kLast, 18000);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(0);
  CheckFrame(2);
  CheckFrame(3);
  CheckFrame(kStartSize + 1);
}

TEST_P(TestPacketBufferH264Parameterized, FindFramesOnPadding) {
  InsertH264(0, kKeyFrame, kFirst, kLast, 1000);
  InsertH264(2, kDeltaFrame, kFirst, kLast, 1000);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  packet_buffer_->PaddingReceived(1);
  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(0);
  CheckFrame(2);
}

class TestPacketBufferH264XIsKeyframe : public TestPacketBufferH264 {
 protected:
  const uint16_t kSeqNum = 5;

  explicit TestPacketBufferH264XIsKeyframe(bool sps_pps_idr_is_keyframe)
      : TestPacketBufferH264(sps_pps_idr_is_keyframe) {
    packet_.codec = kVideoCodecH264;
    packet_.seqNum = kSeqNum;

    packet_.is_first_packet_in_frame = true;
    packet_.is_last_packet_in_frame = true;
  }

  VCMPacket packet_;
};

class TestPacketBufferH264IdrIsKeyframe
    : public TestPacketBufferH264XIsKeyframe {
 protected:
  TestPacketBufferH264IdrIsKeyframe()
      : TestPacketBufferH264XIsKeyframe(false) {}
};

TEST_F(TestPacketBufferH264IdrIsKeyframe, IdrIsKeyframe) {
  auto& h264_header =
      packet_.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 1;
  packet_buffer_->InsertPacket(&packet_);

  ASSERT_EQ(1u, frames_from_callback_.size());
  EXPECT_EQ(kVideoFrameKey, frames_from_callback_[kSeqNum]->frame_type());
}

TEST_F(TestPacketBufferH264IdrIsKeyframe, SpsPpsIdrIsKeyframe) {
  auto& h264_header =
      packet_.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus[2].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 3;

  packet_buffer_->InsertPacket(&packet_);

  ASSERT_EQ(1u, frames_from_callback_.size());
  EXPECT_EQ(kVideoFrameKey, frames_from_callback_[kSeqNum]->frame_type());
}

class TestPacketBufferH264SpsPpsIdrIsKeyframe
    : public TestPacketBufferH264XIsKeyframe {
 protected:
  TestPacketBufferH264SpsPpsIdrIsKeyframe()
      : TestPacketBufferH264XIsKeyframe(true) {}
};

TEST_F(TestPacketBufferH264SpsPpsIdrIsKeyframe, IdrIsNotKeyframe) {
  auto& h264_header =
      packet_.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 1;

  packet_buffer_->InsertPacket(&packet_);

  ASSERT_EQ(1u, frames_from_callback_.size());
  EXPECT_EQ(kVideoFrameDelta, frames_from_callback_[5]->frame_type());
}

TEST_F(TestPacketBufferH264SpsPpsIdrIsKeyframe, SpsPpsIsNotKeyframe) {
  auto& h264_header =
      packet_.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus_length = 2;

  packet_buffer_->InsertPacket(&packet_);

  ASSERT_EQ(1u, frames_from_callback_.size());
  EXPECT_EQ(kVideoFrameDelta, frames_from_callback_[kSeqNum]->frame_type());
}

TEST_F(TestPacketBufferH264SpsPpsIdrIsKeyframe, SpsPpsIdrIsKeyframe) {
  auto& h264_header =
      packet_.video_header.video_type_header.emplace<RTPVideoHeaderH264>();
  h264_header.nalus[0].type = H264::NaluType::kSps;
  h264_header.nalus[1].type = H264::NaluType::kPps;
  h264_header.nalus[2].type = H264::NaluType::kIdr;
  h264_header.nalus_length = 3;

  packet_buffer_->InsertPacket(&packet_);

  ASSERT_EQ(1u, frames_from_callback_.size());
  EXPECT_EQ(kVideoFrameKey, frames_from_callback_[kSeqNum]->frame_type());
}

}  // namespace video_coding
}  // namespace webrtc
