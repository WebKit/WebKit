/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include <list>
#include <memory>
#include <vector>

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/frame_buffer.h"
#include "modules/video_coding/jitter_buffer.h"
#include "modules/video_coding/media_opt_util.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/test/stream_generator.h"
#include "modules/video_coding/test/test_util.h"
#include "rtc_base/location.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/metrics_default.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const uint32_t kProcessIntervalSec = 60;
}  // namespace

class Vp9SsMapTest : public ::testing::Test {
 protected:
  Vp9SsMapTest() : packet_() {}

  virtual void SetUp() {
    packet_.is_first_packet_in_frame = true;
    packet_.dataPtr = data_;
    packet_.sizeBytes = 1400;
    packet_.seqNum = 1234;
    packet_.timestamp = 1;
    packet_.markerBit = true;
    packet_.frameType = kVideoFrameKey;
    packet_.codec = kVideoCodecVP9;
    packet_.video_header.codec = kRtpVideoVp9;
    packet_.video_header.codecHeader.VP9.flexible_mode = false;
    packet_.video_header.codecHeader.VP9.gof_idx = 0;
    packet_.video_header.codecHeader.VP9.temporal_idx = kNoTemporalIdx;
    packet_.video_header.codecHeader.VP9.temporal_up_switch = false;
    packet_.video_header.codecHeader.VP9.ss_data_available = true;
    packet_.video_header.codecHeader.VP9.gof.SetGofInfoVP9(
        kTemporalStructureMode3);  // kTemporalStructureMode3: 0-2-1-2..
  }

  Vp9SsMap map_;
  uint8_t data_[1500];
  VCMPacket packet_;
};

TEST_F(Vp9SsMapTest, Insert) {
  EXPECT_TRUE(map_.Insert(packet_));
}

TEST_F(Vp9SsMapTest, Insert_NoSsData) {
  packet_.video_header.codecHeader.VP9.ss_data_available = false;
  EXPECT_FALSE(map_.Insert(packet_));
}

TEST_F(Vp9SsMapTest, Find) {
  EXPECT_TRUE(map_.Insert(packet_));
  Vp9SsMap::SsMap::iterator it;
  EXPECT_TRUE(map_.Find(packet_.timestamp, &it));
  EXPECT_EQ(packet_.timestamp, it->first);
}

TEST_F(Vp9SsMapTest, Find_WithWrap) {
  const uint32_t kSsTimestamp1 = 0xFFFFFFFF;
  const uint32_t kSsTimestamp2 = 100;
  packet_.timestamp = kSsTimestamp1;
  EXPECT_TRUE(map_.Insert(packet_));
  packet_.timestamp = kSsTimestamp2;
  EXPECT_TRUE(map_.Insert(packet_));
  Vp9SsMap::SsMap::iterator it;
  EXPECT_FALSE(map_.Find(kSsTimestamp1 - 1, &it));
  EXPECT_TRUE(map_.Find(kSsTimestamp1, &it));
  EXPECT_EQ(kSsTimestamp1, it->first);
  EXPECT_TRUE(map_.Find(0, &it));
  EXPECT_EQ(kSsTimestamp1, it->first);
  EXPECT_TRUE(map_.Find(kSsTimestamp2 - 1, &it));
  EXPECT_EQ(kSsTimestamp1, it->first);
  EXPECT_TRUE(map_.Find(kSsTimestamp2, &it));
  EXPECT_EQ(kSsTimestamp2, it->first);
  EXPECT_TRUE(map_.Find(kSsTimestamp2 + 1, &it));
  EXPECT_EQ(kSsTimestamp2, it->first);
}

TEST_F(Vp9SsMapTest, Reset) {
  EXPECT_TRUE(map_.Insert(packet_));
  Vp9SsMap::SsMap::iterator it;
  EXPECT_TRUE(map_.Find(packet_.timestamp, &it));
  EXPECT_EQ(packet_.timestamp, it->first);

  map_.Reset();
  EXPECT_FALSE(map_.Find(packet_.timestamp, &it));
}

TEST_F(Vp9SsMapTest, RemoveOld) {
  Vp9SsMap::SsMap::iterator it;
  const uint32_t kSsTimestamp1 = 10000;
  packet_.timestamp = kSsTimestamp1;
  EXPECT_TRUE(map_.Insert(packet_));

  const uint32_t kTimestamp = kSsTimestamp1 + kProcessIntervalSec * 90000;
  map_.RemoveOld(kTimestamp - 1);              // Interval not passed.
  EXPECT_TRUE(map_.Find(kSsTimestamp1, &it));  // Should not been removed.

  map_.RemoveOld(kTimestamp);
  EXPECT_FALSE(map_.Find(kSsTimestamp1, &it));
  EXPECT_TRUE(map_.Find(kTimestamp, &it));
  EXPECT_EQ(kTimestamp, it->first);
}

TEST_F(Vp9SsMapTest, RemoveOld_WithWrap) {
  Vp9SsMap::SsMap::iterator it;
  const uint32_t kSsTimestamp1 = 0xFFFFFFFF - kProcessIntervalSec * 90000;
  const uint32_t kSsTimestamp2 = 10;
  const uint32_t kSsTimestamp3 = 1000;
  packet_.timestamp = kSsTimestamp1;
  EXPECT_TRUE(map_.Insert(packet_));
  packet_.timestamp = kSsTimestamp2;
  EXPECT_TRUE(map_.Insert(packet_));
  packet_.timestamp = kSsTimestamp3;
  EXPECT_TRUE(map_.Insert(packet_));

  map_.RemoveOld(kSsTimestamp3);
  EXPECT_FALSE(map_.Find(kSsTimestamp1, &it));
  EXPECT_FALSE(map_.Find(kSsTimestamp2, &it));
  EXPECT_TRUE(map_.Find(kSsTimestamp3, &it));
}

TEST_F(Vp9SsMapTest, UpdatePacket_NoSsData) {
  packet_.video_header.codecHeader.VP9.gof_idx = 0;
  EXPECT_FALSE(map_.UpdatePacket(&packet_));
}

TEST_F(Vp9SsMapTest, UpdatePacket_NoGofIdx) {
  EXPECT_TRUE(map_.Insert(packet_));
  packet_.video_header.codecHeader.VP9.gof_idx = kNoGofIdx;
  EXPECT_FALSE(map_.UpdatePacket(&packet_));
}

TEST_F(Vp9SsMapTest, UpdatePacket_InvalidGofIdx) {
  EXPECT_TRUE(map_.Insert(packet_));
  packet_.video_header.codecHeader.VP9.gof_idx = 4;
  EXPECT_FALSE(map_.UpdatePacket(&packet_));
}

TEST_F(Vp9SsMapTest, UpdatePacket) {
  EXPECT_TRUE(map_.Insert(packet_));  // kTemporalStructureMode3: 0-2-1-2..

  packet_.video_header.codecHeader.VP9.gof_idx = 0;
  EXPECT_TRUE(map_.UpdatePacket(&packet_));
  EXPECT_EQ(0, packet_.video_header.codecHeader.VP9.temporal_idx);
  EXPECT_FALSE(packet_.video_header.codecHeader.VP9.temporal_up_switch);
  EXPECT_EQ(1U, packet_.video_header.codecHeader.VP9.num_ref_pics);
  EXPECT_EQ(4, packet_.video_header.codecHeader.VP9.pid_diff[0]);

  packet_.video_header.codecHeader.VP9.gof_idx = 1;
  EXPECT_TRUE(map_.UpdatePacket(&packet_));
  EXPECT_EQ(2, packet_.video_header.codecHeader.VP9.temporal_idx);
  EXPECT_TRUE(packet_.video_header.codecHeader.VP9.temporal_up_switch);
  EXPECT_EQ(1U, packet_.video_header.codecHeader.VP9.num_ref_pics);
  EXPECT_EQ(1, packet_.video_header.codecHeader.VP9.pid_diff[0]);

  packet_.video_header.codecHeader.VP9.gof_idx = 2;
  EXPECT_TRUE(map_.UpdatePacket(&packet_));
  EXPECT_EQ(1, packet_.video_header.codecHeader.VP9.temporal_idx);
  EXPECT_TRUE(packet_.video_header.codecHeader.VP9.temporal_up_switch);
  EXPECT_EQ(1U, packet_.video_header.codecHeader.VP9.num_ref_pics);
  EXPECT_EQ(2, packet_.video_header.codecHeader.VP9.pid_diff[0]);

  packet_.video_header.codecHeader.VP9.gof_idx = 3;
  EXPECT_TRUE(map_.UpdatePacket(&packet_));
  EXPECT_EQ(2, packet_.video_header.codecHeader.VP9.temporal_idx);
  EXPECT_FALSE(packet_.video_header.codecHeader.VP9.temporal_up_switch);
  EXPECT_EQ(2U, packet_.video_header.codecHeader.VP9.num_ref_pics);
  EXPECT_EQ(1, packet_.video_header.codecHeader.VP9.pid_diff[0]);
  EXPECT_EQ(2, packet_.video_header.codecHeader.VP9.pid_diff[1]);
}

class TestBasicJitterBuffer : public ::testing::TestWithParam<std::string>,
                              public NackSender,
                              public KeyFrameRequestSender {
 public:
  void SendNack(const std::vector<uint16_t>& sequence_numbers) override {
    nack_sent_.insert(nack_sent_.end(), sequence_numbers.begin(),
                      sequence_numbers.end());
  }

  void RequestKeyFrame() override { ++keyframe_requests_; }

  std::vector<uint16_t> nack_sent_;
  int keyframe_requests_;

 protected:
  TestBasicJitterBuffer() {}
  void SetUp() override {
    clock_.reset(new SimulatedClock(0));
    jitter_buffer_.reset(new VCMJitterBuffer(
        clock_.get(),
        std::unique_ptr<EventWrapper>(event_factory_.CreateEvent()),
        this,
        this));
    jitter_buffer_->Start();
    seq_num_ = 1234;
    timestamp_ = 0;
    size_ = 1400;
    // Data vector -  0, 0, 0x80, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0x80, 3....
    data_[0] = 0;
    data_[1] = 0;
    data_[2] = 0x80;
    int count = 3;
    for (unsigned int i = 3; i < sizeof(data_) - 3; ++i) {
      data_[i] = count;
      count++;
      if (count == 10) {
        data_[i + 1] = 0;
        data_[i + 2] = 0;
        data_[i + 3] = 0x80;
        count = 3;
        i += 3;
      }
    }
    WebRtcRTPHeader rtpHeader;
    memset(&rtpHeader, 0, sizeof(rtpHeader));
    rtpHeader.header.sequenceNumber = seq_num_;
    rtpHeader.header.timestamp = timestamp_;
    rtpHeader.header.markerBit = true;
    rtpHeader.frameType = kVideoFrameDelta;
    packet_.reset(new VCMPacket(data_, size_, rtpHeader));
  }

  VCMEncodedFrame* DecodeCompleteFrame() {
    VCMEncodedFrame* found_frame = jitter_buffer_->NextCompleteFrame(10);
    if (!found_frame)
      return nullptr;
    return jitter_buffer_->ExtractAndSetDecode(found_frame->TimeStamp());
  }

  VCMEncodedFrame* DecodeIncompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp);
    if (!found_frame)
      return NULL;
    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    return frame;
  }

  void CheckOutFrame(VCMEncodedFrame* frame_out,
                     unsigned int size,
                     bool startCode) {
    ASSERT_TRUE(frame_out);

    const uint8_t* outData = frame_out->Buffer();
    unsigned int i = 0;

    if (startCode) {
      EXPECT_EQ(0, outData[0]);
      EXPECT_EQ(0, outData[1]);
      EXPECT_EQ(0, outData[2]);
      EXPECT_EQ(1, outData[3]);
      i += 4;
    }

    EXPECT_EQ(size, frame_out->Length());
    int count = 3;
    for (; i < size; i++) {
      if (outData[i] == 0 && outData[i + 1] == 0 && outData[i + 2] == 0x80) {
        i += 2;
      } else if (startCode && outData[i] == 0 && outData[i + 1] == 0) {
        EXPECT_EQ(0, outData[0]);
        EXPECT_EQ(0, outData[1]);
        EXPECT_EQ(0, outData[2]);
        EXPECT_EQ(1, outData[3]);
        i += 3;
      } else {
        EXPECT_EQ(count, outData[i]);
        count++;
        if (count == 10) {
          count = 3;
        }
      }
    }
  }

  uint16_t seq_num_;
  uint32_t timestamp_;
  int size_;
  uint8_t data_[1500];
  std::unique_ptr<VCMPacket> packet_;
  std::unique_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
  std::unique_ptr<VCMJitterBuffer> jitter_buffer_;
};

class TestRunningJitterBuffer : public ::testing::TestWithParam<std::string>,
                                public NackSender,
                                public KeyFrameRequestSender {
 public:
  void SendNack(const std::vector<uint16_t>& sequence_numbers) {
    nack_sent_.insert(nack_sent_.end(), sequence_numbers.begin(),
                      sequence_numbers.end());
  }

  void RequestKeyFrame() { ++keyframe_requests_; }

  std::vector<uint16_t> nack_sent_;
  int keyframe_requests_;

 protected:
  enum { kDataBufferSize = 10 };

  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    max_nack_list_size_ = 150;
    oldest_packet_to_nack_ = 250;
    jitter_buffer_ = new VCMJitterBuffer(
        clock_.get(),
        std::unique_ptr<EventWrapper>(event_factory_.CreateEvent()),
        this, this);
    stream_generator_ = new StreamGenerator(0, clock_->TimeInMilliseconds());
    jitter_buffer_->Start();
    jitter_buffer_->SetNackSettings(max_nack_list_size_, oldest_packet_to_nack_,
                                    0);
    memset(data_buffer_, 0, kDataBufferSize);
  }

  virtual void TearDown() {
    jitter_buffer_->Stop();
    delete stream_generator_;
    delete jitter_buffer_;
  }

  VCMFrameBufferEnum InsertPacketAndPop(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->PopPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    bool retransmitted = false;
    return jitter_buffer_->InsertPacket(packet, &retransmitted);
  }

  VCMFrameBufferEnum InsertPacket(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->GetPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    bool retransmitted = false;
    return jitter_buffer_->InsertPacket(packet, &retransmitted);
  }

  VCMFrameBufferEnum InsertFrame(FrameType frame_type) {
    stream_generator_->GenerateFrame(
        frame_type, (frame_type != kEmptyFrame) ? 1 : 0,
        (frame_type == kEmptyFrame) ? 1 : 0, clock_->TimeInMilliseconds());
    VCMFrameBufferEnum ret = InsertPacketAndPop(0);
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
    return ret;
  }

  VCMFrameBufferEnum InsertFrames(int num_frames, FrameType frame_type) {
    VCMFrameBufferEnum ret_for_all = kNoError;
    for (int i = 0; i < num_frames; ++i) {
      VCMFrameBufferEnum ret = InsertFrame(frame_type);
      if (ret < kNoError) {
        ret_for_all = ret;
      } else if (ret_for_all >= kNoError) {
        ret_for_all = ret;
      }
    }
    return ret_for_all;
  }

  void DropFrame(int num_packets) {
    stream_generator_->GenerateFrame(kVideoFrameDelta, num_packets, 0,
                                     clock_->TimeInMilliseconds());
    for (int i = 0; i < num_packets; ++i)
      stream_generator_->DropLastPacket();
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }

  bool DecodeCompleteFrame() {
    VCMEncodedFrame* found_frame = jitter_buffer_->NextCompleteFrame(0);
    if (!found_frame)
      return false;

    VCMEncodedFrame* frame =
        jitter_buffer_->ExtractAndSetDecode(found_frame->TimeStamp());
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  bool DecodeIncompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp);
    if (!found_frame)
      return false;
    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  VCMJitterBuffer* jitter_buffer_;
  StreamGenerator* stream_generator_;
  std::unique_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
  size_t max_nack_list_size_;
  int oldest_packet_to_nack_;
  uint8_t data_buffer_[kDataBufferSize];
};

class TestJitterBufferNack : public TestRunningJitterBuffer {
 protected:
  TestJitterBufferNack() {}
  virtual void SetUp() {
    TestRunningJitterBuffer::SetUp();
    jitter_buffer_->SetNackMode(kNack, -1, -1);
  }

  virtual void TearDown() { TestRunningJitterBuffer::TearDown(); }
};

TEST_F(TestBasicJitterBuffer, StopRunning) {
  jitter_buffer_->Stop();
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());
  jitter_buffer_->Start();
  // Allow selective errors.
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);

  // No packets inserted.
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());

  // Allow decoding with errors.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);

  // No packets inserted.
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());
}

TEST_F(TestBasicJitterBuffer, SinglePacketFrame) {
  // Always start with a complete key frame when not allowing errors.
  jitter_buffer_->SetDecodeErrorMode(kNoErrors);
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->timestamp += 123 * 90;

  // Insert the packet to the jitter buffer and get a frame.
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, VerifyHistogramStats) {
  metrics::Reset();
  // Always start with a complete key frame when not allowing errors.
  jitter_buffer_->SetDecodeErrorMode(kNoErrors);
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->timestamp += 123 * 90;

  // Insert single packet frame to the jitter buffer and get a frame.
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  // Verify that histograms are updated when the jitter buffer is stopped.
  clock_->AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  jitter_buffer_->Stop();
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.DiscardedPacketsInPercent", 0));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.DuplicatedPacketsInPercent", 0));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.CompleteFramesReceivedPerSecond"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.KeyFramesReceivedInPermille", 1000));

  // Verify that histograms are not updated if stop is called again.
  jitter_buffer_->Stop();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.DiscardedPacketsInPercent"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.DuplicatedPacketsInPercent"));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.CompleteFramesReceivedPerSecond"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.KeyFramesReceivedInPermille"));
}

TEST_F(TestBasicJitterBuffer, DualPacketFrame) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  // Should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  ++seq_num_;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, 100PacketKeyFrame) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 frames.
  int loop = 0;
  do {
    seq_num_++;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert last packet.
  ++seq_num_;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, 100PacketDeltaFrame) {
  // Always start with a complete key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_FALSE(frame_out == NULL);
  jitter_buffer_->ReleaseFrame(frame_out);

  ++seq_num_;
  packet_->seqNum = seq_num_;
  packet_->markerBit = false;
  packet_->frameType = kVideoFrameDelta;
  packet_->timestamp += 33 * 90;

  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  packet_->is_first_packet_in_frame = false;
  // Insert 98 frames.
  int loop = 0;
  do {
    ++seq_num_;
    packet_->seqNum = seq_num_;

    // Insert a packet into a frame.
    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert the last packet.
  ++seq_num_;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, PacketReorderingReverseOrder) {
  // Insert the "first" packet last.
  seq_num_ += 100;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 packets.
  int loop = 0;
  do {
    seq_num_--;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert the last packet.
  seq_num_--;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, FrameReordering2Frames2PacketsEach) {
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  // check that we fail to get frame since seqnum is not continuous
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  seq_num_ -= 3;
  timestamp_ -= 33 * 90;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  // It should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, TestReorderingWithPadding) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;

  // Send in an initial good packet/frame (Frame A) to start things off.
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out != NULL);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Now send in a complete delta frame (Frame C), but with a sequence number
  // gap. No pic index either, so no temporal scalability cheating :)
  packet_->frameType = kVideoFrameDelta;
  // Leave a gap of 2 sequence numbers and two frames.
  packet_->seqNum = seq_num_ + 3;
  packet_->timestamp = timestamp_ + (66 * 90);
  // Still isFirst = marker = true.
  // Session should be complete (frame is complete), but there's nothing to
  // decode yet.
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  // Now send in a complete delta frame (Frame B) that is continuous from A, but
  // doesn't fill the full gap to C. The rest of the gap is going to be padding.
  packet_->seqNum = seq_num_ + 1;
  packet_->timestamp = timestamp_ + (33 * 90);
  // Still isFirst = marker = true.
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out != NULL);
  jitter_buffer_->ReleaseFrame(frame_out);

  // But Frame C isn't continuous yet.
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  // Add in the padding. These are empty packets (data length is 0) with no
  // marker bit and matching the timestamp of Frame B.
  WebRtcRTPHeader rtpHeader;
  memset(&rtpHeader, 0, sizeof(rtpHeader));
  rtpHeader.header.sequenceNumber = seq_num_ + 2;
  rtpHeader.header.timestamp = timestamp_ + (33 * 90);
  rtpHeader.header.markerBit = false;
  VCMPacket empty_packet(data_, 0, rtpHeader);
  EXPECT_EQ(kOldPacket,
            jitter_buffer_->InsertPacket(empty_packet, &retransmitted));
  empty_packet.seqNum += 1;
  EXPECT_EQ(kOldPacket,
            jitter_buffer_->InsertPacket(empty_packet, &retransmitted));

  // But now Frame C should be ready!
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out != NULL);
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, DuplicatePackets) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(0, jitter_buffer_->num_packets());
  EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);
  EXPECT_EQ(1, jitter_buffer_->num_packets());
  EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());

  // Insert a packet into a frame.
  EXPECT_EQ(kDuplicatePacket,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(2, jitter_buffer_->num_packets());
  EXPECT_EQ(1, jitter_buffer_->num_duplicated_packets());

  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->markerBit = true;
  packet_->is_first_packet_in_frame = false;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  ASSERT_TRUE(frame_out != NULL);
  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  EXPECT_EQ(3, jitter_buffer_->num_packets());
  EXPECT_EQ(1, jitter_buffer_->num_duplicated_packets());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, DuplicatePreviousDeltaFramePacket) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  jitter_buffer_->SetDecodeErrorMode(kNoErrors);
  EXPECT_EQ(0, jitter_buffer_->num_packets());
  EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());

  bool retransmitted = false;
  // Insert first complete frame.
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  ASSERT_TRUE(frame_out != NULL);
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  // Insert 3 delta frames.
  for (uint16_t i = 1; i <= 3; ++i) {
    packet_->seqNum = seq_num_ + i;
    packet_->timestamp = timestamp_ + (i * 33) * 90;
    packet_->frameType = kVideoFrameDelta;
    EXPECT_EQ(kCompleteSession,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    EXPECT_EQ(i + 1, jitter_buffer_->num_packets());
    EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());
  }

  // Retransmit second delta frame.
  packet_->seqNum = seq_num_ + 2;
  packet_->timestamp = timestamp_ + 66 * 90;

  EXPECT_EQ(kDuplicatePacket,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  EXPECT_EQ(5, jitter_buffer_->num_packets());
  EXPECT_EQ(1, jitter_buffer_->num_duplicated_packets());

  // Should be able to decode 3 delta frames, key frame already decoded.
  for (size_t i = 0; i < 3; ++i) {
    frame_out = DecodeCompleteFrame();
    ASSERT_TRUE(frame_out != NULL);
    CheckOutFrame(frame_out, size_, false);
    EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
    jitter_buffer_->ReleaseFrame(frame_out);
  }
}

TEST_F(TestBasicJitterBuffer, TestSkipForwardVp9) {
  // Verify that JB skips forward to next base layer frame.
  //  -------------------------------------------------
  // | 65485 | 65486 | 65487 | 65488 | 65489 | ...
  // | pid:5 | pid:6 | pid:7 | pid:8 | pid:9 | ...
  // | tid:0 | tid:2 | tid:1 | tid:2 | tid:0 | ...
  // |  ss   |   x   |   x   |   x   |       |
  //  -------------------------------------------------
  // |<----------tl0idx:200--------->|<---tl0idx:201---

  bool re = false;
  packet_->codec = kVideoCodecVP9;
  packet_->video_header.codec = kRtpVideoVp9;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->video_header.codecHeader.VP9.flexible_mode = false;
  packet_->video_header.codecHeader.VP9.spatial_idx = 0;
  packet_->video_header.codecHeader.VP9.beginning_of_frame = true;
  packet_->video_header.codecHeader.VP9.end_of_frame = true;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = false;

  packet_->seqNum = 65485;
  packet_->timestamp = 1000;
  packet_->frameType = kVideoFrameKey;
  packet_->video_header.codecHeader.VP9.picture_id = 5;
  packet_->video_header.codecHeader.VP9.tl0_pic_idx = 200;
  packet_->video_header.codecHeader.VP9.temporal_idx = 0;
  packet_->video_header.codecHeader.VP9.ss_data_available = true;
  packet_->video_header.codecHeader.VP9.gof.SetGofInfoVP9(
      kTemporalStructureMode3);  // kTemporalStructureMode3: 0-2-1-2..
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  // Insert next temporal layer 0.
  packet_->seqNum = 65489;
  packet_->timestamp = 13000;
  packet_->frameType = kVideoFrameDelta;
  packet_->video_header.codecHeader.VP9.picture_id = 9;
  packet_->video_header.codecHeader.VP9.tl0_pic_idx = 201;
  packet_->video_header.codecHeader.VP9.temporal_idx = 0;
  packet_->video_header.codecHeader.VP9.ss_data_available = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(1000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();
  EXPECT_EQ(13000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, ReorderedVp9SsData_3TlLayers) {
  // Verify that frames are updated with SS data when SS packet is reordered.
  //  --------------------------------
  // | 65486 | 65487 | 65485 |...
  // | pid:6 | pid:7 | pid:5 |...
  // | tid:2 | tid:1 | tid:0 |...
  // |       |       |  ss   |
  //  --------------------------------
  // |<--------tl0idx:200--------->|

  bool re = false;
  packet_->codec = kVideoCodecVP9;
  packet_->video_header.codec = kRtpVideoVp9;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->video_header.codecHeader.VP9.flexible_mode = false;
  packet_->video_header.codecHeader.VP9.spatial_idx = 0;
  packet_->video_header.codecHeader.VP9.beginning_of_frame = true;
  packet_->video_header.codecHeader.VP9.end_of_frame = true;
  packet_->video_header.codecHeader.VP9.tl0_pic_idx = 200;

  packet_->seqNum = 65486;
  packet_->timestamp = 6000;
  packet_->frameType = kVideoFrameDelta;
  packet_->video_header.codecHeader.VP9.picture_id = 6;
  packet_->video_header.codecHeader.VP9.temporal_idx = 2;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = true;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  packet_->seqNum = 65487;
  packet_->timestamp = 9000;
  packet_->frameType = kVideoFrameDelta;
  packet_->video_header.codecHeader.VP9.picture_id = 7;
  packet_->video_header.codecHeader.VP9.temporal_idx = 1;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = true;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  // Insert first frame with SS data.
  packet_->seqNum = 65485;
  packet_->timestamp = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->width = 352;
  packet_->height = 288;
  packet_->video_header.codecHeader.VP9.picture_id = 5;
  packet_->video_header.codecHeader.VP9.temporal_idx = 0;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = false;
  packet_->video_header.codecHeader.VP9.ss_data_available = true;
  packet_->video_header.codecHeader.VP9.gof.SetGofInfoVP9(
      kTemporalStructureMode3);  // kTemporalStructureMode3: 0-2-1-2..
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(3000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  EXPECT_EQ(0, frame_out->CodecSpecific()->codecSpecific.VP9.temporal_idx);
  EXPECT_FALSE(
      frame_out->CodecSpecific()->codecSpecific.VP9.temporal_up_switch);
  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();
  EXPECT_EQ(6000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  EXPECT_EQ(2, frame_out->CodecSpecific()->codecSpecific.VP9.temporal_idx);
  EXPECT_TRUE(frame_out->CodecSpecific()->codecSpecific.VP9.temporal_up_switch);
  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();
  EXPECT_EQ(9000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  EXPECT_EQ(1, frame_out->CodecSpecific()->codecSpecific.VP9.temporal_idx);
  EXPECT_TRUE(frame_out->CodecSpecific()->codecSpecific.VP9.temporal_up_switch);
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, ReorderedVp9SsData_2Tl2SLayers) {
  // Verify that frames are updated with SS data when SS packet is reordered.
  //  -----------------------------------------
  // | 65486  | 65487  | 65485  | 65484  |...
  // | pid:6  | pid:6  | pid:5  | pid:5  |...
  // | tid:1  | tid:1  | tid:0  | tid:0  |...
  // | sid:0  | sid:1  | sid:1  | sid:0  |...
  // | t:6000 | t:6000 | t:3000 | t:3000 |
  // |        |        |        |  ss    |
  //  -----------------------------------------
  // |<-----------tl0idx:200------------>|

  bool re = false;
  packet_->codec = kVideoCodecVP9;
  packet_->video_header.codec = kRtpVideoVp9;
  packet_->video_header.codecHeader.VP9.flexible_mode = false;
  packet_->video_header.codecHeader.VP9.beginning_of_frame = true;
  packet_->video_header.codecHeader.VP9.end_of_frame = true;
  packet_->video_header.codecHeader.VP9.tl0_pic_idx = 200;

  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = 65486;
  packet_->timestamp = 6000;
  packet_->frameType = kVideoFrameDelta;
  packet_->video_header.codecHeader.VP9.spatial_idx = 0;
  packet_->video_header.codecHeader.VP9.picture_id = 6;
  packet_->video_header.codecHeader.VP9.temporal_idx = 1;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = true;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_, &re));

  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = 65487;
  packet_->frameType = kVideoFrameDelta;
  packet_->video_header.codecHeader.VP9.spatial_idx = 1;
  packet_->video_header.codecHeader.VP9.picture_id = 6;
  packet_->video_header.codecHeader.VP9.temporal_idx = 1;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = true;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = 65485;
  packet_->timestamp = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->video_header.codecHeader.VP9.spatial_idx = 1;
  packet_->video_header.codecHeader.VP9.picture_id = 5;
  packet_->video_header.codecHeader.VP9.temporal_idx = 0;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_, &re));

  // Insert first frame with SS data.
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = 65484;
  packet_->frameType = kVideoFrameKey;
  packet_->width = 352;
  packet_->height = 288;
  packet_->video_header.codecHeader.VP9.spatial_idx = 0;
  packet_->video_header.codecHeader.VP9.picture_id = 5;
  packet_->video_header.codecHeader.VP9.temporal_idx = 0;
  packet_->video_header.codecHeader.VP9.temporal_up_switch = false;
  packet_->video_header.codecHeader.VP9.ss_data_available = true;
  packet_->video_header.codecHeader.VP9.gof.SetGofInfoVP9(
      kTemporalStructureMode2);  // kTemporalStructureMode3: 0-1-0-1..
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_, &re));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(3000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  EXPECT_EQ(0, frame_out->CodecSpecific()->codecSpecific.VP9.temporal_idx);
  EXPECT_FALSE(
      frame_out->CodecSpecific()->codecSpecific.VP9.temporal_up_switch);
  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();
  EXPECT_EQ(6000U, frame_out->TimeStamp());
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  EXPECT_EQ(1, frame_out->CodecSpecific()->codecSpecific.VP9.temporal_idx);
  EXPECT_TRUE(frame_out->CodecSpecific()->codecSpecific.VP9.temporal_up_switch);
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, H264InsertStartCode) {
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->insertStartCode = true;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_ * 2 + 4 * 2, true);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, SpsAndPpsHandling) {
  jitter_buffer_->SetDecodeErrorMode(kNoErrors);

  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->codec = kVideoCodecH264;
  packet_->video_header.codec = kRtpVideoH264;
  packet_->video_header.codecHeader.H264.nalu_type = H264::NaluType::kIdr;
  packet_->video_header.codecHeader.H264.nalus[0].type = H264::NaluType::kIdr;
  packet_->video_header.codecHeader.H264.nalus[0].sps_id = -1;
  packet_->video_header.codecHeader.H264.nalus[0].pps_id = 0;
  packet_->video_header.codecHeader.H264.nalus_length = 1;
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  // Not decodable since sps and pps are missing.
  EXPECT_EQ(nullptr, DecodeCompleteFrame());

  timestamp_ += 3000;
  packet_->timestamp = timestamp_;
  ++seq_num_;
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->codec = kVideoCodecH264;
  packet_->video_header.codec = kRtpVideoH264;
  packet_->video_header.codecHeader.H264.nalu_type = H264::NaluType::kStapA;
  packet_->video_header.codecHeader.H264.nalus[0].type = H264::NaluType::kSps;
  packet_->video_header.codecHeader.H264.nalus[0].sps_id = 0;
  packet_->video_header.codecHeader.H264.nalus[0].pps_id = -1;
  packet_->video_header.codecHeader.H264.nalus[1].type = H264::NaluType::kPps;
  packet_->video_header.codecHeader.H264.nalus[1].sps_id = 0;
  packet_->video_header.codecHeader.H264.nalus[1].pps_id = 0;
  packet_->video_header.codecHeader.H264.nalus_length = 2;
  // Not complete since the marker bit hasn't been received.
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  ++seq_num_;
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->codec = kVideoCodecH264;
  packet_->video_header.codec = kRtpVideoH264;
  packet_->video_header.codecHeader.H264.nalu_type = H264::NaluType::kIdr;
  packet_->video_header.codecHeader.H264.nalus[0].type = H264::NaluType::kIdr;
  packet_->video_header.codecHeader.H264.nalus[0].sps_id = -1;
  packet_->video_header.codecHeader.H264.nalus[0].pps_id = 0;
  packet_->video_header.codecHeader.H264.nalus_length = 1;
  // Complete and decodable since the pps and sps are received in the first
  // packet of this frame.
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  ASSERT_NE(nullptr, frame_out);
  jitter_buffer_->ReleaseFrame(frame_out);

  timestamp_ += 3000;
  packet_->timestamp = timestamp_;
  ++seq_num_;
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->codec = kVideoCodecH264;
  packet_->video_header.codec = kRtpVideoH264;
  packet_->video_header.codecHeader.H264.nalu_type = H264::NaluType::kSlice;
  packet_->video_header.codecHeader.H264.nalus[0].type = H264::NaluType::kSlice;
  packet_->video_header.codecHeader.H264.nalus[0].sps_id = -1;
  packet_->video_header.codecHeader.H264.nalus[0].pps_id = 0;
  packet_->video_header.codecHeader.H264.nalus_length = 1;
  // Complete and decodable since sps, pps and key frame has been received.
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  frame_out = DecodeCompleteFrame();
  ASSERT_NE(nullptr, frame_out);
  jitter_buffer_->ReleaseFrame(frame_out);
}

// Test threshold conditions of decodable state.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsThresholdCheck) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame. Use 10 packets to test Decodable State
  // boundaries.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  uint32_t timestamp = 0;
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  packet_->is_first_packet_in_frame = false;
  for (int i = 1; i < 9; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // last packet
  packet_->markerBit = true;
  packet_->seqNum++;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 10 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33 * 90 * 8;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame
  packet_->seqNum -= 99;
  packet_->timestamp -= 33 * 90 * 7;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  packet_->is_first_packet_in_frame = false;
  for (int i = 1; i < 8; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kDecodableSession,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
    EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  packet_->seqNum++;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  frame_out = DecodeIncompleteFrame();
  ASSERT_FALSE(NULL == frame_out);
  CheckOutFrame(frame_out, 9 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  packet_->markerBit = true;
  packet_->seqNum++;
  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));
}

// Make sure first packet is present before a frame can be decoded.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsIncompleteKey) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33 * 90 * 8;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  uint32_t timestamp;
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame - an incomplete key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->seqNum -= 99;
  packet_->timestamp -= 33 * 90 * 7;

  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert a few more packets. Make sure we're waiting for the key frame to be
  // complete.
  packet_->is_first_packet_in_frame = false;
  for (int i = 1; i < 5; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // Complete key frame.
  packet_->markerBit = true;
  packet_->seqNum++;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 6 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

// Make sure first packet is present before a frame can be decoded.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsMissingFirstPacket) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33 * 90 * 8;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  uint32_t timestamp;
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame with the first packet missing. Make sure we're waiting
  // for the key frame to be complete.
  packet_->seqNum -= 98;
  packet_->timestamp -= 33 * 90 * 7;

  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  for (int i = 0; i < 5; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // Add first packet. Frame should now be decodable, but incomplete.
  packet_->is_first_packet_in_frame = true;
  packet_->seqNum -= 6;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  frame_out = DecodeIncompleteFrame();
  CheckOutFrame(frame_out, 7 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, DiscontinuousStreamWhenDecodingWithErrors) {
  // Will use one packet per frame.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  uint32_t next_timestamp;
  VCMEncodedFrame* frame = jitter_buffer_->NextCompleteFrame(0);
  EXPECT_NE(frame, nullptr);
  EXPECT_EQ(packet_->timestamp, frame->TimeStamp());
  frame = jitter_buffer_->ExtractAndSetDecode(frame->TimeStamp());
  EXPECT_TRUE(frame != NULL);
  jitter_buffer_->ReleaseFrame(frame);

  // Drop a complete frame.
  timestamp_ += 2 * 33 * 90;
  seq_num_ += 2;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  // Insert a packet (so the previous one will be released).
  timestamp_ += 33 * 90;
  seq_num_ += 2;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_EQ(jitter_buffer_->NextCompleteFrame(0), nullptr);
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&next_timestamp));
  EXPECT_EQ(packet_->timestamp - 33 * 90, next_timestamp);
}

TEST_F(TestBasicJitterBuffer, PacketLoss) {
  // Verify missing packets statistics and not decodable packets statistics.
  // Insert 10 frames consisting of 4 packets and remove one from all of them.
  // The last packet is an empty (non-media) packet.

  // Select a start seqNum which triggers a difficult wrap situation
  // The JB will only output (incomplete)frames if the next one has started
  // to arrive. Start by inserting one frame (key).
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  seq_num_ = 0xffff - 4;
  seq_num_++;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->completeNALU = kNaluStart;

  bool retransmitted = false;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  for (int i = 0; i < 11; ++i) {
    webrtc::FrameType frametype = kVideoFrameDelta;
    seq_num_++;
    timestamp_ += 33 * 90;
    packet_->frameType = frametype;
    packet_->is_first_packet_in_frame = true;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;
    packet_->completeNALU = kNaluStart;

    EXPECT_EQ(kDecodableSession,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));

    VCMEncodedFrame* frame_out = DecodeCompleteFrame();

    // Should not be complete.
    EXPECT_TRUE(frame_out == NULL);

    seq_num_ += 2;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = true;
    packet_->seqNum = seq_num_;
    packet_->completeNALU = kNaluEnd;

    EXPECT_EQ(jitter_buffer_->InsertPacket(*packet_, &retransmitted),
              kDecodableSession);

    // Insert an empty (non-media) packet.
    seq_num_++;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->completeNALU = kNaluEnd;
    packet_->frameType = kEmptyFrame;

    EXPECT_EQ(jitter_buffer_->InsertPacket(*packet_, &retransmitted),
              kDecodableSession);
    frame_out = DecodeIncompleteFrame();

    // One of the packets has been discarded by the jitter buffer.
    // Last frame can't be extracted yet.
    if (i < 10) {
      CheckOutFrame(frame_out, size_, false);

      if (i == 0) {
        EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
      } else {
        EXPECT_EQ(frametype, frame_out->FrameType());
      }
      EXPECT_FALSE(frame_out->Complete());
      EXPECT_FALSE(frame_out->MissingFrame());
    }

    jitter_buffer_->ReleaseFrame(frame_out);
  }

  // Insert 3 old packets and verify that we have 3 discarded packets
  // Match value to actual latest timestamp decoded.
  timestamp_ -= 33 * 90;
  packet_->timestamp = timestamp_ - 1000;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  packet_->timestamp = timestamp_ - 500;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  packet_->timestamp = timestamp_ - 100;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  EXPECT_EQ(3, jitter_buffer_->num_discarded_packets());

  jitter_buffer_->Flush();

  // This statistic shouldn't be reset by a flush.
  EXPECT_EQ(3, jitter_buffer_->num_discarded_packets());
}

TEST_F(TestBasicJitterBuffer, DeltaFrame100PacketsWithSeqNumWrap) {
  seq_num_ = 0xfff0;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  int loop = 0;
  do {
    seq_num_++;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));

    frame_out = DecodeCompleteFrame();

    EXPECT_TRUE(frame_out == NULL);

    loop++;
  } while (loop < 98);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, PacketReorderingReverseWithNegSeqNumWrap) {
  // Insert "first" packet last seqnum.
  seq_num_ = 10;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 frames.
  int loop = 0;
  do {
    seq_num_--;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));

    frame_out = DecodeCompleteFrame();

    EXPECT_TRUE(frame_out == NULL);

    loop++;
  } while (loop < 98);

  // Insert last packet.
  seq_num_--;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, TestInsertOldFrame) {
  //   -------      -------
  //  |   2   |    |   1   |
  //   -------      -------
  //  t = 3000     t = 2000
  seq_num_ = 2;
  timestamp_ = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->timestamp = timestamp_;
  packet_->seqNum = seq_num_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(3000u, frame_out->TimeStamp());
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_--;
  timestamp_ = 2000;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));
}

TEST_F(TestBasicJitterBuffer, TestInsertOldFrameWithSeqNumWrap) {
  //   -------      -------
  //  |   2   |    |   1   |
  //   -------      -------
  //  t = 3000     t = 0xffffff00

  seq_num_ = 2;
  timestamp_ = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(timestamp_, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_--;
  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  // This timestamp is old.
  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_, &retransmitted));
}

TEST_F(TestBasicJitterBuffer, TimestampWrap) {
  //  ---------------     ---------------
  // |   1   |   2   |   |   3   |   4   |
  //  ---------------     ---------------
  //  t = 0xffffff00        t = 33*90

  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);
  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_++;
  timestamp_ += 33 * 90;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kIncomplete,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, 2FrameWithTimestampWrap) {
  //   -------          -------
  //  |   1   |        |   2   |
  //   -------          -------
  // t = 0xffffff00    t = 2700

  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  // Insert first frame (session will be complete).
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  // Insert next frame.
  seq_num_++;
  timestamp_ = 2700;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(0xffffff00, frame_out->TimeStamp());
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  VCMEncodedFrame* frame_out2 = DecodeCompleteFrame();
  EXPECT_EQ(2700u, frame_out2->TimeStamp());
  CheckOutFrame(frame_out2, size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out2->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out2);
}

TEST_F(TestBasicJitterBuffer, Insert2FramesReOrderedWithTimestampWrap) {
  //   -------          -------
  //  |   2   |        |   1   |
  //   -------          -------
  //  t = 2700        t = 0xffffff00

  seq_num_ = 2;
  timestamp_ = 2700;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  // Insert second frame
  seq_num_--;
  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(0xffffff00, frame_out->TimeStamp());
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);

  VCMEncodedFrame* frame_out2 = DecodeCompleteFrame();
  EXPECT_EQ(2700u, frame_out2->TimeStamp());
  CheckOutFrame(frame_out2, size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out2->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out2);
}

TEST_F(TestBasicJitterBuffer, DeltaFrameWithMoreThanMaxNumberOfPackets) {
  int loop = 0;
  bool firstPacket = true;
  bool retransmitted = false;
  // Insert kMaxPacketsInJitterBuffer into frame.
  do {
    seq_num_++;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    if (firstPacket) {
      EXPECT_EQ(kIncomplete,
                jitter_buffer_->InsertPacket(*packet_, &retransmitted));
      firstPacket = false;
    } else {
      EXPECT_EQ(kIncomplete,
                jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    }

    loop++;
  } while (loop < kMaxPacketsInSession);

  // Max number of packets inserted.
  // Insert one more packet.
  seq_num_++;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  // Insert the packet -> frame recycled.
  EXPECT_EQ(kSizeError, jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
}

TEST_F(TestBasicJitterBuffer, ExceedNumOfFrameWithSeqNumWrap) {
  // TEST fill JB with more than max number of frame (50 delta frames +
  // 51 key frames) with wrap in seq_num_
  //
  //  --------------------------------------------------------------
  // | 65485 | 65486 | 65487 | .... | 65535 | 0 | 1 | 2 | .....| 50 |
  //  --------------------------------------------------------------
  // |<-----------delta frames------------->|<------key frames----->|

  // Make sure the jitter doesn't request a keyframe after too much non-
  // decodable frames.
  jitter_buffer_->SetNackMode(kNack, -1, -1);
  jitter_buffer_->SetNackSettings(kMaxNumberOfFrames, kMaxNumberOfFrames, 0);

  int loop = 0;
  seq_num_ = 65485;
  uint32_t first_key_frame_timestamp = 0;
  bool retransmitted = false;
  // Insert MAX_NUMBER_OF_FRAMES frames.
  do {
    timestamp_ += 33 * 90;
    seq_num_++;
    packet_->is_first_packet_in_frame = true;
    packet_->markerBit = true;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;

    if (loop == 50) {
      first_key_frame_timestamp = packet_->timestamp;
      packet_->frameType = kVideoFrameKey;
    }

    // Insert frame.
    EXPECT_EQ(kCompleteSession,
              jitter_buffer_->InsertPacket(*packet_, &retransmitted));

    loop++;
  } while (loop < kMaxNumberOfFrames);

  // Max number of frames inserted.

  // Insert one more frame.
  timestamp_ += 33 * 90;
  seq_num_++;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  // Now, no free frame - frames will be recycled until first key frame.
  EXPECT_EQ(kFlushIndicator,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(first_key_frame_timestamp, frame_out->TimeStamp());
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, EmptyLastFrame) {
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  seq_num_ = 3;
  // Insert one empty packet per frame, should never return the last timestamp
  // inserted. Only return empty frames in the presence of subsequent frames.
  int maxSize = 1000;
  bool retransmitted = false;
  for (int i = 0; i < maxSize + 10; i++) {
    timestamp_ += 33 * 90;
    seq_num_++;
    packet_->is_first_packet_in_frame = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;
    packet_->frameType = kEmptyFrame;

    EXPECT_EQ(kNoError, jitter_buffer_->InsertPacket(*packet_, &retransmitted));
    VCMEncodedFrame* testFrame = DecodeIncompleteFrame();
    // Timestamp should never be the last TS inserted.
    if (testFrame != NULL) {
      EXPECT_TRUE(testFrame->TimeStamp() < timestamp_);
      jitter_buffer_->ReleaseFrame(testFrame);
    }
  }
}

TEST_F(TestBasicJitterBuffer, H264IncompleteNalu) {
  jitter_buffer_->SetNackMode(kNoNack, -1, -1);
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  ++seq_num_;
  timestamp_ += 33 * 90;
  int insertedLength = 0;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;
  bool retransmitted = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  seq_num_ += 2;  // Skip one packet.
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluIncomplete;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = true;  // Last packet.
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  // The JB will only output (incomplete) frames if a packet belonging to a
  // subsequent frame was already inserted. Insert one packet of a subsequent
  // frame. place high timestamp so the JB would always have a next frame
  // (otherwise, for every inserted frame we need to take care of the next
  // frame as well).
  packet_->seqNum = 1;
  packet_->timestamp = timestamp_ + 33 * 90 * 10;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  VCMEncodedFrame* frame_out = DecodeIncompleteFrame();

  // We can decode everything from a NALU until a packet has been lost.
  // Thus we can decode the first packet of the first NALU and the second NALU
  // which consists of one packet.
  CheckOutFrame(frame_out, packet_->sizeBytes * 2, false);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Test reordered start frame + 1 lost.
  seq_num_ += 2;  // Re-order 1 frame.
  timestamp_ += 33 * 90;
  insertedLength = 0;

  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  insertedLength += packet_->sizeBytes;  // This packet should be decoded.
  seq_num_--;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  insertedLength += packet_->sizeBytes;  // This packet should be decoded.

  seq_num_ += 3;  // One packet drop.
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  insertedLength += packet_->sizeBytes;  // This packet should be decoded.
  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  // This packet should be decoded since it's the beginning of a NAL.
  insertedLength += packet_->sizeBytes;

  seq_num_ += 2;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = true;
  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  // This packet should not be decoded because it is an incomplete NAL if it
  // is the last.
  frame_out = DecodeIncompleteFrame();
  // Only last NALU is complete.
  CheckOutFrame(frame_out, insertedLength, false);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Test to insert empty packet.
  seq_num_++;
  timestamp_ += 33 * 90;
  WebRtcRTPHeader rtpHeader;
  memset(&rtpHeader, 0, sizeof(rtpHeader));
  VCMPacket emptypacket(data_, 0, rtpHeader);
  emptypacket.seqNum = seq_num_;
  emptypacket.timestamp = timestamp_;
  emptypacket.frameType = kVideoFrameKey;
  emptypacket.is_first_packet_in_frame = true;
  emptypacket.completeNALU = kNaluComplete;
  emptypacket.markerBit = true;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(emptypacket, &retransmitted));
  // This packet should not be decoded because it is an incomplete NAL if it
  // is the last.

  // Will be sent to the decoder, as a packet belonging to a subsequent frame
  // has arrived.
  frame_out = DecodeIncompleteFrame();
  EXPECT_TRUE(frame_out != NULL);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Test that a frame can include an empty packet.
  seq_num_++;
  timestamp_ += 33 * 90;

  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  seq_num_++;
  emptypacket.seqNum = seq_num_;
  emptypacket.timestamp = timestamp_;
  emptypacket.frameType = kVideoFrameKey;
  emptypacket.is_first_packet_in_frame = true;
  emptypacket.completeNALU = kNaluComplete;
  emptypacket.markerBit = true;
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(emptypacket, &retransmitted));

  frame_out = DecodeCompleteFrame();
  // Only last NALU is complete
  CheckOutFrame(frame_out, packet_->sizeBytes, false);
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestBasicJitterBuffer, NextFrameWhenIncomplete) {
  // Test that a we cannot get incomplete frames from the JB if we haven't
  // received the marker bit, unless we have received a packet from a later
  // timestamp.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  // Start with a complete key frame - insert and decode.
  packet_->frameType = kVideoFrameKey;
  packet_->is_first_packet_in_frame = true;
  packet_->markerBit = true;
  bool retransmitted = false;

  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out != NULL);
  jitter_buffer_->ReleaseFrame(frame_out);

  packet_->seqNum += 2;
  packet_->timestamp += 33 * 90;
  packet_->frameType = kVideoFrameDelta;
  packet_->is_first_packet_in_frame = false;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeIncompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  packet_->seqNum += 2;
  packet_->timestamp += 33 * 90;
  packet_->is_first_packet_in_frame = true;

  EXPECT_EQ(kDecodableSession,
            jitter_buffer_->InsertPacket(*packet_, &retransmitted));

  frame_out = DecodeIncompleteFrame();
  CheckOutFrame(frame_out, packet_->sizeBytes, false);
  jitter_buffer_->ReleaseFrame(frame_out);
}

TEST_F(TestRunningJitterBuffer, Full) {
  // Make sure the jitter doesn't request a keyframe after too much non-
  // decodable frames.
  jitter_buffer_->SetNackMode(kNack, -1, -1);
  jitter_buffer_->SetNackSettings(kMaxNumberOfFrames, kMaxNumberOfFrames, 0);
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());
  DropFrame(1);
  // Fill the jitter buffer.
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kVideoFrameDelta), kNoError);
  // Make sure we can't decode these frames.
  EXPECT_FALSE(DecodeCompleteFrame());
  // This frame will make the jitter buffer recycle frames until a key frame.
  // Since none is found it will have to wait until the next key frame before
  // decoding.
  EXPECT_EQ(kFlushIndicator, InsertFrame(kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, EmptyPackets) {
  // Make sure a frame can get complete even though empty packets are missing.
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 3,
                                   clock_->TimeInMilliseconds());
  bool request_key_frame = false;
  // Insert empty packet.
  EXPECT_EQ(kNoError, InsertPacketAndPop(4));
  EXPECT_FALSE(request_key_frame);
  // Insert 3 media packets.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  // Insert empty packet.
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
}

TEST_F(TestRunningJitterBuffer, StatisticsTest) {
  FrameCounts frame_stats(jitter_buffer_->FrameStatistics());
  EXPECT_EQ(0, frame_stats.delta_frames);
  EXPECT_EQ(0, frame_stats.key_frames);

  uint32_t framerate = 0;
  uint32_t bitrate = 0;
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(0u, framerate);
  EXPECT_EQ(0u, bitrate);

  // Insert a couple of key and delta frames.
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  // Decode some of them to make sure the statistics doesn't depend on frames
  // being decoded.
  EXPECT_TRUE(DecodeCompleteFrame());
  EXPECT_TRUE(DecodeCompleteFrame());
  frame_stats = jitter_buffer_->FrameStatistics();
  EXPECT_EQ(3, frame_stats.delta_frames);
  EXPECT_EQ(2, frame_stats.key_frames);

  // Insert 20 more frames to get estimates of bitrate and framerate over
  // 1 second.
  for (int i = 0; i < 20; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  // TODO(holmer): The current implementation returns the average of the last
  // two framerate calculations, which is why it takes two calls to reach the
  // actual framerate. This should be fixed.
  EXPECT_EQ(kDefaultFrameRate / 2u, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
  // Insert 25 more frames to get estimates of bitrate and framerate over
  // 2 seconds.
  for (int i = 0; i < 25; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(kDefaultFrameRate, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
}

TEST_F(TestRunningJitterBuffer, SkipToKeyFrame) {
  // Insert delta frames.
  EXPECT_GE(InsertFrames(5, kVideoFrameDelta), kNoError);
  // Can't decode without a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  InsertFrame(kVideoFrameKey);
  // Skip to the next key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, DontSkipToKeyFrameIfDecodable) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  const int kNumDeltaFrames = 5;
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  for (int i = 0; i < kNumDeltaFrames + 1; ++i) {
    EXPECT_TRUE(DecodeCompleteFrame());
  }
}

TEST_F(TestRunningJitterBuffer, KeyDeltaKeyDelta) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  const int kNumDeltaFrames = 5;
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  for (int i = 0; i < 2 * (kNumDeltaFrames + 1); ++i) {
    EXPECT_TRUE(DecodeCompleteFrame());
  }
}

TEST_F(TestRunningJitterBuffer, TwoPacketsNonContinuous) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                   clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  stream_generator_->GenerateFrame(kVideoFrameDelta, 2, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_TRUE(DecodeCompleteFrame());
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, EmptyPackets) {
  // Make sure empty packets doesn't clog the jitter buffer.
  jitter_buffer_->SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kEmptyFrame), kNoError);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackTooOldPackets) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Drop one frame and insert |kNackHistoryLength| to trigger NACKing a too
  // old packet.
  DropFrame(1);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator,
            InsertFrames(oldest_packet_to_nack_ + 1, kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  bool request_key_frame = false;
  std::vector<uint16_t> nack_list =
      jitter_buffer_->GetNackList(&request_key_frame);
  // No key frame will be requested since the jitter buffer is empty.
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(0u, nack_list.size());

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // Waiting for a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());

  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackLargeJitterBuffer) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_GE(InsertFrames(oldest_packet_to_nack_, kVideoFrameDelta), kNoError);

  bool request_key_frame = false;
  std::vector<uint16_t> nack_list =
      jitter_buffer_->GetNackList(&request_key_frame);
  // Verify that the jitter buffer does not request a key frame.
  EXPECT_FALSE(request_key_frame);
  // Verify that no packets are NACKed.
  EXPECT_EQ(0u, nack_list.size());
  // Verify that we can decode the next frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackListFull) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Generate and drop |kNackHistoryLength| packets to fill the NACK list.
  DropFrame(max_nack_list_size_ + 1);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator, InsertFrame(kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  bool request_key_frame = false;
  jitter_buffer_->GetNackList(&request_key_frame);
  // The jitter buffer is empty, so we won't request key frames until we get a
  // packet.
  EXPECT_FALSE(request_key_frame);

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // Now we have a packet in the jitter buffer, a key frame will be requested
  // since it's not a key frame.
  jitter_buffer_->GetNackList(&request_key_frame);
  // The jitter buffer is empty, so we won't request key frames until we get a
  // packet.
  EXPECT_TRUE(request_key_frame);
  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NoNackListReturnedBeforeFirstDecode) {
  DropFrame(10);
  // Insert a frame and try to generate a NACK list. Shouldn't get one.
  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  bool request_key_frame = false;
  std::vector<uint16_t> nack_list =
      jitter_buffer_->GetNackList(&request_key_frame);
  // No list generated, and a key frame request is signaled.
  EXPECT_EQ(0u, nack_list.size());
  EXPECT_TRUE(request_key_frame);
}

TEST_F(TestJitterBufferNack, NackListBuiltBeforeFirstDecode) {
  stream_generator_->Init(0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  stream_generator_->GenerateFrame(kVideoFrameDelta, 2, 0,
                                   clock_->TimeInMilliseconds());
  stream_generator_->NextPacket(NULL);  // Drop packet.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_TRUE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  EXPECT_EQ(1u, nack_list.size());
}

TEST_F(TestJitterBufferNack, VerifyRetransmittedFlag) {
  stream_generator_->Init(0, clock_->TimeInMilliseconds());
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                   clock_->TimeInMilliseconds());
  VCMPacket packet;
  stream_generator_->PopPacket(&packet, 0);
  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(packet, &retransmitted));
  EXPECT_FALSE(retransmitted);
  // Drop second packet.
  stream_generator_->PopPacket(&packet, 1);
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(packet, &retransmitted));
  EXPECT_FALSE(retransmitted);
  EXPECT_FALSE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  uint16_t seq_num;
  EXPECT_EQ(1u, nack_list.size());
  seq_num = nack_list[0];
  stream_generator_->PopPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, seq_num);
  EXPECT_EQ(kCompleteSession,
            jitter_buffer_->InsertPacket(packet, &retransmitted));
  EXPECT_TRUE(retransmitted);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, UseNackToRecoverFirstKeyFrame) {
  stream_generator_->Init(0, clock_->TimeInMilliseconds());
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  uint16_t seq_num;
  ASSERT_EQ(1u, nack_list.size());
  seq_num = nack_list[0];
  VCMPacket packet;
  stream_generator_->GetPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, seq_num);
}

TEST_F(TestJitterBufferNack, UseNackToRecoverFirstKeyFrameSecondInQueue) {
  VCMPacket packet;
  stream_generator_->Init(0, clock_->TimeInMilliseconds());
  // First frame is delta.
  stream_generator_->GenerateFrame(kVideoFrameDelta, 3, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet in frame.
  ASSERT_TRUE(stream_generator_->PopPacket(&packet, 0));
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Second frame is key.
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                   clock_->TimeInMilliseconds() + 10);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet in frame.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  uint16_t seq_num;
  ASSERT_EQ(1u, nack_list.size());
  seq_num = nack_list[0];
  stream_generator_->GetPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, seq_num);
}

TEST_F(TestJitterBufferNack, NormalOperation) {
  EXPECT_EQ(kNack, jitter_buffer_->nack_mode());
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);

  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeIncompleteFrame());

  //  ----------------------------------------------------------------
  // | 1 | 2 | .. | 8 | 9 | x | 11 | 12 | .. | 19 | x | 21 | .. | 100 |
  //  ----------------------------------------------------------------
  stream_generator_->GenerateFrame(kVideoFrameKey, 100, 0,
                                   clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
  // Verify that the frame is incomplete.
  EXPECT_FALSE(DecodeCompleteFrame());
  while (stream_generator_->PacketsRemaining() > 1) {
    if (stream_generator_->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
  }
  EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
  EXPECT_EQ(0, stream_generator_->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  bool request_key_frame = false;

  // Verify the NACK list.
  std::vector<uint16_t> nack_list =
      jitter_buffer_->GetNackList(&request_key_frame);
  const size_t kExpectedNackSize = 9;
  ASSERT_EQ(kExpectedNackSize, nack_list.size());
  for (size_t i = 0; i < nack_list.size(); ++i)
    EXPECT_EQ((1 + i) * 10, nack_list[i]);
}

TEST_F(TestJitterBufferNack, NormalOperationWrap) {
  bool request_key_frame = false;
  //  -------   ------------------------------------------------------------
  // | 65532 | | 65533 | 65534 | 65535 | x | 1 | .. | 9 | x | 11 |.....| 96 |
  //  -------   ------------------------------------------------------------
  stream_generator_->Init(65532, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 100, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  while (stream_generator_->PacketsRemaining() > 1) {
    if (stream_generator_->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
      EXPECT_FALSE(request_key_frame);
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(0, stream_generator_->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  // Verify the NACK list.
  const size_t kExpectedNackSize = 10;
  ASSERT_EQ(kExpectedNackSize, nack_list.size());
  for (size_t i = 0; i < nack_list.size(); ++i)
    EXPECT_EQ(i * 10, nack_list[i]);
}

TEST_F(TestJitterBufferNack, NormalOperationWrap2) {
  bool request_key_frame = false;
  //  -----------------------------------
  // | 65532 | 65533 | 65534 | x | 0 | 1 |
  //  -----------------------------------
  stream_generator_->Init(65532, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                   clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  for (int i = 0; i < 5; ++i) {
    if (stream_generator_->NextSequenceNumber() != 65535) {
      EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
      EXPECT_FALSE(request_key_frame);
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
    stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                     clock_->TimeInMilliseconds());
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  // Verify the NACK list.
  ASSERT_EQ(1u, nack_list.size());
  EXPECT_EQ(65535, nack_list[0]);
}

TEST_F(TestJitterBufferNack, ResetByFutureKeyFrameDoesntError) {
  stream_generator_->Init(0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  bool extended = false;
  std::vector<uint16_t> nack_list = jitter_buffer_->GetNackList(&extended);
  EXPECT_EQ(0u, nack_list.size());

  // Far-into-the-future video frame, could be caused by resetting the encoder
  // or otherwise restarting. This should not fail when error when the packet is
  // a keyframe, even if all of the nack list needs to be flushed.
  stream_generator_->Init(10000, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  nack_list = jitter_buffer_->GetNackList(&extended);
  EXPECT_EQ(0u, nack_list.size());

  // Stream should be decodable from this point.
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  InsertFrame(kVideoFrameDelta);
  EXPECT_TRUE(DecodeCompleteFrame());
  nack_list = jitter_buffer_->GetNackList(&extended);
  EXPECT_EQ(0u, nack_list.size());
}
}  // namespace webrtc
