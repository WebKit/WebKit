/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/video_broadcaster.h"

#include <limits>

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_source_interface.h"
#include "media/base/fake_video_renderer.h"
#include "test/gmock.h"
#include "test/gtest.h"

using cricket::FakeVideoRenderer;
using rtc::VideoBroadcaster;
using rtc::VideoSinkWants;
using FrameSize = rtc::VideoSinkWants::FrameSize;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Mock;
using ::testing::Optional;

class MockSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  void OnFrame(const webrtc::VideoFrame&) override {}

  MOCK_METHOD(void,
              OnConstraintsChanged,
              (const webrtc::VideoTrackSourceConstraints& constraints),
              (override));
};

TEST(VideoBroadcasterTest, frame_wanted) {
  VideoBroadcaster broadcaster;
  EXPECT_FALSE(broadcaster.frame_wanted());

  FakeVideoRenderer sink;
  broadcaster.AddOrUpdateSink(&sink, rtc::VideoSinkWants());
  EXPECT_TRUE(broadcaster.frame_wanted());

  broadcaster.RemoveSink(&sink);
  EXPECT_FALSE(broadcaster.frame_wanted());
}

TEST(VideoBroadcasterTest, OnFrame) {
  VideoBroadcaster broadcaster;

  FakeVideoRenderer sink1;
  FakeVideoRenderer sink2;
  broadcaster.AddOrUpdateSink(&sink1, rtc::VideoSinkWants());
  broadcaster.AddOrUpdateSink(&sink2, rtc::VideoSinkWants());
  static int kWidth = 100;
  static int kHeight = 50;

  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      webrtc::I420Buffer::Create(kWidth, kHeight));
  // Initialize, to avoid warnings on use of initialized values.
  webrtc::I420Buffer::SetBlack(buffer.get());

  webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                                 .set_video_frame_buffer(buffer)
                                 .set_rotation(webrtc::kVideoRotation_0)
                                 .set_timestamp_us(0)
                                 .build();

  broadcaster.OnFrame(frame);
  EXPECT_EQ(1, sink1.num_rendered_frames());
  EXPECT_EQ(1, sink2.num_rendered_frames());

  broadcaster.RemoveSink(&sink1);
  broadcaster.OnFrame(frame);
  EXPECT_EQ(1, sink1.num_rendered_frames());
  EXPECT_EQ(2, sink2.num_rendered_frames());

  broadcaster.AddOrUpdateSink(&sink1, rtc::VideoSinkWants());
  broadcaster.OnFrame(frame);
  EXPECT_EQ(2, sink1.num_rendered_frames());
  EXPECT_EQ(3, sink2.num_rendered_frames());
}

TEST(VideoBroadcasterTest, AppliesRotationIfAnySinkWantsRotationApplied) {
  VideoBroadcaster broadcaster;
  EXPECT_FALSE(broadcaster.wants().rotation_applied);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.rotation_applied = false;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_FALSE(broadcaster.wants().rotation_applied);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.rotation_applied = true;

  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_TRUE(broadcaster.wants().rotation_applied);

  broadcaster.RemoveSink(&sink2);
  EXPECT_FALSE(broadcaster.wants().rotation_applied);
}

TEST(VideoBroadcasterTest, AppliesMinOfSinkWantsMaxPixelCount) {
  VideoBroadcaster broadcaster;
  EXPECT_EQ(std::numeric_limits<int>::max(),
            broadcaster.wants().max_pixel_count);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.max_pixel_count = 1280 * 720;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(1280 * 720, broadcaster.wants().max_pixel_count);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.max_pixel_count = 640 * 360;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(640 * 360, broadcaster.wants().max_pixel_count);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(1280 * 720, broadcaster.wants().max_pixel_count);
}

TEST(VideoBroadcasterTest, AppliesMinOfSinkWantsMaxAndTargetPixelCount) {
  VideoBroadcaster broadcaster;
  EXPECT_TRUE(!broadcaster.wants().target_pixel_count);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.target_pixel_count = 1280 * 720;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().target_pixel_count);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.target_pixel_count = 640 * 360;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(640 * 360, *broadcaster.wants().target_pixel_count);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(1280 * 720, *broadcaster.wants().target_pixel_count);
}

TEST(VideoBroadcasterTest, AppliesMinOfSinkWantsMaxFramerate) {
  VideoBroadcaster broadcaster;
  EXPECT_EQ(std::numeric_limits<int>::max(),
            broadcaster.wants().max_framerate_fps);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.max_framerate_fps = 30;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(30, broadcaster.wants().max_framerate_fps);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.max_framerate_fps = 15;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(15, broadcaster.wants().max_framerate_fps);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(30, broadcaster.wants().max_framerate_fps);
}

TEST(VideoBroadcasterTest,
     AppliesLeastCommonMultipleOfSinkWantsResolutionAlignment) {
  VideoBroadcaster broadcaster;
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 1);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.resolution_alignment = 2;
  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 2);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.resolution_alignment = 3;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 6);

  FakeVideoRenderer sink3;
  VideoSinkWants wants3;
  wants3.resolution_alignment = 4;
  broadcaster.AddOrUpdateSink(&sink3, wants3);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 12);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 4);
}

TEST(VideoBroadcasterTest, SinkWantsBlackFrames) {
  VideoBroadcaster broadcaster;
  EXPECT_TRUE(!broadcaster.wants().black_frames);

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.black_frames = true;
  broadcaster.AddOrUpdateSink(&sink1, wants1);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.black_frames = false;
  broadcaster.AddOrUpdateSink(&sink2, wants2);

  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      webrtc::I420Buffer::Create(100, 200));
  // Makes it not all black.
  buffer->InitializeData();

  webrtc::VideoFrame frame1 = webrtc::VideoFrame::Builder()
                                  .set_video_frame_buffer(buffer)
                                  .set_rotation(webrtc::kVideoRotation_0)
                                  .set_timestamp_us(10)
                                  .build();
  broadcaster.OnFrame(frame1);
  EXPECT_TRUE(sink1.black_frame());
  EXPECT_EQ(10, sink1.timestamp_us());
  EXPECT_FALSE(sink2.black_frame());
  EXPECT_EQ(10, sink2.timestamp_us());

  // Switch the sink wants.
  wants1.black_frames = false;
  broadcaster.AddOrUpdateSink(&sink1, wants1);
  wants2.black_frames = true;
  broadcaster.AddOrUpdateSink(&sink2, wants2);

  webrtc::VideoFrame frame2 = webrtc::VideoFrame::Builder()
                                  .set_video_frame_buffer(buffer)
                                  .set_rotation(webrtc::kVideoRotation_0)
                                  .set_timestamp_us(30)
                                  .build();
  broadcaster.OnFrame(frame2);
  EXPECT_FALSE(sink1.black_frame());
  EXPECT_EQ(30, sink1.timestamp_us());
  EXPECT_TRUE(sink2.black_frame());
  EXPECT_EQ(30, sink2.timestamp_us());
}

TEST(VideoBroadcasterTest, ConstraintsChangedNotCalledOnSinkAddition) {
  MockSink sink;
  VideoBroadcaster broadcaster;
  EXPECT_CALL(sink, OnConstraintsChanged).Times(0);
  broadcaster.AddOrUpdateSink(&sink, VideoSinkWants());
}

TEST(VideoBroadcasterTest, ForwardsLastConstraintsOnAdd) {
  MockSink sink;
  VideoBroadcaster broadcaster;
  broadcaster.ProcessConstraints(webrtc::VideoTrackSourceConstraints{2, 3});
  broadcaster.ProcessConstraints(webrtc::VideoTrackSourceConstraints{1, 4});
  EXPECT_CALL(
      sink,
      OnConstraintsChanged(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(1)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps, Optional(4)))));
  broadcaster.AddOrUpdateSink(&sink, VideoSinkWants());
}

TEST(VideoBroadcasterTest, UpdatesOnlyNewSinksWithConstraints) {
  MockSink sink1;
  VideoBroadcaster broadcaster;
  broadcaster.AddOrUpdateSink(&sink1, VideoSinkWants());
  broadcaster.ProcessConstraints(webrtc::VideoTrackSourceConstraints{1, 4});
  Mock::VerifyAndClearExpectations(&sink1);
  EXPECT_CALL(sink1, OnConstraintsChanged).Times(0);
  MockSink sink2;
  EXPECT_CALL(
      sink2,
      OnConstraintsChanged(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(1)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps, Optional(4)))));
  broadcaster.AddOrUpdateSink(&sink2, VideoSinkWants());
}

TEST(VideoBroadcasterTest, ForwardsConstraintsToSink) {
  MockSink sink;
  VideoBroadcaster broadcaster;
  EXPECT_CALL(sink, OnConstraintsChanged).Times(0);
  broadcaster.AddOrUpdateSink(&sink, VideoSinkWants());
  Mock::VerifyAndClearExpectations(&sink);

  EXPECT_CALL(sink, OnConstraintsChanged(AllOf(
                        Field(&webrtc::VideoTrackSourceConstraints::min_fps,
                              Eq(absl::nullopt)),
                        Field(&webrtc::VideoTrackSourceConstraints::max_fps,
                              Eq(absl::nullopt)))));
  broadcaster.ProcessConstraints(
      webrtc::VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  Mock::VerifyAndClearExpectations(&sink);

  EXPECT_CALL(
      sink,
      OnConstraintsChanged(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps,
                Eq(absl::nullopt)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps, Optional(3)))));
  broadcaster.ProcessConstraints(
      webrtc::VideoTrackSourceConstraints{absl::nullopt, 3});
  Mock::VerifyAndClearExpectations(&sink);

  EXPECT_CALL(
      sink,
      OnConstraintsChanged(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(2)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps,
                Eq(absl::nullopt)))));
  broadcaster.ProcessConstraints(
      webrtc::VideoTrackSourceConstraints{2, absl::nullopt});
  Mock::VerifyAndClearExpectations(&sink);

  EXPECT_CALL(
      sink,
      OnConstraintsChanged(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(2)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps, Optional(3)))));
  broadcaster.ProcessConstraints(webrtc::VideoTrackSourceConstraints{2, 3});
}

TEST(VideoBroadcasterTest, AppliesMaxOfSinkWantsRequestedResolution) {
  VideoBroadcaster broadcaster;

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.is_active = true;
  wants1.requested_resolution = FrameSize(640, 360);

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(FrameSize(640, 360), *broadcaster.wants().requested_resolution);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.is_active = true;
  wants2.requested_resolution = FrameSize(650, 350);
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(FrameSize(650, 360), *broadcaster.wants().requested_resolution);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(FrameSize(640, 360), *broadcaster.wants().requested_resolution);
}

TEST(VideoBroadcasterTest, AnyActive) {
  VideoBroadcaster broadcaster;

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.is_active = false;

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(false, broadcaster.wants().is_active);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.is_active = true;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(true, broadcaster.wants().is_active);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(false, broadcaster.wants().is_active);
}

TEST(VideoBroadcasterTest, AnyActiveWithoutRequestedResolution) {
  VideoBroadcaster broadcaster;

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.is_active = true;
  wants1.requested_resolution = FrameSize(640, 360);

  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(
      false,
      broadcaster.wants().aggregates->any_active_without_requested_resolution);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.is_active = true;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(
      true,
      broadcaster.wants().aggregates->any_active_without_requested_resolution);

  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(
      false,
      broadcaster.wants().aggregates->any_active_without_requested_resolution);
}

// This verifies that the VideoSinkWants from a Sink that is_active = false
// is ignored IF there is an active sink using new api (Requested_Resolution).
// The uses resolution_alignment for verification.
TEST(VideoBroadcasterTest, IgnoreInactiveSinkIfNewApiUsed) {
  VideoBroadcaster broadcaster;

  FakeVideoRenderer sink1;
  VideoSinkWants wants1;
  wants1.is_active = true;
  wants1.requested_resolution = FrameSize(640, 360);
  wants1.resolution_alignment = 2;
  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 2);

  FakeVideoRenderer sink2;
  VideoSinkWants wants2;
  wants2.is_active = true;
  wants2.resolution_alignment = 8;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 8);

  // Now wants2 will be ignored.
  wants2.is_active = false;
  broadcaster.AddOrUpdateSink(&sink2, wants2);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 2);

  // But when wants1 is inactive, wants2 matters again.
  wants1.is_active = false;
  broadcaster.AddOrUpdateSink(&sink1, wants1);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 8);

  // inactive wants1 (new api) is always ignored.
  broadcaster.RemoveSink(&sink2);
  EXPECT_EQ(broadcaster.wants().resolution_alignment, 1);
}
