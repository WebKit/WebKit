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
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/random.h"
#include "rtc_base/refcount.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {

class FakePacketBuffer : public PacketBuffer {
 public:
  FakePacketBuffer() : PacketBuffer(nullptr, 0, 0, nullptr) {}

  VCMPacket* GetPacket(uint16_t seq_num) override {
    auto packet_it = packets_.find(seq_num);
    return packet_it == packets_.end() ? nullptr : &packet_it->second;
  }

  bool InsertPacket(VCMPacket* packet) override {
    packets_[packet->seqNum] = *packet;
    return true;
  }

  bool GetBitstream(const RtpFrameObject& frame,
                    uint8_t* destination) override {
    return true;
  }

  void ReturnFrame(RtpFrameObject* frame) override {
    packets_.erase(frame->first_seq_num());
  }

 private:
  std::map<uint16_t, VCMPacket> packets_;
};

class TestRtpFrameReferenceFinder : public ::testing::Test,
                                    public OnCompleteFrameCallback {
 protected:
  static constexpr uint64_t kUnwrappedSequenceStart = 1000000000000000000UL;

  TestRtpFrameReferenceFinder()
      : rand_(0x8739211),
        ref_packet_buffer_(new FakePacketBuffer()),
        reference_finder_(new RtpFrameReferenceFinder(this)),
        frames_from_callback_(FrameComp()) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) override {
    int64_t pid = frame->id.picture_id;
    uint16_t sidx = frame->id.spatial_layer;
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it != frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with (pid:sidx): (" << pid << ":"
                    << sidx << ")";
      return;
    }

    frames_from_callback_.insert(
        std::make_pair(std::make_pair(pid, sidx), std::move(frame)));
  }

  void InsertGeneric(uint16_t seq_num_start,
                     uint16_t seq_num_end,
                     bool keyframe) {
    VCMPacket packet;
    packet.codec = kVideoCodecGeneric;
    packet.seqNum = seq_num_start;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    ref_packet_buffer_->InsertPacket(&packet);

    packet.seqNum = seq_num_end;
    packet.is_last_packet_in_frame = true;
    ref_packet_buffer_->InsertPacket(&packet);

    std::unique_ptr<RtpFrameObject> frame(new RtpFrameObject(
        ref_packet_buffer_, seq_num_start, seq_num_end, 0, 0, 0));
    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertVp8(uint16_t seq_num_start,
                 uint16_t seq_num_end,
                 bool keyframe,
                 int32_t pid = kNoPictureId,
                 uint8_t tid = kNoTemporalIdx,
                 int32_t tl0 = kNoTl0PicIdx,
                 bool sync = false) {
    VCMPacket packet;
    packet.codec = kVideoCodecVP8;
    packet.seqNum = seq_num_start;
    packet.is_last_packet_in_frame = (seq_num_start == seq_num_end);
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    auto& vp8_header =
        packet.video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
    vp8_header.pictureId = pid % (1 << 15);
    vp8_header.temporalIdx = tid;
    vp8_header.tl0PicIdx = tl0;
    vp8_header.layerSync = sync;
    ref_packet_buffer_->InsertPacket(&packet);

    if (seq_num_start != seq_num_end) {
      packet.seqNum = seq_num_end;
      packet.is_last_packet_in_frame = true;
      ref_packet_buffer_->InsertPacket(&packet);
    }

    std::unique_ptr<RtpFrameObject> frame(new RtpFrameObject(
        ref_packet_buffer_, seq_num_start, seq_num_end, 0, 0, 0));
    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertVp9Gof(uint16_t seq_num_start,
                    uint16_t seq_num_end,
                    bool keyframe,
                    int32_t pid = kNoPictureId,
                    uint8_t sid = kNoSpatialIdx,
                    uint8_t tid = kNoTemporalIdx,
                    int32_t tl0 = kNoTl0PicIdx,
                    bool up_switch = false,
                    GofInfoVP9* ss = nullptr) {
    VCMPacket packet;
    auto& vp9_header =
        packet.video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
    packet.timestamp = pid;
    packet.codec = kVideoCodecVP9;
    packet.seqNum = seq_num_start;
    packet.is_last_packet_in_frame = (seq_num_start == seq_num_end);
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    vp9_header.flexible_mode = false;
    vp9_header.picture_id = pid % (1 << 15);
    vp9_header.temporal_idx = tid;
    vp9_header.spatial_idx = sid;
    vp9_header.tl0_pic_idx = tl0;
    vp9_header.temporal_up_switch = up_switch;
    if (ss != nullptr) {
      vp9_header.ss_data_available = true;
      vp9_header.gof = *ss;
    }
    ref_packet_buffer_->InsertPacket(&packet);

    if (seq_num_start != seq_num_end) {
      packet.is_last_packet_in_frame = true;
      vp9_header.ss_data_available = false;
      packet.seqNum = seq_num_end;
      ref_packet_buffer_->InsertPacket(&packet);
    }

    std::unique_ptr<RtpFrameObject> frame(new RtpFrameObject(
        ref_packet_buffer_, seq_num_start, seq_num_end, 0, 0, 0));
    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertVp9Flex(uint16_t seq_num_start,
                     uint16_t seq_num_end,
                     bool keyframe,
                     int32_t pid = kNoPictureId,
                     uint8_t sid = kNoSpatialIdx,
                     uint8_t tid = kNoTemporalIdx,
                     bool inter = false,
                     std::vector<uint8_t> refs = std::vector<uint8_t>()) {
    VCMPacket packet;
    auto& vp9_header =
        packet.video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
    packet.timestamp = pid;
    packet.codec = kVideoCodecVP9;
    packet.seqNum = seq_num_start;
    packet.is_last_packet_in_frame = (seq_num_start == seq_num_end);
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    vp9_header.inter_layer_predicted = inter;
    vp9_header.flexible_mode = true;
    vp9_header.picture_id = pid % (1 << 15);
    vp9_header.temporal_idx = tid;
    vp9_header.spatial_idx = sid;
    vp9_header.tl0_pic_idx = kNoTl0PicIdx;
    vp9_header.num_ref_pics = refs.size();
    for (size_t i = 0; i < refs.size(); ++i)
      vp9_header.pid_diff[i] = refs[i];
    ref_packet_buffer_->InsertPacket(&packet);

    if (seq_num_start != seq_num_end) {
      packet.seqNum = seq_num_end;
      packet.is_last_packet_in_frame = true;
      ref_packet_buffer_->InsertPacket(&packet);
    }

    std::unique_ptr<RtpFrameObject> frame(new RtpFrameObject(
        ref_packet_buffer_, seq_num_start, seq_num_end, 0, 0, 0));
    reference_finder_->ManageFrame(std::move(frame));
  }

  // Check if a frame with picture id |pid| and spatial index |sidx| has been
  // delivered from the packet buffer, and if so, if it has the references
  // specified by |refs|.
  template <typename... T>
  void CheckReferences(int64_t picture_id_offset,
                       uint16_t sidx,
                       T... refs) const {
    int64_t pid = kUnwrappedSequenceStart + picture_id_offset;
    auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
    if (frame_it == frames_from_callback_.end()) {
      ADD_FAILURE() << "Could not find frame with (pid:sidx): (" << pid << ":"
                    << sidx << ")";
      return;
    }

    std::set<int64_t> actual_refs;
    for (uint8_t r = 0; r < frame_it->second->num_references; ++r)
      actual_refs.insert(frame_it->second->references[r]);

    std::set<int64_t> expected_refs;
    RefsToSet(&expected_refs, refs...);

    ASSERT_EQ(expected_refs, actual_refs);
  }

  template <typename... T>
  void CheckReferencesGeneric(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesVp8(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void CheckReferencesVp9(int64_t pid, uint8_t sidx, T... refs) const {
    CheckReferences(pid, sidx, refs...);
  }

  template <typename... T>
  void RefsToSet(std::set<int64_t>* m, int64_t ref, T... refs) const {
    m->insert(ref + kUnwrappedSequenceStart);
    RefsToSet(m, refs...);
  }

  void RefsToSet(std::set<int64_t>* m) const {}

  Random rand_;
  rtc::scoped_refptr<FakePacketBuffer> ref_packet_buffer_;
  std::unique_ptr<RtpFrameReferenceFinder> reference_finder_;
  struct FrameComp {
    bool operator()(const std::pair<int64_t, uint8_t> f1,
                    const std::pair<int64_t, uint8_t> f2) const {
      if (f1.first == f2.first)
        return f1.second < f2.second;
      return f1.first < f2.first;
    }
  };
  std::
      map<std::pair<int64_t, uint8_t>, std::unique_ptr<EncodedFrame>, FrameComp>
          frames_from_callback_;
};

TEST_F(TestRtpFrameReferenceFinder, PaddingPackets) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertGeneric(sn + 2, sn + 2, false);
  EXPECT_EQ(1UL, frames_from_callback_.size());
  reference_finder_->PaddingReceived(sn + 1);
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReordered) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  reference_finder_->PaddingReceived(sn + 1);
  reference_finder_->PaddingReceived(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesGeneric(0);
  CheckReferencesGeneric(3, 0);
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReorderedMultipleKeyframes) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  reference_finder_->PaddingReceived(sn + 1);
  reference_finder_->PaddingReceived(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);
  InsertGeneric(sn + 5, sn + 5, true);
  reference_finder_->PaddingReceived(sn + 6);
  reference_finder_->PaddingReceived(sn + 9);
  InsertGeneric(sn + 7, sn + 8, false);

  EXPECT_EQ(4UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, AdvanceSavedKeyframe) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertGeneric(sn + 1, sn + 1, true);
  InsertGeneric(sn + 2, sn + 10000, false);
  InsertGeneric(sn + 10001, sn + 20000, false);
  InsertGeneric(sn + 20001, sn + 30000, false);
  InsertGeneric(sn + 30001, sn + 40000, false);

  EXPECT_EQ(6UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, ClearTo) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn + 1, true);
  InsertGeneric(sn + 4, sn + 5, false);  // stashed
  EXPECT_EQ(1UL, frames_from_callback_.size());

  InsertGeneric(sn + 6, sn + 7, true);  // keyframe
  EXPECT_EQ(2UL, frames_from_callback_.size());
  reference_finder_->ClearTo(sn + 7);

  InsertGeneric(sn + 8, sn + 9, false);  // first frame after keyframe.
  EXPECT_EQ(3UL, frames_from_callback_.size());

  InsertGeneric(sn + 2, sn + 3, false);  // late, cleared past this frame.
  EXPECT_EQ(3UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, Vp8NoPictureId) {
  uint16_t sn = Rand();

  InsertVp8(sn, sn + 2, true);
  ASSERT_EQ(1UL, frames_from_callback_.size());

  InsertVp8(sn + 3, sn + 4, false);
  ASSERT_EQ(2UL, frames_from_callback_.size());

  InsertVp8(sn + 5, sn + 8, false);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(sn + 9, sn + 9, false);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(sn + 10, sn + 11, false);
  ASSERT_EQ(5UL, frames_from_callback_.size());

  InsertVp8(sn + 12, sn + 12, true);
  ASSERT_EQ(6UL, frames_from_callback_.size());

  InsertVp8(sn + 13, sn + 17, false);
  ASSERT_EQ(7UL, frames_from_callback_.size());

  InsertVp8(sn + 18, sn + 18, false);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  InsertVp8(sn + 19, sn + 20, false);
  ASSERT_EQ(9UL, frames_from_callback_.size());

  InsertVp8(sn + 21, sn + 21, false);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(6, 2);
  CheckReferencesVp8(7, 6);
  CheckReferencesVp8(9, 7);
  CheckReferencesVp8(10);
  CheckReferencesVp8(15, 10);
  CheckReferencesVp8(16, 15);
  CheckReferencesVp8(18, 16);
  CheckReferencesVp8(19, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8NoPictureIdReordered) {
  uint16_t sn = 0xfffa;

  InsertVp8(sn, sn + 2, true);
  InsertVp8(sn + 3, sn + 4, false);
  InsertVp8(sn + 5, sn + 8, false);
  InsertVp8(sn + 9, sn + 9, false);
  InsertVp8(sn + 10, sn + 11, false);
  InsertVp8(sn + 12, sn + 12, true);
  InsertVp8(sn + 13, sn + 17, false);
  InsertVp8(sn + 18, sn + 18, false);
  InsertVp8(sn + 19, sn + 20, false);
  InsertVp8(sn + 21, sn + 21, false);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(6, 2);
  CheckReferencesVp8(7, 6);
  CheckReferencesVp8(9, 7);
  CheckReferencesVp8(10);
  CheckReferencesVp8(15, 10);
  CheckReferencesVp8(16, 15);
  CheckReferencesVp8(18, 16);
  CheckReferencesVp8(19, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8KeyFrameReferences) {
  uint16_t sn = Rand();
  InsertVp8(sn, sn, true);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
}

// Test with 1 temporal layer.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 1);
  CheckReferencesVp8(3, 2);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8DuplicateTl1Frames) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 0);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 0, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 1);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 1);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 1);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 2);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 2);

  ASSERT_EQ(6UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 1, 2);
  CheckReferencesVp8(4, 2);
  CheckReferencesVp8(5, 3, 4);
}

// Test with 1 temporal layer.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 0, 6);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 7);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 5);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 1);
  CheckReferencesVp8(3, 2);
  CheckReferencesVp8(4, 3);
  CheckReferencesVp8(5, 4);
  CheckReferencesVp8(6, 5);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 255);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 255, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 0);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 0);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 1, 2);
}

// Test with 2 temporal layers in a 01 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 255, true);
  InsertVp8(sn, sn, true, pid, 0, 255);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 1, 0);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 1);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 0);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 1);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 2);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 1, 2);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 1, 2);
  CheckReferencesVp8(4, 2);
  CheckReferencesVp8(5, 3, 4);
  CheckReferencesVp8(6, 4);
  CheckReferencesVp8(7, 5, 6);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 55);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, 55, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 56);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 2, 56);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 1, 56);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 2, 56);
  InsertVp8(sn + 8, sn + 8, false, pid + 8, 0, 57);
  InsertVp8(sn + 9, sn + 9, false, pid + 9, 2, 57, true);
  InsertVp8(sn + 10, sn + 10, false, pid + 10, 1, 57, true);
  InsertVp8(sn + 11, sn + 11, false, pid + 11, 2, 57);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 0, 1, 2);
  CheckReferencesVp8(4, 0);
  CheckReferencesVp8(5, 2, 3, 4);
  CheckReferencesVp8(6, 2, 4);
  CheckReferencesVp8(7, 4, 5, 6);
  CheckReferencesVp8(8, 4);
  CheckReferencesVp8(9, 8);
  CheckReferencesVp8(10, 8);
  CheckReferencesVp8(11, 8, 9, 10);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersMissingFrame_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 55, false);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55, false);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(2, 0);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersReordering_0212) {
  uint16_t pid = 126;
  uint16_t sn = Rand();

  InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, 55, true);
  InsertVp8(sn, sn, true, pid, 0, 55, false);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 56, false);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 2, 56, false);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55, false);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 2, 56, false);
  InsertVp8(sn + 9, sn + 9, false, pid + 9, 2, 57, true);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 1, 56, false);
  InsertVp8(sn + 8, sn + 8, false, pid + 8, 0, 57, false);
  InsertVp8(sn + 11, sn + 11, false, pid + 11, 2, 57, false);
  InsertVp8(sn + 10, sn + 10, false, pid + 10, 1, 57, true);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 0, 1, 2);
  CheckReferencesVp8(4, 0);
  CheckReferencesVp8(5, 2, 3, 4);
  CheckReferencesVp8(6, 2, 4);
  CheckReferencesVp8(7, 4, 5, 6);
  CheckReferencesVp8(8, 4);
  CheckReferencesVp8(9, 8);
  CheckReferencesVp8(10, 8);
  CheckReferencesVp8(11, 8, 9, 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8InsertManyFrames_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  const int keyframes_to_insert = 50;
  const int frames_per_keyframe = 120;  // Should be a multiple of 4.
  int64_t offset = 0;
  uint8_t tl0 = 128;

  for (int k = 0; k < keyframes_to_insert; ++k) {
    InsertVp8(sn, sn, true, pid, 0, tl0, false);
    InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, tl0, true);
    InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, tl0, true);
    InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, tl0, false);
    CheckReferencesVp8(offset);
    CheckReferencesVp8(offset + 1, offset);
    CheckReferencesVp8(offset + 2, offset);
    CheckReferencesVp8(offset + 3, offset, offset + 1, offset + 2);
    frames_from_callback_.clear();
    ++tl0;

    for (int f = 4; f < frames_per_keyframe; f += 4) {
      uint16_t sf = sn + f;
      uint16_t pidf = pid + f;
      int64_t offsetf = offset + f;

      InsertVp8(sf, sf, false, pidf, 0, tl0, false);
      InsertVp8(sf + 1, sf + 1, false, pidf + 1, 2, tl0, false);
      InsertVp8(sf + 2, sf + 2, false, pidf + 2, 1, tl0, false);
      InsertVp8(sf + 3, sf + 3, false, pidf + 3, 2, tl0, false);
      CheckReferencesVp8(offsetf, offsetf - 4);
      CheckReferencesVp8(offsetf + 1, offsetf, offsetf - 1, offsetf - 2);
      CheckReferencesVp8(offsetf + 2, offsetf, offsetf - 2);
      CheckReferencesVp8(offsetf + 3, offsetf, offsetf + 1, offsetf + 2);
      frames_from_callback_.clear();
      ++tl0;
    }

    offset += frames_per_keyframe;
    pid += frames_per_keyframe;
    sn += frames_per_keyframe;
  }
}

TEST_F(TestRtpFrameReferenceFinder, Vp8LayerSync) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 0, false);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 0, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 1, false);
  ASSERT_EQ(3UL, frames_from_callback_.size());

  InsertVp8(sn + 4, sn + 4, false, pid + 4, 0, 2, false);
  InsertVp8(sn + 5, sn + 5, false, pid + 5, 1, 2, true);
  InsertVp8(sn + 6, sn + 6, false, pid + 6, 0, 3, false);
  InsertVp8(sn + 7, sn + 7, false, pid + 7, 1, 3, false);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(4, 2);
  CheckReferencesVp8(5, 4);
  CheckReferencesVp8(6, 4);
  CheckReferencesVp8(7, 6, 5);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8Tl1SyncFrameAfterTl1Frame) {
  InsertVp8(1000, 1000, true, 1, 0, 247, true);
  InsertVp8(1001, 1001, false, 3, 0, 248, false);
  InsertVp8(1002, 1002, false, 4, 1, 248, false);  // Will be dropped
  InsertVp8(1003, 1003, false, 5, 1, 248, true);   // due to this frame.

  ASSERT_EQ(3UL, frames_from_callback_.size());
  CheckReferencesVp8(0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(4, 2);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8DetectMissingFrame_0212) {
  InsertVp8(1, 1, true, 1, 0, 1, false);
  InsertVp8(2, 2, false, 2, 2, 1, true);
  InsertVp8(3, 3, false, 3, 1, 1, true);
  InsertVp8(4, 4, false, 4, 2, 1, false);

  InsertVp8(6, 6, false, 6, 2, 2, false);
  InsertVp8(7, 7, false, 7, 1, 2, false);
  InsertVp8(8, 8, false, 8, 2, 2, false);
  ASSERT_EQ(4UL, frames_from_callback_.size());

  InsertVp8(5, 5, false, 5, 0, 2, false);
  ASSERT_EQ(8UL, frames_from_callback_.size());

  CheckReferencesVp8(0);
  CheckReferencesVp8(1, 0);
  CheckReferencesVp8(2, 0);
  CheckReferencesVp8(3, 2, 1, 0);

  CheckReferencesVp8(4, 0);
  CheckReferencesVp8(5, 4, 3, 2);
  CheckReferencesVp8(6, 4, 2);
  CheckReferencesVp8(7, 6, 5, 4);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofInsertOneFrame) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);

  CheckReferencesVp9(0, 0);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9NoPictureIdReordered) {
  uint16_t sn = 0xfffa;

  InsertVp9Gof(sn, sn + 2, true);
  InsertVp9Gof(sn + 3, sn + 4, false);
  InsertVp9Gof(sn + 9, sn + 9, false);
  InsertVp9Gof(sn + 5, sn + 8, false);
  InsertVp9Gof(sn + 12, sn + 12, true);
  InsertVp9Gof(sn + 10, sn + 11, false);
  InsertVp9Gof(sn + 13, sn + 17, false);
  InsertVp9Gof(sn + 19, sn + 20, false);
  InsertVp9Gof(sn + 21, sn + 21, false);
  InsertVp9Gof(sn + 18, sn + 18, false);

  ASSERT_EQ(10UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(6, 0, 2);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(9, 0, 7);
  CheckReferencesVp9(10, 0);
  CheckReferencesVp9(15, 0, 10);
  CheckReferencesVp9(16, 0, 15);
  CheckReferencesVp9(18, 0, 16);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 2, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 0, 3, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 4, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 0, 5, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 0, 6, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 0, 7, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 8, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 0, 9, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 10, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 0, 11, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 12, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 0, 13, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 0, 14, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 0, 15, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 16, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 0, 17, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 0, 18, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 0, 19, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 1);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 3);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 5);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 7);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 9);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 11);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 13);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 15);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 17);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 2, false);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 4, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 0, 3, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 0, 5, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 0, 7, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 0, 6, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 8, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 10, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 0, 13, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 0, 11, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 0, 9, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 16, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 0, 14, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 0, 15, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 12, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 0, 17, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 0, 19, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 0, 18, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 1);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 3);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 5);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 7);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 9);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 11);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 13);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 15);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 17);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofSkipFramesTemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 0101 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  // Skip GOF with tl0 1
  InsertVp9Gof(sn + 4, sn + 4, true, pid + 4, 0, 0, 2, false, &ss);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 1, 2, false);
  // Skip GOF with tl0 3
  // Skip GOF with tl0 4
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 5, false, &ss);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 1, 5, false);

  ASSERT_EQ(6UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(4, 0);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofSkipFramesTemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 02120212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);

  // Skip frames with tl0 = 1

  InsertVp9Gof(sn + 8, sn + 8, true, pid + 8, 0, 0, 2, false, &ss);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, false);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferencesVp9(8, 0);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);

  // Now insert frames with tl0 = 1
  InsertVp9Gof(sn + 4, sn + 4, true, pid + 4, 0, 0, 1, false, &ss);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);

  ASSERT_EQ(9UL, frames_from_callback_.size());
  CheckReferencesVp9(4, 0);

  // Rest of frames belonging to tl0 = 1
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, true);  // up-switch

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 0101 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 1, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 1, 1, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 2, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 1, 2, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 0, 3, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 1, 3, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 4, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 1, 4, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 5, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 1, 5, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 6, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 1, 6, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 0, 7, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 1, 7, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 8, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 1, 8, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 0, 9, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 1, 9, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 2);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 6);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 10);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 12);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 14);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 16);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 1, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 2, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 1, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 1, 2, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 1, 3, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 0, 3, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 5, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 4, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 1, 4, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 1, 5, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 1, 6, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 8, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 6, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 0, 7, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 1, 8, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 1, 9, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 1, 7, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 0, 9, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 2);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 6);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 10);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 12);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 14);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 16);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 2, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 3, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 2, 3, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 1, 3, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 2, 3, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 4, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 2, 4, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 1, 4, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 2, 4, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 0);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 4);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 8);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 12);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 12);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 16);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 1, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 2, 3, false);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 3, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 1, 3, false);
  InsertVp9Gof(sn + 16, sn + 16, false, pid + 16, 0, 0, 4, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 2, 3, false);
  InsertVp9Gof(sn + 17, sn + 17, false, pid + 17, 0, 2, 4, false);
  InsertVp9Gof(sn + 19, sn + 19, false, pid + 19, 0, 2, 4, false);
  InsertVp9Gof(sn + 18, sn + 18, false, pid + 18, 0, 1, 4, false);

  ASSERT_EQ(20UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 0);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 4);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
  CheckReferencesVp9(12, 0, 8);
  CheckReferencesVp9(13, 0, 12);
  CheckReferencesVp9(14, 0, 12);
  CheckReferencesVp9(15, 0, 14);
  CheckReferencesVp9(16, 0, 12);
  CheckReferencesVp9(17, 0, 16);
  CheckReferencesVp9(18, 0, 16);
  CheckReferencesVp9(19, 0, 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersUpSwitch_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, true);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 2, true);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, true);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 3, false);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 2, 3, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 1, 3, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 2, 3, false);

  ASSERT_EQ(16UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 1, 2);
  CheckReferencesVp9(4, 0, 0);
  CheckReferencesVp9(5, 0, 3, 4);
  CheckReferencesVp9(6, 0, 2, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 4);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 9, 10);
  CheckReferencesVp9(12, 0, 8);
  CheckReferencesVp9(13, 0, 11, 12);
  CheckReferencesVp9(14, 0, 10, 12);
  CheckReferencesVp9(15, 0, 13, 14);
}

TEST_F(TestRtpFrameReferenceFinder,
       Vp9GofTemporalLayersUpSwitchReordered_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 1, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, true);
  InsertVp9Gof(sn + 12, sn + 12, false, pid + 12, 0, 0, 3, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 2, true);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, true);
  InsertVp9Gof(sn + 13, sn + 13, false, pid + 13, 0, 2, 3, false);
  InsertVp9Gof(sn + 15, sn + 15, false, pid + 15, 0, 2, 3, false);
  InsertVp9Gof(sn + 14, sn + 14, false, pid + 14, 0, 1, 3, false);

  ASSERT_EQ(16UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 1, 2);
  CheckReferencesVp9(4, 0, 0);
  CheckReferencesVp9(5, 0, 3, 4);
  CheckReferencesVp9(6, 0, 2, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 4);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 9, 10);
  CheckReferencesVp9(12, 0, 8);
  CheckReferencesVp9(13, 0, 11, 12);
  CheckReferencesVp9(14, 0, 10, 12);
  CheckReferencesVp9(15, 0, 13, 14);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_01_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 1, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 2, false);
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 2, false, &ss);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 2, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 3, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 3, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 3, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 3, false);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(3, 0, 2);
  CheckReferencesVp9(4, 0, 0);
  CheckReferencesVp9(5, 0, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(7, 0, 6);
  CheckReferencesVp9(8, 0, 4);
  CheckReferencesVp9(9, 0, 8);
  CheckReferencesVp9(10, 0, 8);
  CheckReferencesVp9(11, 0, 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9FlexibleModeOneFrame) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp9Flex(sn, sn, true, pid, 0, 0, false);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9FlexibleModeTwoSpatialLayers) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp9Flex(sn, sn, true, pid, 0, 0, false);
  InsertVp9Flex(sn + 1, sn + 1, true, pid, 1, 0, true);
  InsertVp9Flex(sn + 2, sn + 2, false, pid + 1, 1, 0, false, {1});
  InsertVp9Flex(sn + 3, sn + 3, false, pid + 2, 0, 0, false, {2});
  InsertVp9Flex(sn + 4, sn + 4, false, pid + 2, 1, 0, false, {1});
  InsertVp9Flex(sn + 5, sn + 5, false, pid + 3, 1, 0, false, {1});
  InsertVp9Flex(sn + 6, sn + 6, false, pid + 4, 0, 0, false, {2});
  InsertVp9Flex(sn + 7, sn + 7, false, pid + 4, 1, 0, false, {1});
  InsertVp9Flex(sn + 8, sn + 8, false, pid + 5, 1, 0, false, {1});
  InsertVp9Flex(sn + 9, sn + 9, false, pid + 6, 0, 0, false, {2});
  InsertVp9Flex(sn + 10, sn + 10, false, pid + 6, 1, 0, false, {1});
  InsertVp9Flex(sn + 11, sn + 11, false, pid + 7, 1, 0, false, {1});
  InsertVp9Flex(sn + 12, sn + 12, false, pid + 8, 0, 0, false, {2});
  InsertVp9Flex(sn + 13, sn + 13, false, pid + 8, 1, 0, false, {1});

  ASSERT_EQ(14UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(0, 1);
  CheckReferencesVp9(1, 1, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(2, 1, 1);
  CheckReferencesVp9(3, 1, 2);
  CheckReferencesVp9(4, 0, 2);
  CheckReferencesVp9(4, 1, 3);
  CheckReferencesVp9(5, 1, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(6, 1, 5);
  CheckReferencesVp9(7, 1, 6);
  CheckReferencesVp9(8, 0, 6);
  CheckReferencesVp9(8, 1, 7);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9FlexibleModeTwoSpatialLayersReordered) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp9Flex(sn + 1, sn + 1, true, pid, 1, 0, true);
  InsertVp9Flex(sn + 2, sn + 2, false, pid + 1, 1, 0, false, {1});
  InsertVp9Flex(sn, sn, true, pid, 0, 0, false);
  InsertVp9Flex(sn + 4, sn + 4, false, pid + 2, 1, 0, false, {1});
  InsertVp9Flex(sn + 5, sn + 5, false, pid + 3, 1, 0, false, {1});
  InsertVp9Flex(sn + 3, sn + 3, false, pid + 2, 0, 0, false, {2});
  InsertVp9Flex(sn + 7, sn + 7, false, pid + 4, 1, 0, false, {1});
  InsertVp9Flex(sn + 6, sn + 6, false, pid + 4, 0, 0, false, {2});
  InsertVp9Flex(sn + 8, sn + 8, false, pid + 5, 1, 0, false, {1});
  InsertVp9Flex(sn + 9, sn + 9, false, pid + 6, 0, 0, false, {2});
  InsertVp9Flex(sn + 11, sn + 11, false, pid + 7, 1, 0, false, {1});
  InsertVp9Flex(sn + 10, sn + 10, false, pid + 6, 1, 0, false, {1});
  InsertVp9Flex(sn + 13, sn + 13, false, pid + 8, 1, 0, false, {1});
  InsertVp9Flex(sn + 12, sn + 12, false, pid + 8, 0, 0, false, {2});

  ASSERT_EQ(14UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(0, 1);
  CheckReferencesVp9(1, 1, 0);
  CheckReferencesVp9(2, 0, 0);
  CheckReferencesVp9(2, 1, 1);
  CheckReferencesVp9(3, 1, 2);
  CheckReferencesVp9(4, 0, 2);
  CheckReferencesVp9(4, 1, 3);
  CheckReferencesVp9(5, 1, 4);
  CheckReferencesVp9(6, 0, 4);
  CheckReferencesVp9(6, 1, 5);
  CheckReferencesVp9(7, 1, 6);
  CheckReferencesVp9(8, 0, 6);
  CheckReferencesVp9(8, 1, 7);
}

TEST_F(TestRtpFrameReferenceFinder, WrappingFlexReference) {
  InsertVp9Flex(0, 0, false, 0, 0, 0, false, {1});

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(1, 0, 0);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofPidJump) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1000, 0, 0, 1);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTl0Jump) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);

  InsertVp9Gof(sn, sn, true, pid + 0, 0, 0, 125, true, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 0, false, &ss);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTidTooHigh) {
  // Same as RtpFrameReferenceFinder::kMaxTemporalLayers.
  const int kMaxTemporalLayers = 5;
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);
  ss.temporal_idx[1] = kMaxTemporalLayers;

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofZeroFrames) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.num_frames_in_gof = 0;

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp9(0, 0);
  CheckReferencesVp9(1, 0, 0);
}

}  // namespace video_coding
}  // namespace webrtc
