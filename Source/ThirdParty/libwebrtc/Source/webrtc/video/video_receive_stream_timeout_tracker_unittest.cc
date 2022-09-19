/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream_timeout_tracker.h"

#include <utility>
#include <vector>

#include "api/task_queue/task_queue_base.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
constexpr VideoReceiveStreamTimeoutTracker::Timeouts config = {
    kMaxWaitForKeyframe, kMaxWaitForFrame};
}  // namespace

class VideoReceiveStreamTimeoutTrackerTest : public ::testing::Test {
 public:
  VideoReceiveStreamTimeoutTrackerTest()
      : time_controller_(Timestamp::Millis(2000)),
        timeout_tracker_(time_controller_.GetClock(),
                         time_controller_.GetMainThread(),
                         config,
                         [this](TimeDelta delay) { OnTimeout(delay); }) {}

 protected:
  GlobalSimulatedTimeController time_controller_;
  VideoReceiveStreamTimeoutTracker timeout_tracker_;
  std::vector<TimeDelta> timeouts_;

 private:
  void OnTimeout(TimeDelta delay) { timeouts_.push_back(delay); }
};

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutAfterInitialPeriod) {
  timeout_tracker_.Start(true);
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_THAT(timeouts_, testing::ElementsAre(kMaxWaitForKeyframe));
  timeout_tracker_.Stop();
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, NoTimeoutAfterStop) {
  timeout_tracker_.Start(true);
  time_controller_.AdvanceTime(kMaxWaitForKeyframe / 2);
  timeout_tracker_.Stop();
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_THAT(timeouts_, testing::IsEmpty());
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutForDeltaFrame) {
  timeout_tracker_.Start(true);
  time_controller_.AdvanceTime(TimeDelta::Millis(5));
  timeout_tracker_.OnEncodedFrameReleased();
  time_controller_.AdvanceTime(kMaxWaitForFrame);
  EXPECT_THAT(timeouts_, testing::ElementsAre(kMaxWaitForFrame));
  timeout_tracker_.Stop();
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TimeoutForKeyframeWhenForced) {
  timeout_tracker_.Start(true);
  time_controller_.AdvanceTime(TimeDelta::Millis(5));
  timeout_tracker_.OnEncodedFrameReleased();
  timeout_tracker_.SetWaitingForKeyframe();
  time_controller_.AdvanceTime(kMaxWaitForKeyframe);
  EXPECT_THAT(timeouts_, testing::ElementsAre(kMaxWaitForKeyframe));
  timeout_tracker_.Stop();
}

TEST_F(VideoReceiveStreamTimeoutTrackerTest, TotalTimeoutUsedInCallback) {
  timeout_tracker_.Start(true);
  time_controller_.AdvanceTime(kMaxWaitForKeyframe * 2);
  timeout_tracker_.OnEncodedFrameReleased();
  time_controller_.AdvanceTime(kMaxWaitForFrame * 2);
  EXPECT_THAT(timeouts_,
              testing::ElementsAre(kMaxWaitForKeyframe, kMaxWaitForKeyframe * 2,
                                   kMaxWaitForFrame, kMaxWaitForFrame * 2));
  timeout_tracker_.Stop();
}

}  // namespace webrtc
