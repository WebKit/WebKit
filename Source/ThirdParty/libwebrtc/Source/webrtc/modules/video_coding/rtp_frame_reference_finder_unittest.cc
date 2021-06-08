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
#include <vector>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/random.h"
#include "rtc_base/ref_count.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {

namespace {
std::unique_ptr<RtpFrameObject> CreateFrame(
    uint16_t seq_num_start,
    uint16_t seq_num_end,
    bool keyframe,
    VideoCodecType codec,
    const RTPVideoTypeHeader& video_type_header) {
  RTPVideoHeader video_header;
  video_header.frame_type = keyframe ? VideoFrameType::kVideoFrameKey
                                     : VideoFrameType::kVideoFrameDelta;
  video_header.video_type_header = video_type_header;

  // clang-format off
  return std::make_unique<RtpFrameObject>(
      seq_num_start,
      seq_num_end,
      /*markerBit=*/true,
      /*times_nacked=*/0,
      /*first_packet_received_time=*/0,
      /*last_packet_received_time=*/0,
      /*rtp_timestamp=*/0,
      /*ntp_time_ms=*/0,
      VideoSendTiming(),
      /*payload_type=*/0,
      codec,
      kVideoRotation_0,
      VideoContentType::UNSPECIFIED,
      video_header,
      /*color_space=*/absl::nullopt,
      RtpPacketInfos(),
      EncodedImageBuffer::Create(/*size=*/0));
  // clang-format on
}
}  // namespace

class TestRtpFrameReferenceFinder : public ::testing::Test,
                                    public OnCompleteFrameCallback {
 protected:
  TestRtpFrameReferenceFinder()
      : rand_(0x8739211),
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
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecGeneric,
                    RTPVideoTypeHeader());

    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertVp8(uint16_t seq_num_start,
                 uint16_t seq_num_end,
                 bool keyframe,
                 int32_t pid = kNoPictureId,
                 uint8_t tid = kNoTemporalIdx,
                 int32_t tl0 = kNoTl0PicIdx,
                 bool sync = false) {
    RTPVideoHeaderVP8 vp8_header{};
    vp8_header.pictureId = pid % (1 << 15);
    vp8_header.temporalIdx = tid;
    vp8_header.tl0PicIdx = tl0;
    vp8_header.layerSync = sync;

    std::unique_ptr<RtpFrameObject> frame = CreateFrame(
        seq_num_start, seq_num_end, keyframe, kVideoCodecVP8, vp8_header);

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
                    bool inter_pic_predicted = true,
                    GofInfoVP9* ss = nullptr) {
    RTPVideoHeaderVP9 vp9_header{};
    vp9_header.flexible_mode = false;
    vp9_header.picture_id = pid % (1 << 15);
    vp9_header.temporal_idx = tid;
    vp9_header.spatial_idx = sid;
    vp9_header.tl0_pic_idx = tl0;
    vp9_header.temporal_up_switch = up_switch;
    vp9_header.inter_pic_predicted = inter_pic_predicted && !keyframe;
    if (ss != nullptr) {
      vp9_header.ss_data_available = true;
      vp9_header.gof = *ss;
    }

    std::unique_ptr<RtpFrameObject> frame = CreateFrame(
        seq_num_start, seq_num_end, keyframe, kVideoCodecVP9, vp9_header);

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
    RTPVideoHeaderVP9 vp9_header{};
    vp9_header.inter_layer_predicted = inter;
    vp9_header.flexible_mode = true;
    vp9_header.picture_id = pid % (1 << 15);
    vp9_header.temporal_idx = tid;
    vp9_header.spatial_idx = sid;
    vp9_header.tl0_pic_idx = kNoTl0PicIdx;
    vp9_header.num_ref_pics = refs.size();
    for (size_t i = 0; i < refs.size(); ++i)
      vp9_header.pid_diff[i] = refs[i];

    std::unique_ptr<RtpFrameObject> frame = CreateFrame(
        seq_num_start, seq_num_end, keyframe, kVideoCodecVP9, vp9_header);
    reference_finder_->ManageFrame(std::move(frame));
  }

  void InsertH264(uint16_t seq_num_start, uint16_t seq_num_end, bool keyframe) {
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecH264,
                    RTPVideoTypeHeader());
    reference_finder_->ManageFrame(std::move(frame));
  }

  // Check if a frame with picture id |pid| and spatial index |sidx| has been
  // delivered from the packet buffer, and if so, if it has the references
  // specified by |refs|.
  template <typename... T>
  void CheckReferences(int64_t picture_id_offset,
                       uint16_t sidx,
                       T... refs) const {
    int64_t pid = picture_id_offset;
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
  void CheckReferencesH264(int64_t pid, T... refs) const {
    CheckReferences(pid, 0, refs...);
  }

  template <typename... T>
  void RefsToSet(std::set<int64_t>* m, int64_t ref, T... refs) const {
    m->insert(ref);
    RefsToSet(m, refs...);
  }

  void RefsToSet(std::set<int64_t>* m) const {}

  Random rand_;
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
  CheckReferencesGeneric(sn);
  CheckReferencesGeneric(sn + 3, sn + 0);
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

TEST_F(TestRtpFrameReferenceFinder, AdvanceSavedKeyframeBigJump) {
  InsertVp9Flex(0, 0, true);
  InsertVp9Flex(1, 1, true);
  reference_finder_->PaddingReceived(32768);
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
  CheckReferencesVp8(sn + 2);
  CheckReferencesVp8(sn + 4, sn + 2);
  CheckReferencesVp8(sn + 8, sn + 4);
  CheckReferencesVp8(sn + 9, sn + 8);
  CheckReferencesVp8(sn + 11, sn + 9);
  CheckReferencesVp8(sn + 12);
  CheckReferencesVp8(sn + 17, sn + 12);
  CheckReferencesVp8(sn + 18, sn + 17);
  CheckReferencesVp8(sn + 20, sn + 18);
  CheckReferencesVp8(sn + 21, sn + 20);
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
  CheckReferencesVp8(sn + 2);
  CheckReferencesVp8(sn + 4, sn + 2);
  CheckReferencesVp8(sn + 8, sn + 4);
  CheckReferencesVp8(sn + 9, sn + 8);
  CheckReferencesVp8(sn + 11, sn + 9);
  CheckReferencesVp8(sn + 12);
  CheckReferencesVp8(sn + 17, sn + 12);
  CheckReferencesVp8(sn + 18, sn + 17);
  CheckReferencesVp8(sn + 20, sn + 18);
  CheckReferencesVp8(sn + 21, sn + 20);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8KeyFrameReferences) {
  uint16_t sn = Rand();
  InsertVp8(sn, sn, true);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp8(sn);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrame_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrameLayerSync_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 1, true);
  ASSERT_EQ(2UL, frames_from_callback_.size());
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 1, 1, true);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8RepeatedFrame_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 1);
  InsertVp8(sn + 1, sn + 1, false, pid + 1, 0, 2, true);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 0, 3);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 0, 4);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 3, pid + 4);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid + 1);
  CheckReferencesVp8(pid + 3, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 3);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 5);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 5, pid + 6);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid);
  CheckReferencesVp8(pid + 5, pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8, pid + 4);
  CheckReferencesVp8(pid + 9, pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(TestRtpFrameReferenceFinder, Vp8TemporalLayersMissingFrame_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp8(sn, sn, true, pid, 0, 55, false);
  InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, 55, true);
  InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, 55, false);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 2, pid);
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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
  CheckReferencesVp8(pid + 4, pid);
  CheckReferencesVp8(pid + 5, pid + 2, pid + 3, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 2, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 4, pid + 5, pid + 6);
  CheckReferencesVp8(pid + 8, pid + 4);
  CheckReferencesVp8(pid + 9, pid + 8);
  CheckReferencesVp8(pid + 10, pid + 8);
  CheckReferencesVp8(pid + 11, pid + 8, pid + 9, pid + 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8InsertManyFrames_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  const int keyframes_to_insert = 50;
  const int frames_per_keyframe = 120;  // Should be a multiple of 4.
  uint8_t tl0 = 128;

  for (int k = 0; k < keyframes_to_insert; ++k) {
    InsertVp8(sn, sn, true, pid, 0, tl0, false);
    InsertVp8(sn + 1, sn + 1, false, pid + 1, 2, tl0, true);
    InsertVp8(sn + 2, sn + 2, false, pid + 2, 1, tl0, true);
    InsertVp8(sn + 3, sn + 3, false, pid + 3, 2, tl0, false);
    CheckReferencesVp8(pid);
    CheckReferencesVp8(pid + 1, pid);
    CheckReferencesVp8(pid + 2, pid);
    CheckReferencesVp8(pid + 3, pid, pid + 1, pid + 2);
    frames_from_callback_.clear();
    ++tl0;

    for (int f = 4; f < frames_per_keyframe; f += 4) {
      uint16_t sf = sn + f;
      int64_t pidf = pid + f;

      InsertVp8(sf, sf, false, pidf, 0, tl0, false);
      InsertVp8(sf + 1, sf + 1, false, pidf + 1, 2, tl0, false);
      InsertVp8(sf + 2, sf + 2, false, pidf + 2, 1, tl0, false);
      InsertVp8(sf + 3, sf + 3, false, pidf + 3, 2, tl0, false);
      CheckReferencesVp8(pidf, pidf - 4);
      CheckReferencesVp8(pidf + 1, pidf, pidf - 1, pidf - 2);
      CheckReferencesVp8(pidf + 2, pidf, pidf - 2);
      CheckReferencesVp8(pidf + 3, pidf, pidf + 1, pidf + 2);
      frames_from_callback_.clear();
      ++tl0;
    }

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
  CheckReferencesVp8(pid);
  CheckReferencesVp8(pid + 1, pid);
  CheckReferencesVp8(pid + 2, pid);
  CheckReferencesVp8(pid + 4, pid + 2);
  CheckReferencesVp8(pid + 5, pid + 4);
  CheckReferencesVp8(pid + 6, pid + 4);
  CheckReferencesVp8(pid + 7, pid + 6, pid + 5);
}

TEST_F(TestRtpFrameReferenceFinder, Vp8Tl1SyncFrameAfterTl1Frame) {
  InsertVp8(1000, 1000, true, 1, 0, 247, true);
  InsertVp8(1001, 1001, false, 3, 0, 248, false);
  InsertVp8(1002, 1002, false, 4, 1, 248, false);  // Will be dropped
  InsertVp8(1003, 1003, false, 5, 1, 248, true);   // due to this frame.

  ASSERT_EQ(3UL, frames_from_callback_.size());
  CheckReferencesVp8(1);
  CheckReferencesVp8(3, 1);
  CheckReferencesVp8(5, 3);
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

  CheckReferencesVp8(1);
  CheckReferencesVp8(2, 1);
  CheckReferencesVp8(3, 1);
  CheckReferencesVp8(4, 3, 2, 1);

  CheckReferencesVp8(5, 1);
  CheckReferencesVp8(6, 5, 4, 3);
  CheckReferencesVp8(7, 5, 3);
  CheckReferencesVp8(8, 7, 6, 5);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofInsertOneFrame) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);

  CheckReferencesVp9(pid, 0);
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
  CheckReferencesVp9(sn + 2, 0);
  CheckReferencesVp9(sn + 4, 0, sn + 2);
  CheckReferencesVp9(sn + 8, 0, sn + 4);
  CheckReferencesVp9(sn + 9, 0, sn + 8);
  CheckReferencesVp9(sn + 11, 0, sn + 9);
  CheckReferencesVp9(sn + 12, 0);
  CheckReferencesVp9(sn + 17, 0, sn + 12);
  CheckReferencesVp9(sn + 18, 0, sn + 17);
  CheckReferencesVp9(sn + 20, 0, sn + 18);
  CheckReferencesVp9(sn + 21, 0, sn + 20);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid + 1);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 3);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 5);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 7);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 9);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 11);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 13);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 15);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 17);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofSpatialLayers_2) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1, false, true);
  // Not inter_pic_predicted because it's the first frame with this layer.
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 1, 1, 0, 1, false, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 2, 0, 0, 1, false, true);
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 2, 1, 0, 1, false, true);

  ASSERT_EQ(5UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 1, 1);
  CheckReferencesVp9(pid + 2, 0, pid + 1);
  CheckReferencesVp9(pid + 2, 1, pid + 1);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_0) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode1);  // Only 1 spatial layer.

  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 2, false);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid + 1);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 3);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 5);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 7);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 9);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 11);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 13);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 15);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 17);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofSkipFramesTemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 0101 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  // Skip GOF with tl0 1
  InsertVp9Gof(sn + 4, sn + 4, true, pid + 4, 0, 0, 2, false, true, &ss);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 1, 2, false);
  // Skip GOF with tl0 3
  // Skip GOF with tl0 4
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 0, 5, false, true, &ss);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 1, 5, false);

  ASSERT_EQ(6UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 4, 0);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofSkipFramesTemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 02120212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 2, 0, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);

  // Skip frames with tl0 = 1

  InsertVp9Gof(sn + 8, sn + 8, true, pid + 8, 0, 0, 2, false, false, &ss);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 2, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 2, false);

  ASSERT_EQ(8UL, frames_from_callback_.size());
  CheckReferencesVp9(pid + 8, 0);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);

  // Now insert frames with tl0 = 1
  InsertVp9Gof(sn + 4, sn + 4, true, pid + 4, 0, 0, 1, false, true, &ss);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 1, false);

  ASSERT_EQ(9UL, frames_from_callback_.size());
  CheckReferencesVp9(pid + 4, 0);

  // Rest of frames belonging to tl0 = 1
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 1, true);  // up-switch

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 0101 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 10);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 14);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_01) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 10);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 14);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayers_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 4);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 12);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern

  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 1, 0, false);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 4);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 14);
  CheckReferencesVp9(pid + 16, 0, pid + 12);
  CheckReferencesVp9(pid + 17, 0, pid + 16);
  CheckReferencesVp9(pid + 18, 0, pid + 16);
  CheckReferencesVp9(pid + 19, 0, pid + 18);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersUpSwitch_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid);
  CheckReferencesVp9(pid + 5, 0, pid + 3, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 2, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 4);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 11, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 10, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
}

TEST_F(TestRtpFrameReferenceFinder,
       Vp9GofTemporalLayersUpSwitchReordered_02120212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode4);  // 02120212 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 2, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid);
  CheckReferencesVp9(pid + 5, 0, pid + 3, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 2, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 4);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 9, pid + 10);
  CheckReferencesVp9(pid + 12, 0, pid + 8);
  CheckReferencesVp9(pid + 13, 0, pid + 11, pid + 12);
  CheckReferencesVp9(pid + 14, 0, pid + 10, pid + 12);
  CheckReferencesVp9(pid + 15, 0, pid + 13, pid + 14);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTemporalLayersReordered_01_0212) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);  // 01 pattern

  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 1, 0, false);
  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 3, sn + 3, false, pid + 3, 0, 1, 1, false);
  InsertVp9Gof(sn + 6, sn + 6, false, pid + 6, 0, 1, 2, false);
  ss.SetGofInfoVP9(kTemporalStructureMode3);  // 0212 pattern
  InsertVp9Gof(sn + 4, sn + 4, false, pid + 4, 0, 0, 2, false, true, &ss);
  InsertVp9Gof(sn + 2, sn + 2, false, pid + 2, 0, 0, 1, false);
  InsertVp9Gof(sn + 5, sn + 5, false, pid + 5, 0, 2, 2, false);
  InsertVp9Gof(sn + 8, sn + 8, false, pid + 8, 0, 0, 3, false);
  InsertVp9Gof(sn + 10, sn + 10, false, pid + 10, 0, 1, 3, false);
  InsertVp9Gof(sn + 7, sn + 7, false, pid + 7, 0, 2, 2, false);
  InsertVp9Gof(sn + 11, sn + 11, false, pid + 11, 0, 2, 3, false);
  InsertVp9Gof(sn + 9, sn + 9, false, pid + 9, 0, 2, 3, false);

  ASSERT_EQ(12UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 3, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid);
  CheckReferencesVp9(pid + 5, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 7, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 4);
  CheckReferencesVp9(pid + 9, 0, pid + 8);
  CheckReferencesVp9(pid + 10, 0, pid + 8);
  CheckReferencesVp9(pid + 11, 0, pid + 10);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9FlexibleModeOneFrame) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();

  InsertVp9Flex(sn, sn, true, pid, 0, 0, false);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid, 1);
  CheckReferencesVp9(pid + 1, 1, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 2, 1, pid + 1);
  CheckReferencesVp9(pid + 3, 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 1, pid + 3);
  CheckReferencesVp9(pid + 5, 1, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 1, pid + 5);
  CheckReferencesVp9(pid + 7, 1, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 1, pid + 7);
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
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid, 1);
  CheckReferencesVp9(pid + 1, 1, pid);
  CheckReferencesVp9(pid + 2, 0, pid);
  CheckReferencesVp9(pid + 2, 1, pid + 1);
  CheckReferencesVp9(pid + 3, 1, pid + 2);
  CheckReferencesVp9(pid + 4, 0, pid + 2);
  CheckReferencesVp9(pid + 4, 1, pid + 3);
  CheckReferencesVp9(pid + 5, 1, pid + 4);
  CheckReferencesVp9(pid + 6, 0, pid + 4);
  CheckReferencesVp9(pid + 6, 1, pid + 5);
  CheckReferencesVp9(pid + 7, 1, pid + 6);
  CheckReferencesVp9(pid + 8, 0, pid + 6);
  CheckReferencesVp9(pid + 8, 1, pid + 7);
}

TEST_F(TestRtpFrameReferenceFinder, WrappingFlexReference) {
  InsertVp9Flex(0, 0, false, 0, 0, 0, false, {1});

  ASSERT_EQ(1UL, frames_from_callback_.size());
  const EncodedFrame& frame = *frames_from_callback_.begin()->second;
  ASSERT_EQ(frame.id.picture_id - frame.references[0], 1);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofPidJump) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1000, 0, 0, 1);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTl0Jump) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode3);

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 125, true, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 0, false, true, &ss);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofTidTooHigh) {
  // Same as RtpFrameReferenceFinder::kMaxTemporalLayers.
  const int kMaxTemporalLayers = 5;
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.SetGofInfoVP9(kTemporalStructureMode2);
  ss.temporal_idx[1] = kMaxTemporalLayers;

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
}

TEST_F(TestRtpFrameReferenceFinder, Vp9GofZeroFrames) {
  uint16_t pid = Rand();
  uint16_t sn = Rand();
  GofInfoVP9 ss;
  ss.num_frames_in_gof = 0;

  InsertVp9Gof(sn, sn, true, pid, 0, 0, 0, false, false, &ss);
  InsertVp9Gof(sn + 1, sn + 1, false, pid + 1, 0, 0, 1);

  ASSERT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesVp9(pid, 0);
  CheckReferencesVp9(pid + 1, 0, pid);
}

TEST_F(TestRtpFrameReferenceFinder, H264KeyFrameReferences) {
  uint16_t sn = Rand();
  InsertH264(sn, sn, true);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
}

TEST_F(TestRtpFrameReferenceFinder, H264SequenceNumberWrap) {
  uint16_t sn = 0xFFFF;

  InsertH264(sn - 1, sn - 1, true);
  InsertH264(sn, sn, false);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 2, sn + 2, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn - 1);
  CheckReferencesH264(sn, sn - 1);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
}

TEST_F(TestRtpFrameReferenceFinder, H264Frames) {
  uint16_t sn = Rand();

  InsertH264(sn, sn, true);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 2, sn + 2, false);
  InsertH264(sn + 3, sn + 3, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
  CheckReferencesH264(sn + 3, sn + 2);
}

TEST_F(TestRtpFrameReferenceFinder, H264Reordering) {
  uint16_t sn = Rand();

  InsertH264(sn, sn, true);
  InsertH264(sn + 1, sn + 1, false);
  InsertH264(sn + 3, sn + 3, false);
  InsertH264(sn + 2, sn + 2, false);
  InsertH264(sn + 5, sn + 5, false);
  InsertH264(sn + 6, sn + 6, false);
  InsertH264(sn + 4, sn + 4, false);

  ASSERT_EQ(7UL, frames_from_callback_.size());
  CheckReferencesH264(sn);
  CheckReferencesH264(sn + 1, sn);
  CheckReferencesH264(sn + 2, sn + 1);
  CheckReferencesH264(sn + 3, sn + 2);
  CheckReferencesH264(sn + 4, sn + 3);
  CheckReferencesH264(sn + 5, sn + 4);
  CheckReferencesH264(sn + 6, sn + 5);
}

TEST_F(TestRtpFrameReferenceFinder, H264SequenceNumberWrapMulti) {
  uint16_t sn = 0xFFFF;

  InsertH264(sn - 3, sn - 2, true);
  InsertH264(sn - 1, sn + 1, false);
  InsertH264(sn + 2, sn + 3, false);
  InsertH264(sn + 4, sn + 7, false);

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckReferencesH264(sn - 2);
  CheckReferencesH264(sn + 1, sn - 2);
  CheckReferencesH264(sn + 3, sn + 1);
  CheckReferencesH264(sn + 7, sn + 3);
}

}  // namespace video_coding
}  // namespace webrtc
