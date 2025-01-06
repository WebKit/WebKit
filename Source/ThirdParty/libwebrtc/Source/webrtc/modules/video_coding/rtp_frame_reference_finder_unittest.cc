/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_frame_reference_finder.h"

#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/random.h"
#include "rtc_base/ref_count.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

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
      /*color_space=*/std::nullopt,
      /*frame_instrumentation_data=*/std::nullopt,
      RtpPacketInfos(),
      EncodedImageBuffer::Create(/*size=*/0));
  // clang-format on
}
}  // namespace

class TestRtpFrameReferenceFinder : public ::testing::Test {
 protected:
  TestRtpFrameReferenceFinder()
      : rand_(0x8739211),
        reference_finder_(std::make_unique<RtpFrameReferenceFinder>()),
        frames_from_callback_(FrameComp()) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnCompleteFrames(RtpFrameReferenceFinder::ReturnVector frames) {
    for (auto& frame : frames) {
      int64_t pid = frame->Id();
      uint16_t sidx = *frame->SpatialIndex();
      auto frame_it = frames_from_callback_.find(std::make_pair(pid, sidx));
      if (frame_it != frames_from_callback_.end()) {
        ADD_FAILURE() << "Already received frame with (pid:sidx): (" << pid
                      << ":" << sidx << ")";
        return;
      }

      frames_from_callback_.insert(
          std::make_pair(std::make_pair(pid, sidx), std::move(frame)));
    }
  }

  void InsertGeneric(uint16_t seq_num_start,
                     uint16_t seq_num_end,
                     bool keyframe) {
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecGeneric,
                    RTPVideoTypeHeader());

    OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
  }

  void InsertH264(uint16_t seq_num_start, uint16_t seq_num_end, bool keyframe) {
    std::unique_ptr<RtpFrameObject> frame =
        CreateFrame(seq_num_start, seq_num_end, keyframe, kVideoCodecH264,
                    RTPVideoTypeHeader());
    OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));
  }

  void InsertPadding(uint16_t seq_num) {
    OnCompleteFrames(reference_finder_->PaddingReceived(seq_num));
  }

  // Check if a frame with picture id `pid` and spatial index `sidx` has been
  // delivered from the packet buffer, and if so, if it has the references
  // specified by `refs`.
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
  InsertPadding(sn + 1);
  EXPECT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReordered) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertPadding(sn + 1);
  InsertPadding(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckReferencesGeneric(sn);
  CheckReferencesGeneric(sn + 3, sn + 0);
}

TEST_F(TestRtpFrameReferenceFinder, PaddingPacketsReorderedMultipleKeyframes) {
  uint16_t sn = Rand();

  InsertGeneric(sn, sn, true);
  InsertPadding(sn + 1);
  InsertPadding(sn + 4);
  InsertGeneric(sn + 2, sn + 3, false);
  InsertGeneric(sn + 5, sn + 5, true);
  InsertPadding(sn + 6);
  InsertPadding(sn + 9);
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

TEST_F(TestRtpFrameReferenceFinder, Av1FrameNoDependencyDescriptor) {
  uint16_t sn = 0xFFFF;
  std::unique_ptr<RtpFrameObject> frame =
      CreateFrame(/*seq_num_start=*/sn, /*seq_num_end=*/sn, /*keyframe=*/true,
                  kVideoCodecAV1, RTPVideoTypeHeader());

  OnCompleteFrames(reference_finder_->ManageFrame(std::move(frame)));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckReferencesGeneric(sn);
}

}  // namespace webrtc
