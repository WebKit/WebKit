/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_adaptation_processor.h"

#include <cstddef>
#include <memory>

#include "api/adaptation/resource.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "api/video/video_adaptation_counters.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

namespace {

using ::testing::Eq;

constexpr int kDefaultFrameRate = 30;
constexpr int kDefaultFrameSize = 1280 * 720;
constexpr TimeDelta kDefaultTimeout = TimeDelta::Seconds(5);

class VideoSourceRestrictionsListenerForTesting
    : public VideoSourceRestrictionsListener {
 public:
  VideoSourceRestrictionsListenerForTesting()
      : restrictions_updated_count_(0),
        restrictions_(),
        adaptation_counters_(),
        reason_(nullptr) {}
  ~VideoSourceRestrictionsListenerForTesting() override {}

  size_t restrictions_updated_count() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return restrictions_updated_count_;
  }
  VideoSourceRestrictions restrictions() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return restrictions_;
  }
  VideoAdaptationCounters adaptation_counters() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return adaptation_counters_;
  }
  scoped_refptr<Resource> reason() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return reason_;
  }

  // VideoSourceRestrictionsListener implementation.
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& /* unfiltered_restrictions */) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    ++restrictions_updated_count_;
    restrictions_ = restrictions;
    adaptation_counters_ = adaptation_counters;
    reason_ = reason;
  }

 private:
  SequenceChecker sequence_checker_;
  size_t restrictions_updated_count_ RTC_GUARDED_BY(&sequence_checker_);
  VideoSourceRestrictions restrictions_ RTC_GUARDED_BY(&sequence_checker_);
  VideoAdaptationCounters adaptation_counters_
      RTC_GUARDED_BY(&sequence_checker_);
  scoped_refptr<Resource> reason_ RTC_GUARDED_BY(&sequence_checker_);
};

class ResourceAdaptationProcessorTest : public ::testing::Test {
 public:
  ResourceAdaptationProcessorTest()
      : frame_rate_provider_(),
        input_state_provider_(&frame_rate_provider_),
        resource_(FakeResource::Create("FakeResource")),
        other_resource_(FakeResource::Create("OtherFakeResource")),
        video_stream_adapter_(
            std::make_unique<VideoStreamAdapter>(&input_state_provider_,
                                                 &frame_rate_provider_,
                                                 CreateTestFieldTrials())),
        processor_(std::make_unique<ResourceAdaptationProcessor>(
            video_stream_adapter_.get())) {
    video_stream_adapter_->AddRestrictionsListener(&restrictions_listener_);
    processor_->AddResource(resource_);
    processor_->AddResource(other_resource_);
  }
  ~ResourceAdaptationProcessorTest() override {
    if (processor_) {
      DestroyProcessor();
    }
  }

  void SetInputStates(bool has_input, int fps, int frame_size) {
    input_state_provider_.OnHasInputChanged(has_input);
    frame_rate_provider_.set_fps(fps);
    input_state_provider_.OnFrameSizeObserved(frame_size);
  }

  void RestrictSource(VideoSourceRestrictions restrictions) {
    SetInputStates(
        true, restrictions.max_frame_rate().value_or(kDefaultFrameRate),
        restrictions.target_pixels_per_frame().has_value()
            ? restrictions.target_pixels_per_frame().value()
            : restrictions.max_pixels_per_frame().value_or(kDefaultFrameSize));
  }

  void DestroyProcessor() {
    if (resource_) {
      processor_->RemoveResource(resource_);
    }
    if (other_resource_) {
      processor_->RemoveResource(other_resource_);
    }
    video_stream_adapter_->RemoveRestrictionsListener(&restrictions_listener_);
    processor_.reset();
  }

  static void WaitUntilTaskQueueIdle() {
    ASSERT_TRUE(Thread::Current()->ProcessMessages(0));
  }

 protected:
  AutoThread main_thread_;
  FakeFrameRateProvider frame_rate_provider_;
  VideoStreamInputStateProvider input_state_provider_;
  scoped_refptr<FakeResource> resource_;
  scoped_refptr<FakeResource> other_resource_;
  std::unique_ptr<VideoStreamAdapter> video_stream_adapter_;
  std::unique_ptr<ResourceAdaptationProcessor> processor_;
  VideoSourceRestrictionsListenerForTesting restrictions_listener_;
};

}  // namespace

TEST_F(ResourceAdaptationProcessorTest, DisabledByDefault) {
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adaptation does not happen when disabled.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, InsufficientInput) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  // Adaptation does not happen if input is insufficient.
  // When frame size is missing (OnFrameSizeObserved not called yet).
  input_state_provider_.OnHasInputChanged(true);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
  // When "has input" is missing.
  SetInputStates(false, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
  // Note: frame rate cannot be missing, if unset it is 0.
}

// These tests verify that restrictions are applied, but not exactly how much
// the source is restricted. This ensures that the VideoStreamAdapter is wired
// up correctly but not exactly how the VideoStreamAdapter generates
// restrictions. For that, see video_stream_adapter_unittest.cc.
TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingResolutionInMaintainFrameRate) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_pixels_per_frame().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateInMaintainResolution) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateAndResolutionInBalanced) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::BALANCED);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapting multiple times eventually resticts both frame rate and
  // resolution. Exactly many times we need to adapt depends on
  // BalancedDegradationSettings, VideoStreamAdapter and default input
  // states. This test requires it to be achieved within 4 adaptations.
  for (size_t i = 0; i < 4; ++i) {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    EXPECT_EQ(i + 1, restrictions_listener_.restrictions_updated_count());
    RestrictSource(restrictions_listener_.restrictions());
  }
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_pixels_per_frame().has_value());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest, AwaitingPreviousAdaptation) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  // If we don't restrict the source then adaptation will not happen again
  // due to "awaiting previous adaptation". This prevents "double-adapt".
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, CannotAdaptUpWhenUnrestricted) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, UnderuseTakesUsBackToUnrestricted) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2u, restrictions_listener_.restrictions_updated_count());
  EXPECT_EQ(VideoSourceRestrictions(), restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNeverAdaptedDown) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  RestrictSource(restrictions_listener_.restrictions());

  // Other resource signals under-use
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNotAdaptedDownAfterReset) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());

  video_stream_adapter_->ClearRestrictions();
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ did not overuse after we reset the restrictions, so adapt
  // up should be disallowed.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest, OnlyMostLimitedResourceMayAdaptUp) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // `other_resource_` is most limited, resource_ can't adapt up.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // `resource_` and `other_resource_` are now most limited, so both must
  // signal underuse to adapt up.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest,
       MultipleResourcesCanTriggerMultipleAdaptations) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ is not most limited so can't adapt from underuse.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // resource_ is still not most limited so can't adapt from underuse.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // However it will be after overuse
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // Now other_resource_ can't adapt up as it is not most restricted.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ is limited at 3 adaptations and other_resource_ 2.
  // With the most limited resource signalling underuse in the following
  // order we get back to unrestricted video.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // Both resource_ and other_resource_ are most limited.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // Again both are most limited.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest,
       MostLimitedResourceAdaptationWorksAfterChangingDegradataionPreference) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapt down until we can't anymore.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  int last_total = restrictions_listener_.adaptation_counters().Total();

  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  // resource_ can not adapt up since we have never reduced FPS.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(last_total, restrictions_listener_.adaptation_counters().Total());

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(last_total + 1,
            restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // other_resource_ is most limited so should be able to adapt up.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(last_total, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest,
       AdaptsDownWhenOtherResourceIsAlwaysUnderused) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  // Does not trigger adapataion because there's no restriction.
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());

  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  // Adapts down even if other resource asked for adapting up.
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());

  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  // Doesn't adapt up because adaptation is due to another resource.
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest,
       TriggerOveruseNotOnAdaptationTaskQueue) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  TaskQueueForTest resource_task_queue("ResourceTaskQueue");
  resource_task_queue.PostTask(
      [&]() { resource_->SetUsageState(ResourceUsageState::kOveruse); });

  EXPECT_THAT(
      WaitUntil(
          [&] { return restrictions_listener_.restrictions_updated_count(); },
          Eq(1u)),
      IsRtcOk());
}

TEST_F(ResourceAdaptationProcessorTest,
       DestroyProcessorWhileResourceListenerDelegateHasTaskInFlight) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  // Wait for `resource_` to signal oversue first so we know that the delegate
  // has passed it on to the processor's task queue.
  Event resource_event;
  TaskQueueForTest resource_task_queue("ResourceTaskQueue");
  resource_task_queue.PostTask([&]() {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    resource_event.Set();
  });

  EXPECT_TRUE(resource_event.Wait(kDefaultTimeout));
  // Now destroy the processor while handling the overuse is in flight.
  DestroyProcessor();

  // Because the processor was destroyed by the time the delegate's task ran,
  // the overuse signal must not have been handled.
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourceOveruseIgnoredWhenSignalledDuringRemoval) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  Event overuse_event;
  TaskQueueForTest resource_task_queue("ResourceTaskQueue");
  // Queues task for `resource_` overuse while `processor_` is still listening.
  resource_task_queue.PostTask([&]() {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    overuse_event.Set();
  });
  EXPECT_TRUE(overuse_event.Wait(kDefaultTimeout));
  // Once we know the overuse task is queued, remove `resource_` so that
  // `processor_` is not listening to it.
  processor_->RemoveResource(resource_);

  // Runs the queued task so `processor_` gets signalled kOveruse from
  // `resource_` even though `processor_` was not listening.
  WaitUntilTaskQueueIdle();

  // No restrictions should change even though `resource_` signaled `kOveruse`.
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingOnlyAdaptedResourceResetsAdaptation) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  processor_->RemoveResource(resource_);
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingMostLimitedResourceSetsAdaptationToNextLimitedLevel) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::BALANCED);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  VideoSourceRestrictions next_limited_restrictions =
      restrictions_listener_.restrictions();
  VideoAdaptationCounters next_limited_counters =
      restrictions_listener_.adaptation_counters();

  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());

  // Removing most limited `resource_` should revert us back to
  processor_->RemoveResource(resource_);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  EXPECT_EQ(next_limited_restrictions, restrictions_listener_.restrictions());
  EXPECT_EQ(next_limited_counters,
            restrictions_listener_.adaptation_counters());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingMostLimitedResourceSetsAdaptationIfInputStateUnchanged) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  VideoSourceRestrictions next_limited_restrictions =
      restrictions_listener_.restrictions();
  VideoAdaptationCounters next_limited_counters =
      restrictions_listener_.adaptation_counters();

  // Overuse twice and underuse once. After the underuse we don't restrict the
  // source. Normally this would block future underuses.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());

  // Removing most limited `resource_` should revert us back to, even though we
  // did not call RestrictSource() after `resource_` was overused. Normally
  // adaptation for MAINTAIN_FRAMERATE would be blocked here but for removal we
  // allow this anyways.
  processor_->RemoveResource(resource_);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  EXPECT_EQ(next_limited_restrictions, restrictions_listener_.restrictions());
  EXPECT_EQ(next_limited_counters,
            restrictions_listener_.adaptation_counters());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingResourceNotMostLimitedHasNoEffectOnLimitations) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::BALANCED);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());

  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  VideoSourceRestrictions current_restrictions =
      restrictions_listener_.restrictions();
  VideoAdaptationCounters current_counters =
      restrictions_listener_.adaptation_counters();
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());

  // Removing most limited `resource_` should revert us back to
  processor_->RemoveResource(other_resource_);
  EXPECT_EQ(current_restrictions, restrictions_listener_.restrictions());
  EXPECT_EQ(current_counters, restrictions_listener_.adaptation_counters());

  // Delete `other_resource_` for cleanup.
  other_resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingMostLimitedResourceAfterSwitchingDegradationPreferences) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  VideoSourceRestrictions next_limited_restrictions =
      restrictions_listener_.restrictions();
  VideoAdaptationCounters next_limited_counters =
      restrictions_listener_.adaptation_counters();

  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());

  // Revert to `other_resource_` when removing `resource_` even though the
  // degradation preference was different when it was overused.
  processor_->RemoveResource(resource_);
  EXPECT_EQ(next_limited_counters,
            restrictions_listener_.adaptation_counters());

  // After switching back to MAINTAIN_FRAMERATE, the next most limited settings
  // are restored.
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_EQ(next_limited_restrictions, restrictions_listener_.restrictions());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingMostLimitedResourceSetsNextLimitationsInDisabled) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  VideoSourceRestrictions next_limited_restrictions =
      restrictions_listener_.restrictions();
  VideoAdaptationCounters next_limited_counters =
      restrictions_listener_.adaptation_counters();
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());

  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::DISABLED);

  // Revert to `other_resource_` when removing `resource_` even though the
  // current degradataion preference is disabled.
  processor_->RemoveResource(resource_);

  // After switching back to MAINTAIN_FRAMERATE, the next most limited settings
  // are restored.
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_EQ(next_limited_restrictions, restrictions_listener_.restrictions());
  EXPECT_EQ(next_limited_counters,
            restrictions_listener_.adaptation_counters());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovedResourceSignalsIgnoredByProcessor) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  processor_->RemoveResource(resource_);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       RemovingResourceWhenMultipleMostLimtedHasNoEffect) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  // Adapt `resource_` up and then down so that both resource's are most
  // limited at 1 adaptation.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  RestrictSource(restrictions_listener_.restrictions());
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());

  // Removing `resource_` has no effect since both `resource_` and
  // `other_resource_` are most limited.
  processor_->RemoveResource(resource_);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());

  // Delete `resource_` for cleanup.
  resource_ = nullptr;
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourceOverusedAtLimitReachedWillShareMostLimited) {
  video_stream_adapter_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  bool has_reached_min_pixels = false;
  ON_CALL(frame_rate_provider_, OnMinPixelLimitReached())
      .WillByDefault(testing::Assign(&has_reached_min_pixels, true));

  // Adapt 10 times, which should make us hit the limit.
  for (int i = 0; i < 10; ++i) {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    RestrictSource(restrictions_listener_.restrictions());
  }
  EXPECT_TRUE(has_reached_min_pixels);
  auto last_update_count = restrictions_listener_.restrictions_updated_count();
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  // Now both `resource_` and `other_resource_` are most limited. Underuse of
  // `resource_` will not adapt up.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(last_update_count,
            restrictions_listener_.restrictions_updated_count());
}

}  // namespace webrtc
