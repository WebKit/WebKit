/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/dvqa/pausable_state.h"

#include <memory>

#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

class PausableStateTest : public testing::Test {
 protected:
  PausableStateTest()
      : time_controller_(std::make_unique<GlobalSimulatedTimeController>(
            Timestamp::Seconds(1000))) {}

  void AdvanceTime(TimeDelta time) { time_controller_->AdvanceTime(time); }

  Clock* GetClock() { return time_controller_->GetClock(); }

  Timestamp Now() const { return time_controller_->GetClock()->CurrentTime(); }

 private:
  std::unique_ptr<TimeController> time_controller_;
};

TEST_F(PausableStateTest, NewIsActive) {
  PausableState state(GetClock());

  EXPECT_FALSE(state.IsPaused());
}

TEST_F(PausableStateTest, IsPausedAfterPaused) {
  PausableState state(GetClock());

  state.Pause();
  EXPECT_TRUE(state.IsPaused());
}

TEST_F(PausableStateTest, IsActiveAfterResume) {
  PausableState state(GetClock());

  state.Pause();
  state.Resume();
  EXPECT_FALSE(state.IsPaused());
}

TEST_F(PausableStateTest, PauseAlreadyPausedIsNoOp) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();

  EXPECT_EQ(state.GetLastEventTime(), test_time);
}

TEST_F(PausableStateTest, ResumeAlreadyResumedIsNoOp) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_EQ(state.GetLastEventTime(), test_time);
}

TEST_F(PausableStateTest, WasPausedAtFalseWhenMultiplePauseResumeAtSameTime) {
  PausableState state(GetClock());

  state.Pause();
  state.Resume();
  state.Pause();
  state.Resume();
  state.Pause();
  state.Resume();
  EXPECT_FALSE(state.WasPausedAt(Now()));
}

TEST_F(PausableStateTest,
       WasPausedAtTrueWhenMultiplePauseResumeAtSameTimeAndThenPause) {
  PausableState state(GetClock());

  state.Pause();
  state.Resume();
  state.Pause();
  state.Resume();
  state.Pause();
  state.Resume();
  state.Pause();
  EXPECT_TRUE(state.WasPausedAt(Now()));
}

TEST_F(PausableStateTest, WasPausedAtFalseBeforeFirstPause) {
  PausableState state(GetClock());

  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();

  EXPECT_FALSE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtTrueAfterPauseBeforeResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_TRUE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtFalseAfterResumeBeforePause) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_FALSE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtTrueAtPauseBeforeResume) {
  PausableState state(GetClock());

  state.Pause();
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_TRUE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtFalseAfterPauseAtResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_FALSE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtTrueAfterPause) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();

  EXPECT_TRUE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasPausedAtFalseAfterResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();

  EXPECT_FALSE(state.WasPausedAt(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterFalseBeforeFirstPause) {
  PausableState state(GetClock());

  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();

  EXPECT_FALSE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterTrueAfterPauseBeforeResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_TRUE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterFalseAfterResumeBeforePause) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_FALSE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterTrueAtPauseBeforeResume) {
  PausableState state(GetClock());

  state.Pause();
  Timestamp test_time = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_TRUE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterFalseAfterPauseAtResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();

  EXPECT_FALSE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterFalseAfterPause) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();

  EXPECT_FALSE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, WasResumedAfterFalseAfterResume) {
  PausableState state(GetClock());

  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp test_time = Now();

  EXPECT_FALSE(state.WasResumedAfter(test_time));
}

TEST_F(PausableStateTest, GetActiveDurationFromWithoutPausesReturnAllTime) {
  PausableState state(GetClock());

  Timestamp time_from = Now();
  AdvanceTime(TimeDelta::Seconds(5));

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Seconds(5));
}

TEST_F(PausableStateTest, GetActiveDurationFromRespectsPauses) {
  PausableState state(GetClock());

  Timestamp time_from = Now();

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Seconds(3));
}

TEST_F(PausableStateTest, GetActiveDurationFromMiddleOfPauseAccountOnlyActive) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp time_from = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Seconds(2));
}

TEST_F(PausableStateTest, GetActiveDurationFromMiddleOfActiveAccountAllActive) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp time_from = Now();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Seconds(2));
}

TEST_F(PausableStateTest, GetActiveDurationFromWhenPauseReturnsZero) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp time_from = Now();

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Zero());
}

TEST_F(PausableStateTest, GetActiveDurationFromWhenActiveReturnsAllTime) {
  PausableState state(GetClock());

  AdvanceTime(TimeDelta::Seconds(1));
  state.Pause();
  AdvanceTime(TimeDelta::Seconds(1));
  state.Resume();
  AdvanceTime(TimeDelta::Seconds(1));
  Timestamp time_from = Now();
  AdvanceTime(TimeDelta::Seconds(1));

  EXPECT_EQ(state.GetActiveDurationFrom(time_from), TimeDelta::Seconds(1));
}

}  // namespace
}  // namespace webrtc
