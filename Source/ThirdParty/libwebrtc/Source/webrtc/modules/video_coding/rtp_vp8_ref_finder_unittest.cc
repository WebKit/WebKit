/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_vp8_ref_finder.h"

#include <optional>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/frame_object.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Matches;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

namespace webrtc {
namespace {

MATCHER_P2(HasIdAndRefs, id, refs, "") {
  return Matches(Eq(id))(arg->Id()) &&
         Matches(UnorderedElementsAreArray(refs))(
             rtc::ArrayView<int64_t>(arg->references, arg->num_references));
}

Matcher<const std::vector<std::unique_ptr<EncodedFrame>>&>
HasFrameWithIdAndRefs(int64_t frame_id, const std::vector<int64_t>& refs) {
  return Contains(HasIdAndRefs(frame_id, refs));
}

class Frame {
 public:
  Frame& AsKeyFrame(bool is_keyframe = true) {
    is_keyframe_ = is_keyframe;
    return *this;
  }

  Frame& Pid(int pid) {
    picture_id_ = pid;
    return *this;
  }

  Frame& Tid(int tid) {
    temporal_id_ = tid;
    return *this;
  }

  Frame& Tl0(int tl0) {
    tl0_idx_ = tl0;
    return *this;
  }

  Frame& AsSync(bool is_sync = true) {
    sync = is_sync;
    return *this;
  }

  operator std::unique_ptr<RtpFrameObject>() {
    RTPVideoHeaderVP8 vp8_header{};
    vp8_header.pictureId = *picture_id_;
    vp8_header.temporalIdx = *temporal_id_;
    vp8_header.tl0PicIdx = *tl0_idx_;
    vp8_header.layerSync = sync;

    RTPVideoHeader video_header;
    video_header.frame_type = is_keyframe_ ? VideoFrameType::kVideoFrameKey
                                           : VideoFrameType::kVideoFrameDelta;
    video_header.video_type_header = vp8_header;
    // clang-format off
    return std::make_unique<RtpFrameObject>(
        /*seq_num_start=*/0,
        /*seq_num_end=*/0,
        /*markerBit=*/true,
        /*times_nacked=*/0,
        /*first_packet_received_time=*/0,
        /*last_packet_received_time=*/0,
        /*rtp_timestamp=*/0,
        /*ntp_time_ms=*/0,
        VideoSendTiming(),
        /*payload_type=*/0,
        kVideoCodecVP8,
        kVideoRotation_0,
        VideoContentType::UNSPECIFIED,
        video_header,
        /*color_space=*/std::nullopt,
        /*frame_instrumentation_data=*/std::nullopt,
        RtpPacketInfos(),
        EncodedImageBuffer::Create(/*size=*/0));
    // clang-format on
  }

 private:
  bool is_keyframe_ = false;
  std::optional<int> picture_id_;
  std::optional<int> temporal_id_;
  std::optional<int> tl0_idx_;
  bool sync = false;
};

}  // namespace

class RtpVp8RefFinderTest : public ::testing::Test {
 protected:
  RtpVp8RefFinderTest() : ref_finder_(std::make_unique<RtpVp8RefFinder>()) {}

  void Insert(std::unique_ptr<RtpFrameObject> frame) {
    for (auto& f : ref_finder_->ManageFrame(std::move(frame))) {
      frames_.push_back(std::move(f));
    }
  }

  std::unique_ptr<RtpVp8RefFinder> ref_finder_;
  std::vector<std::unique_ptr<EncodedFrame>> frames_;
};

TEST_F(RtpVp8RefFinderTest, Vp8RepeatedFrame_0) {
  Insert(Frame().Pid(0).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(0).Tl0(2));
  Insert(Frame().Pid(1).Tid(0).Tl0(2));

  EXPECT_THAT(frames_, SizeIs(2));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
}

TEST_F(RtpVp8RefFinderTest, Vp8RepeatedFrameLayerSync_01) {
  Insert(Frame().Pid(0).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(1).Tl0(1).AsSync());
  Insert(Frame().Pid(1).Tid(1).Tl0(1).AsSync());

  EXPECT_THAT(frames_, SizeIs(2));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
}

TEST_F(RtpVp8RefFinderTest, Vp8RepeatedFrame_01) {
  Insert(Frame().Pid(0).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(0).Tl0(2).AsSync());
  Insert(Frame().Pid(2).Tid(0).Tl0(3));
  Insert(Frame().Pid(3).Tid(0).Tl0(4));
  Insert(Frame().Pid(3).Tid(0).Tl0(4));

  EXPECT_THAT(frames_, SizeIs(4));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {2}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayers_0) {
  Insert(Frame().Pid(0).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(0).Tl0(2));

  EXPECT_THAT(frames_, SizeIs(2));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
}

TEST_F(RtpVp8RefFinderTest, Vp8DuplicateTl1Frames) {
  Insert(Frame().Pid(0).Tid(0).Tl0(0).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(1).Tl0(0).AsSync());
  Insert(Frame().Pid(2).Tid(0).Tl0(1));
  Insert(Frame().Pid(3).Tid(1).Tl0(1));
  Insert(Frame().Pid(3).Tid(1).Tl0(1));
  Insert(Frame().Pid(4).Tid(0).Tl0(2));
  Insert(Frame().Pid(5).Tid(1).Tl0(2));

  EXPECT_THAT(frames_, SizeIs(6));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {1, 2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {3, 4}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayersReordering_0) {
  Insert(Frame().Pid(1).Tid(0).Tl0(2));
  Insert(Frame().Pid(0).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(3).Tid(0).Tl0(4));
  Insert(Frame().Pid(2).Tid(0).Tl0(3));
  Insert(Frame().Pid(5).Tid(0).Tl0(6));
  Insert(Frame().Pid(6).Tid(0).Tl0(7));
  Insert(Frame().Pid(4).Tid(0).Tl0(5));

  EXPECT_THAT(frames_, SizeIs(7));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {3}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(6, {5}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayers_01) {
  Insert(Frame().Pid(0).Tid(0).Tl0(255).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(1).Tl0(255).AsSync());
  Insert(Frame().Pid(2).Tid(0).Tl0(0));
  Insert(Frame().Pid(3).Tid(1).Tl0(0));

  EXPECT_THAT(frames_, SizeIs(4));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {1, 2}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayersReordering_01) {
  Insert(Frame().Pid(1).Tid(1).Tl0(255).AsSync());
  Insert(Frame().Pid(0).Tid(0).Tl0(255).AsKeyFrame());
  Insert(Frame().Pid(3).Tid(1).Tl0(0));
  Insert(Frame().Pid(5).Tid(1).Tl0(1));
  Insert(Frame().Pid(2).Tid(0).Tl0(0));
  Insert(Frame().Pid(4).Tid(0).Tl0(1));
  Insert(Frame().Pid(6).Tid(0).Tl0(2));
  Insert(Frame().Pid(7).Tid(1).Tl0(2));

  EXPECT_THAT(frames_, SizeIs(8));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {1, 2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {3, 4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(6, {4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(7, {5, 6}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayers_0212) {
  Insert(Frame().Pid(0).Tid(0).Tl0(55).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(2).Tl0(55).AsSync());
  Insert(Frame().Pid(2).Tid(1).Tl0(55).AsSync());
  Insert(Frame().Pid(3).Tid(2).Tl0(55));
  Insert(Frame().Pid(4).Tid(0).Tl0(56));
  Insert(Frame().Pid(5).Tid(2).Tl0(56));
  Insert(Frame().Pid(6).Tid(1).Tl0(56));
  Insert(Frame().Pid(7).Tid(2).Tl0(56));
  Insert(Frame().Pid(8).Tid(0).Tl0(57));
  Insert(Frame().Pid(9).Tid(2).Tl0(57).AsSync());
  Insert(Frame().Pid(10).Tid(1).Tl0(57).AsSync());
  Insert(Frame().Pid(11).Tid(2).Tl0(57));

  EXPECT_THAT(frames_, SizeIs(12));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {0, 1, 2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {2, 3, 4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(6, {2, 4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(7, {4, 5, 6}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(8, {4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(9, {8}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(10, {8}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(11, {8, 9, 10}));
}

TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayersMissingFrame_0212) {
  Insert(Frame().Pid(0).Tid(0).Tl0(55).AsKeyFrame());
  Insert(Frame().Pid(2).Tid(1).Tl0(55).AsSync());
  Insert(Frame().Pid(3).Tid(2).Tl0(55));

  EXPECT_THAT(frames_, SizeIs(2));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
}

// Test with 3 temporal layers in a 0212 pattern.
TEST_F(RtpVp8RefFinderTest, Vp8TemporalLayersReordering_0212) {
  Insert(Frame().Pid(127).Tid(2).Tl0(55).AsSync());
  Insert(Frame().Pid(126).Tid(0).Tl0(55).AsKeyFrame());
  Insert(Frame().Pid(128).Tid(1).Tl0(55).AsSync());
  Insert(Frame().Pid(130).Tid(0).Tl0(56));
  Insert(Frame().Pid(131).Tid(2).Tl0(56));
  Insert(Frame().Pid(129).Tid(2).Tl0(55));
  Insert(Frame().Pid(133).Tid(2).Tl0(56));
  Insert(Frame().Pid(135).Tid(2).Tl0(57).AsSync());
  Insert(Frame().Pid(132).Tid(1).Tl0(56));
  Insert(Frame().Pid(134).Tid(0).Tl0(57));
  Insert(Frame().Pid(137).Tid(2).Tl0(57));
  Insert(Frame().Pid(136).Tid(1).Tl0(57).AsSync());

  EXPECT_THAT(frames_, SizeIs(12));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(126, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(127, {126}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(128, {126}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(129, {126, 127, 128}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(130, {126}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(131, {128, 129, 130}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(132, {128, 130}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(133, {130, 131, 132}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(134, {130}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(135, {134}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(136, {134}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(137, {134, 135, 136}));
}

TEST_F(RtpVp8RefFinderTest, Vp8LayerSync) {
  Insert(Frame().Pid(0).Tid(0).Tl0(0).AsKeyFrame());
  Insert(Frame().Pid(1).Tid(1).Tl0(0).AsSync());
  Insert(Frame().Pid(2).Tid(0).Tl0(1));
  Insert(Frame().Pid(4).Tid(0).Tl0(2));
  Insert(Frame().Pid(5).Tid(1).Tl0(2).AsSync());
  Insert(Frame().Pid(6).Tid(0).Tl0(3));
  Insert(Frame().Pid(7).Tid(1).Tl0(3));

  EXPECT_THAT(frames_, SizeIs(7));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(0, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {0}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {2}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(6, {4}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(7, {5, 6}));
}

TEST_F(RtpVp8RefFinderTest, Vp8Tl1SyncFrameAfterTl1Frame) {
  Insert(Frame().Pid(1).Tid(0).Tl0(247).AsKeyFrame().AsSync());
  Insert(Frame().Pid(3).Tid(0).Tl0(248));
  Insert(Frame().Pid(4).Tid(1).Tl0(248));
  Insert(Frame().Pid(5).Tid(1).Tl0(248).AsSync());

  EXPECT_THAT(frames_, SizeIs(3));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {3}));
}

TEST_F(RtpVp8RefFinderTest, Vp8DetectMissingFrame_0212) {
  Insert(Frame().Pid(1).Tid(0).Tl0(1).AsKeyFrame());
  Insert(Frame().Pid(2).Tid(2).Tl0(1).AsSync());
  Insert(Frame().Pid(3).Tid(1).Tl0(1).AsSync());
  Insert(Frame().Pid(4).Tid(2).Tl0(1));
  Insert(Frame().Pid(6).Tid(2).Tl0(2));
  Insert(Frame().Pid(7).Tid(1).Tl0(2));
  Insert(Frame().Pid(8).Tid(2).Tl0(2));
  Insert(Frame().Pid(5).Tid(0).Tl0(2));

  EXPECT_THAT(frames_, SizeIs(8));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(1, {}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(2, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(3, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(4, {1, 2, 3}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(5, {1}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(6, {3, 4, 5}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(7, {3, 5}));
  EXPECT_THAT(frames_, HasFrameWithIdAndRefs(8, {5, 6, 7}));
}

TEST_F(RtpVp8RefFinderTest, StashedFramesDoNotWrapTl0Backwards) {
  Insert(Frame().Pid(0).Tid(0).Tl0(0));
  EXPECT_THAT(frames_, SizeIs(0));

  Insert(Frame().Pid(128).Tid(0).Tl0(128).AsKeyFrame());
  EXPECT_THAT(frames_, SizeIs(1));
  Insert(Frame().Pid(129).Tid(0).Tl0(129));
  EXPECT_THAT(frames_, SizeIs(2));
}

}  // namespace webrtc
