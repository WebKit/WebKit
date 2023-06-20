/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_adapter.h"

#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/fake_video_stream_input_state_provider.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "rtc_base/string_encode.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "test/testsupport/rtc_expect_death.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

const int kBalancedHighResolutionPixels = 1280 * 720;
const int kBalancedHighFrameRateFps = 30;

const int kBalancedMediumResolutionPixels = 640 * 480;
const int kBalancedMediumFrameRateFps = 20;

const int kBalancedLowResolutionPixels = 320 * 240;
const int kBalancedLowFrameRateFps = 10;

std::string BalancedFieldTrialConfig() {
  return "WebRTC-Video-BalancedDegradationSettings/pixels:" +
         rtc::ToString(kBalancedLowResolutionPixels) + "|" +
         rtc::ToString(kBalancedMediumResolutionPixels) + "|" +
         rtc::ToString(kBalancedHighResolutionPixels) +
         ",fps:" + rtc::ToString(kBalancedLowFrameRateFps) + "|" +
         rtc::ToString(kBalancedMediumFrameRateFps) + "|" +
         rtc::ToString(kBalancedHighFrameRateFps) + "/";
}

// Responsible for adjusting the inputs to VideoStreamAdapter (SetInput), such
// as pixels and frame rate, according to the most recent source restrictions.
// This helps tests that apply adaptations multiple times: if the input is not
// adjusted between adaptations, the subsequent adaptations fail with
// kAwaitingPreviousAdaptation.
class FakeVideoStream {
 public:
  FakeVideoStream(VideoStreamAdapter* adapter,
                  FakeVideoStreamInputStateProvider* provider,
                  int input_pixels,
                  int input_fps,
                  int min_pixels_per_frame)
      : adapter_(adapter),
        provider_(provider),
        input_pixels_(input_pixels),
        input_fps_(input_fps),
        min_pixels_per_frame_(min_pixels_per_frame) {
    provider_->SetInputState(input_pixels_, input_fps_, min_pixels_per_frame_);
  }

  int input_pixels() const { return input_pixels_; }
  int input_fps() const { return input_fps_; }

  // Performs ApplyAdaptation() followed by SetInput() with input pixels and
  // frame rate adjusted according to the resulting restrictions.
  void ApplyAdaptation(Adaptation adaptation) {
    adapter_->ApplyAdaptation(adaptation, nullptr);
    // Update input pixels and fps according to the resulting restrictions.
    auto restrictions = adapter_->source_restrictions();
    if (restrictions.target_pixels_per_frame().has_value()) {
      RTC_DCHECK(!restrictions.max_pixels_per_frame().has_value() ||
                 restrictions.max_pixels_per_frame().value() >=
                     restrictions.target_pixels_per_frame().value());
      input_pixels_ = restrictions.target_pixels_per_frame().value();
    } else if (restrictions.max_pixels_per_frame().has_value()) {
      input_pixels_ = restrictions.max_pixels_per_frame().value();
    }
    if (restrictions.max_frame_rate().has_value()) {
      input_fps_ = restrictions.max_frame_rate().value();
    }
    provider_->SetInputState(input_pixels_, input_fps_, min_pixels_per_frame_);
  }

 private:
  VideoStreamAdapter* adapter_;
  FakeVideoStreamInputStateProvider* provider_;
  int input_pixels_;
  int input_fps_;
  int min_pixels_per_frame_;
};

class FakeVideoStreamAdapterListner : public VideoSourceRestrictionsListener {
 public:
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) override {
    calls_++;
    last_restrictions_ = unfiltered_restrictions;
  }

  int calls() const { return calls_; }

  VideoSourceRestrictions last_restrictions() const {
    return last_restrictions_;
  }

 private:
  int calls_ = 0;
  VideoSourceRestrictions last_restrictions_;
};

class MockAdaptationConstraint : public AdaptationConstraint {
 public:
  MOCK_METHOD(bool,
              IsAdaptationUpAllowed,
              (const VideoStreamInputState& input_state,
               const VideoSourceRestrictions& restrictions_before,
               const VideoSourceRestrictions& restrictions_after),
              (const, override));

  // MOCK_METHOD(std::string, Name, (), (const, override));
  std::string Name() const override { return "MockAdaptationConstraint"; }
};

}  // namespace

class VideoStreamAdapterTest : public ::testing::Test {
 public:
  VideoStreamAdapterTest()
      : field_trials_(BalancedFieldTrialConfig()),
        resource_(FakeResource::Create("FakeResource")),
        adapter_(&input_state_provider_,
                 &encoder_stats_observer_,
                 field_trials_) {}

 protected:
  webrtc::test::ScopedKeyValueConfig field_trials_;
  FakeVideoStreamInputStateProvider input_state_provider_;
  rtc::scoped_refptr<Resource> resource_;
  testing::StrictMock<MockVideoStreamEncoderObserver> encoder_stats_observer_;
  VideoStreamAdapter adapter_;
};

TEST_F(VideoStreamAdapterTest, NoRestrictionsByDefault) {
  EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_EQ(0, adapter_.adaptation_counters().Total());
}

TEST_F(VideoStreamAdapterTest, MaintainFramerate_DecreasesPixelsToThreeFifths) {
  const int kInputPixels = 1280 * 720;
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider_.SetInputState(kInputPixels, 30,
                                      kDefaultMinPixelsPerFrame);
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  adapter_.ApplyAdaptation(adaptation, nullptr);
  EXPECT_EQ(static_cast<size_t>((kInputPixels * 3) / 5),
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(absl::nullopt, adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       MaintainFramerate_DecreasesPixelsToLimitReached) {
  const int kMinPixelsPerFrame = 640 * 480;

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider_.SetInputState(kMinPixelsPerFrame + 1, 30,
                                      kMinPixelsPerFrame);
  EXPECT_CALL(encoder_stats_observer_, OnMinPixelLimitReached());
  // Even though we are above kMinPixelsPerFrame, because adapting down would
  // have exceeded the limit, we are said to have reached the limit already.
  // This differs from the frame rate adaptation logic, which would have clamped
  // to the limit in the first step and reported kLimitReached in the second
  // step.
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kLimitReached, adaptation.status());
}

TEST_F(VideoStreamAdapterTest, MaintainFramerate_IncreasePixelsToFiveThirds) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Go down twice, ensuring going back up is still a restricted resolution.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(2, adapter_.adaptation_counters().resolution_adaptations);
  int input_pixels = fake_stream.input_pixels();
  // Go up once. The target is 5/3 and the max is 12/5 of the target.
  const int target = (input_pixels * 5) / 3;
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(static_cast<size_t>((target * 12) / 5),
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(static_cast<size_t>(target),
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(absl::nullopt, adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
}

TEST_F(VideoStreamAdapterTest, MaintainFramerate_IncreasePixelsToUnrestricted) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // We are unrestricted by default and should not be able to adapt up.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter_.GetAdaptationUp().status());
  // If we go down once and then back up we should not have any restrictions.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_EQ(0, adapter_.adaptation_counters().Total());
}

TEST_F(VideoStreamAdapterTest, MaintainResolution_DecreasesFpsToTwoThirds) {
  const int kInputFps = 30;

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  input_state_provider_.SetInputState(1280 * 720, kInputFps,
                                      kDefaultMinPixelsPerFrame);
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  adapter_.ApplyAdaptation(adaptation, nullptr);
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>((kInputFps * 2) / 3),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest, MaintainResolution_DecreasesFpsToLimitReached) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720,
                              kMinFrameRateFps + 1, kDefaultMinPixelsPerFrame);
  // If we are not yet at the limit and the next step would exceed it, the step
  // is clamped such that we end up exactly on the limit.
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);
  EXPECT_EQ(static_cast<double>(kMinFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  // Having reached the limit, the next adaptation down is not valid.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter_.GetAdaptationDown().status());
}

TEST_F(VideoStreamAdapterTest, MaintainResolution_IncreaseFpsToThreeHalves) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Go down twice, ensuring going back up is still a restricted frame rate.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(2, adapter_.adaptation_counters().fps_adaptations);
  int input_fps = fake_stream.input_fps();
  // Go up once. The target is 3/2 of the input.
  Adaptation adaptation = adapter_.GetAdaptationUp();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>((input_fps * 3) / 2),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest, MaintainResolution_IncreaseFpsToUnrestricted) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // We are unrestricted by default and should not be able to adapt up.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter_.GetAdaptationUp().status());
  // If we go down once and then back up we should not have any restrictions.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_EQ(0, adapter_.adaptation_counters().Total());
}

TEST_F(VideoStreamAdapterTest, Balanced_DecreaseFrameRate) {
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  input_state_provider_.SetInputState(kBalancedMediumResolutionPixels,
                                      kBalancedHighFrameRateFps,
                                      kDefaultMinPixelsPerFrame);
  // If our frame rate is higher than the frame rate associated with our
  // resolution we should try to adapt to the frame rate associated with our
  // resolution: kBalancedMediumFrameRateFps.
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  adapter_.ApplyAdaptation(adaptation, nullptr);
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedMediumFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(0, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest, Balanced_DecreaseResolution) {
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(
      &adapter_, &input_state_provider_, kBalancedHighResolutionPixels,
      kBalancedHighFrameRateFps, kDefaultMinPixelsPerFrame);
  // If we are not below the current resolution's frame rate limit, we should
  // adapt resolution according to "maintain-framerate" logic (three fifths).
  //
  // However, since we are unlimited at the start and input frame rate is not
  // below kBalancedHighFrameRateFps, we first restrict the frame rate to
  // kBalancedHighFrameRateFps even though that is our current frame rate. This
  // does prevent the source from going higher, though, so it's technically not
  // a NO-OP.
  {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(0, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  // Verify "maintain-framerate" logic the second time we adapt: Frame rate
  // restrictions remains the same and resolution goes down.
  {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  constexpr size_t kReducedPixelsFirstStep =
      static_cast<size_t>((kBalancedHighResolutionPixels * 3) / 5);
  EXPECT_EQ(kReducedPixelsFirstStep,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  // If we adapt again, because the balanced settings' proposed frame rate is
  // still kBalancedHighFrameRateFps, "maintain-framerate" will trigger again.
  static_assert(kReducedPixelsFirstStep > kBalancedMediumResolutionPixels,
                "The reduced resolution is still greater than the next lower "
                "balanced setting resolution");
  constexpr size_t kReducedPixelsSecondStep = (kReducedPixelsFirstStep * 3) / 5;
  {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  EXPECT_EQ(kReducedPixelsSecondStep,
            adapter_.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter_.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(2, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
}

// Testing when to adapt frame rate and when to adapt resolution is quite
// entangled, so this test covers both cases.
//
// There is an asymmetry: When we adapt down we do it in one order, but when we
// adapt up we don't do it in the reverse order. Instead we always try to adapt
// frame rate first according to balanced settings' configs and only when the
// frame rate is already achieved do we adjust the resolution.
TEST_F(VideoStreamAdapterTest, Balanced_IncreaseFrameRateAndResolution) {
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(
      &adapter_, &input_state_provider_, kBalancedHighResolutionPixels,
      kBalancedHighFrameRateFps, kDefaultMinPixelsPerFrame);
  // The desired starting point of this test is having adapted frame rate twice.
  // This requires performing a number of adaptations.
  constexpr size_t kReducedPixelsFirstStep =
      static_cast<size_t>((kBalancedHighResolutionPixels * 3) / 5);
  constexpr size_t kReducedPixelsSecondStep = (kReducedPixelsFirstStep * 3) / 5;
  constexpr size_t kReducedPixelsThirdStep = (kReducedPixelsSecondStep * 3) / 5;
  static_assert(kReducedPixelsFirstStep > kBalancedMediumResolutionPixels,
                "The first pixel reduction is greater than the balanced "
                "settings' medium pixel configuration");
  static_assert(kReducedPixelsSecondStep > kBalancedMediumResolutionPixels,
                "The second pixel reduction is greater than the balanced "
                "settings' medium pixel configuration");
  static_assert(kReducedPixelsThirdStep <= kBalancedMediumResolutionPixels,
                "The third pixel reduction is NOT greater than the balanced "
                "settings' medium pixel configuration");
  // The first adaptation should affect the frame rate: See
  // Balanced_DecreaseResolution for explanation why.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  // The next three adaptations affects the resolution, because we have to reach
  // kBalancedMediumResolutionPixels before a lower frame rate is considered by
  // BalancedDegradationSettings. The number three is derived from the
  // static_asserts above.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsFirstStep,
            adapter_.source_restrictions().max_pixels_per_frame());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsSecondStep,
            adapter_.source_restrictions().max_pixels_per_frame());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsThirdStep,
            adapter_.source_restrictions().max_pixels_per_frame());
  // Thus, the next adaptation will reduce frame rate to
  // kBalancedMediumFrameRateFps.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedMediumFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(3, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(2, adapter_.adaptation_counters().fps_adaptations);
  // Adapt up!
  // While our resolution is in the medium-range, the frame rate associated with
  // the next resolution configuration up ("high") is kBalancedHighFrameRateFps
  // and "balanced" prefers adapting frame rate if not already applied.
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
              adapter_.source_restrictions().max_frame_rate());
    EXPECT_EQ(3, adapter_.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  }
  // Now that we have already achieved the next frame rate up, we act according
  // to "maintain-framerate". We go back up in resolution. Due to rounding
  // errors we don't end up back at kReducedPixelsSecondStep. Rather we get to
  // kReducedPixelsSecondStepUp, which is off by one compared to
  // kReducedPixelsSecondStep.
  constexpr size_t kReducedPixelsSecondStepUp =
      (kReducedPixelsThirdStep * 5) / 3;
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(kReducedPixelsSecondStepUp,
              adapter_.source_restrictions().target_pixels_per_frame());
    EXPECT_EQ(2, adapter_.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  }
  // Now that our resolution is back in the high-range, the next frame rate to
  // try out is "unlimited".
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(absl::nullopt, adapter_.source_restrictions().max_frame_rate());
    EXPECT_EQ(2, adapter_.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(0, adapter_.adaptation_counters().fps_adaptations);
  }
  // Now only adapting resolution remains.
  constexpr size_t kReducedPixelsFirstStepUp =
      (kReducedPixelsSecondStepUp * 5) / 3;
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(kReducedPixelsFirstStepUp,
              adapter_.source_restrictions().target_pixels_per_frame());
    EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(0, adapter_.adaptation_counters().fps_adaptations);
  }
  // The last step up should make us entirely unrestricted.
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
    EXPECT_EQ(0, adapter_.adaptation_counters().Total());
  }
}

TEST_F(VideoStreamAdapterTest, Balanced_LimitReached) {
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(
      &adapter_, &input_state_provider_, kBalancedLowResolutionPixels,
      kBalancedLowFrameRateFps, kDefaultMinPixelsPerFrame);
  // Attempting to adapt up while unrestricted should result in kLimitReached.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter_.GetAdaptationUp().status());
  // Adapting down once result in restricted frame rate, in this case we reach
  // the lowest possible frame rate immediately: kBalancedLowFrameRateFps.
  EXPECT_CALL(encoder_stats_observer_, OnMinPixelLimitReached()).Times(2);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedLowFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  // Any further adaptation must follow "maintain-framerate" rules (these are
  // covered in more depth by the MaintainFramerate tests). This test does not
  // assert exactly how resolution is adjusted, only that resolution always
  // decreases and that we eventually reach kLimitReached.
  size_t previous_resolution = kBalancedLowResolutionPixels;
  bool did_reach_limit = false;
  // If we have not reached the limit within 5 adaptations something is wrong...
  for (int i = 0; i < 5; i++) {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    if (adaptation.status() == Adaptation::Status::kLimitReached) {
      did_reach_limit = true;
      break;
    }
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_LT(adapter_.source_restrictions().max_pixels_per_frame().value(),
              previous_resolution);
    previous_resolution =
        adapter_.source_restrictions().max_pixels_per_frame().value();
  }
  EXPECT_TRUE(did_reach_limit);
  // Frame rate restrictions are the same as before.
  EXPECT_EQ(static_cast<double>(kBalancedLowFrameRateFps),
            adapter_.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
}

// kAwaitingPreviousAdaptation is only supported in "maintain-framerate".
TEST_F(VideoStreamAdapterTest,
       MaintainFramerate_AwaitingPreviousAdaptationDown) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  // Adapt down once, but don't update the input.
  adapter_.ApplyAdaptation(adapter_.GetAdaptationDown(), nullptr);
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
  {
    // Having performed the adaptation, but not updated the input based on the
    // new restrictions, adapting again in the same direction will not work.
    Adaptation adaptation = adapter_.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
              adaptation.status());
  }
}

// kAwaitingPreviousAdaptation is only supported in "maintain-framerate".
TEST_F(VideoStreamAdapterTest, MaintainFramerate_AwaitingPreviousAdaptationUp) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Perform two adaptation down so that adapting up twice is possible.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(2, adapter_.adaptation_counters().resolution_adaptations);
  // Adapt up once, but don't update the input.
  adapter_.ApplyAdaptation(adapter_.GetAdaptationUp(), nullptr);
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
  {
    // Having performed the adaptation, but not updated the input based on the
    // new restrictions, adapting again in the same direction will not work.
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
              adaptation.status());
  }
}

TEST_F(VideoStreamAdapterTest,
       MaintainResolution_AdaptsUpAfterSwitchingDegradationPreference) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down in fps for later.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  EXPECT_EQ(0, adapter_.adaptation_counters().resolution_adaptations);

  // We should be able to adapt in framerate one last time after the change of
  // degradation preference.
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  Adaptation adaptation = adapter_.GetAdaptationUp();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(0, adapter_.adaptation_counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       MaintainFramerate_AdaptsUpAfterSwitchingDegradationPreference) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down in resolution for later.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(0, adapter_.adaptation_counters().fps_adaptations);

  // We should be able to adapt in framerate one last time after the change of
  // degradation preference.
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  Adaptation adaptation = adapter_.GetAdaptationUp();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationUp());
  EXPECT_EQ(0, adapter_.adaptation_counters().resolution_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       PendingResolutionIncreaseAllowsAdaptUpAfterSwitchToMaintainResolution) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt fps down so we can adapt up later in the test.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  // Apply adaptation up but don't update input.
  adapter_.ApplyAdaptation(adapter_.GetAdaptationUp(), nullptr);
  EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
            adapter_.GetAdaptationUp().status());

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  Adaptation adaptation = adapter_.GetAdaptationUp();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
}

TEST_F(VideoStreamAdapterTest,
       MaintainFramerate_AdaptsDownAfterSwitchingDegradationPreference) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down once, should change FPS.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  // Adaptation down should apply after the degradation prefs change.
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);
  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       MaintainResolution_AdaptsDownAfterSwitchingDegradationPreference) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down once, should change FPS.
  fake_stream.ApplyAdaptation(adapter_.GetAdaptationDown());
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);

  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);

  EXPECT_EQ(1, adapter_.adaptation_counters().fps_adaptations);
  EXPECT_EQ(1, adapter_.adaptation_counters().resolution_adaptations);
}

TEST_F(
    VideoStreamAdapterTest,
    PendingResolutionDecreaseAllowsAdaptDownAfterSwitchToMaintainResolution) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Apply adaptation but don't update the input.
  adapter_.ApplyAdaptation(adapter_.GetAdaptationDown(), nullptr);
  EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
            adapter_.GetAdaptationDown().status());
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  Adaptation adaptation = adapter_.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
}

TEST_F(VideoStreamAdapterTest, RestrictionBroadcasted) {
  FakeVideoStreamAdapterListner listener;
  adapter_.AddRestrictionsListener(&listener);
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Not broadcast on invalid ApplyAdaptation.
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    adapter_.ApplyAdaptation(adaptation, nullptr);
    EXPECT_EQ(0, listener.calls());
  }

  // Broadcast on ApplyAdaptation.
  {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(1, listener.calls());
    EXPECT_EQ(adaptation.restrictions(), listener.last_restrictions());
  }

  // Broadcast on ClearRestrictions().
  adapter_.ClearRestrictions();
  EXPECT_EQ(2, listener.calls());
  EXPECT_EQ(VideoSourceRestrictions(), listener.last_restrictions());
}

TEST_F(VideoStreamAdapterTest, AdaptationHasNextRestrcitions) {
  // Any non-disabled DegradationPreference will do.
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // When adaptation is not possible.
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kLimitReached, adaptation.status());
    EXPECT_EQ(adaptation.restrictions(), adapter_.source_restrictions());
    EXPECT_EQ(0, adaptation.counters().Total());
  }
  // When we adapt down.
  {
    Adaptation adaptation = adapter_.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(adaptation.restrictions(), adapter_.source_restrictions());
    EXPECT_EQ(adaptation.counters(), adapter_.adaptation_counters());
  }
  // When we adapt up.
  {
    Adaptation adaptation = adapter_.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(adaptation.restrictions(), adapter_.source_restrictions());
    EXPECT_EQ(adaptation.counters(), adapter_.adaptation_counters());
  }
}

TEST_F(VideoStreamAdapterTest,
       SetDegradationPreferenceToOrFromBalancedClearsRestrictions) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  adapter_.ApplyAdaptation(adapter_.GetAdaptationDown(), nullptr);
  EXPECT_NE(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_NE(0, adapter_.adaptation_counters().Total());
  // Changing from non-balanced to balanced clears the restrictions.
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_EQ(0, adapter_.adaptation_counters().Total());
  // Apply adaptation again.
  adapter_.ApplyAdaptation(adapter_.GetAdaptationDown(), nullptr);
  EXPECT_NE(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_NE(0, adapter_.adaptation_counters().Total());
  // Changing from balanced to non-balanced clears the restrictions.
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_EQ(VideoSourceRestrictions(), adapter_.source_restrictions());
  EXPECT_EQ(0, adapter_.adaptation_counters().Total());
}

TEST_F(VideoStreamAdapterTest,
       GetAdaptDownResolutionAdaptsResolutionInMaintainFramerate) {
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);

  auto adaptation = adapter_.GetAdaptDownResolution();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  EXPECT_EQ(1, adaptation.counters().resolution_adaptations);
  EXPECT_EQ(0, adaptation.counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       GetAdaptDownResolutionReturnsWithStatusInDisabledAndMaintainResolution) {
  adapter_.SetDegradationPreference(DegradationPreference::DISABLED);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  EXPECT_EQ(Adaptation::Status::kAdaptationDisabled,
            adapter_.GetAdaptDownResolution().status());
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter_.GetAdaptDownResolution().status());
}

TEST_F(VideoStreamAdapterTest,
       GetAdaptDownResolutionAdaptsFpsAndResolutionInBalanced) {
  // Note: This test depends on BALANCED implementation, but with current
  // implementation and input state settings, BALANCED will adapt resolution and
  // frame rate once.
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);

  auto adaptation = adapter_.GetAdaptDownResolution();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  EXPECT_EQ(1, adaptation.counters().resolution_adaptations);
  EXPECT_EQ(1, adaptation.counters().fps_adaptations);
}

TEST_F(
    VideoStreamAdapterTest,
    GetAdaptDownResolutionAdaptsOnlyResolutionIfFpsAlreadyAdapterInBalanced) {
  // Note: This test depends on BALANCED implementation, but with current
  // implementation and input state settings, BALANCED will adapt resolution
  // only.
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  input_state_provider_.SetInputState(1280 * 720, 5, kDefaultMinPixelsPerFrame);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);

  auto first_adaptation = adapter_.GetAdaptationDown();
  fake_stream.ApplyAdaptation(first_adaptation);

  auto adaptation = adapter_.GetAdaptDownResolution();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  EXPECT_EQ(1, adaptation.counters().resolution_adaptations);
  EXPECT_EQ(first_adaptation.counters().fps_adaptations,
            adaptation.counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       GetAdaptDownResolutionAdaptsOnlyFpsIfResolutionLowInBalanced) {
  // Note: This test depends on BALANCED implementation, but with current
  // implementation and input state settings, BALANCED will adapt resolution
  // only.
  adapter_.SetDegradationPreference(DegradationPreference::BALANCED);
  input_state_provider_.SetInputState(kDefaultMinPixelsPerFrame, 30,
                                      kDefaultMinPixelsPerFrame);

  auto adaptation = adapter_.GetAdaptDownResolution();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  EXPECT_EQ(0, adaptation.counters().resolution_adaptations);
  EXPECT_EQ(1, adaptation.counters().fps_adaptations);
}

TEST_F(VideoStreamAdapterTest,
       AdaptationDisabledStatusAlwaysWhenDegradationPreferenceDisabled) {
  adapter_.SetDegradationPreference(DegradationPreference::DISABLED);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  EXPECT_EQ(Adaptation::Status::kAdaptationDisabled,
            adapter_.GetAdaptationDown().status());
  EXPECT_EQ(Adaptation::Status::kAdaptationDisabled,
            adapter_.GetAdaptationUp().status());
  EXPECT_EQ(Adaptation::Status::kAdaptationDisabled,
            adapter_.GetAdaptDownResolution().status());
}

TEST_F(VideoStreamAdapterTest, AdaptationConstraintAllowsAdaptationsUp) {
  testing::StrictMock<MockAdaptationConstraint> adaptation_constraint;
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter_.AddAdaptationConstraint(&adaptation_constraint);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down once so we can adapt up later.
  auto first_adaptation = adapter_.GetAdaptationDown();
  fake_stream.ApplyAdaptation(first_adaptation);

  EXPECT_CALL(adaptation_constraint,
              IsAdaptationUpAllowed(_, first_adaptation.restrictions(), _))
      .WillOnce(Return(true));
  EXPECT_EQ(Adaptation::Status::kValid, adapter_.GetAdaptationUp().status());
  adapter_.RemoveAdaptationConstraint(&adaptation_constraint);
}

TEST_F(VideoStreamAdapterTest, AdaptationConstraintDisallowsAdaptationsUp) {
  testing::StrictMock<MockAdaptationConstraint> adaptation_constraint;
  adapter_.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter_.AddAdaptationConstraint(&adaptation_constraint);
  input_state_provider_.SetInputState(1280 * 720, 30,
                                      kDefaultMinPixelsPerFrame);
  FakeVideoStream fake_stream(&adapter_, &input_state_provider_, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Adapt down once so we can adapt up later.
  auto first_adaptation = adapter_.GetAdaptationDown();
  fake_stream.ApplyAdaptation(first_adaptation);

  EXPECT_CALL(adaptation_constraint,
              IsAdaptationUpAllowed(_, first_adaptation.restrictions(), _))
      .WillOnce(Return(false));
  EXPECT_EQ(Adaptation::Status::kRejectedByConstraint,
            adapter_.GetAdaptationUp().status());
  adapter_.RemoveAdaptationConstraint(&adaptation_constraint);
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(VideoStreamAdapterDeathTest,
     SetDegradationPreferenceInvalidatesAdaptations) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  FakeVideoStreamInputStateProvider input_state_provider;
  testing::StrictMock<MockVideoStreamEncoderObserver> encoder_stats_observer_;
  VideoStreamAdapter adapter(&input_state_provider, &encoder_stats_observer_,
                             field_trials);
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  input_state_provider.SetInputState(1280 * 720, 30, kDefaultMinPixelsPerFrame);
  Adaptation adaptation = adapter.GetAdaptationDown();
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_DEATH(adapter.ApplyAdaptation(adaptation, nullptr), "");
}

TEST(VideoStreamAdapterDeathTest, AdaptDownInvalidatesAdaptations) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  FakeVideoStreamInputStateProvider input_state_provider;
  testing::StrictMock<MockVideoStreamEncoderObserver> encoder_stats_observer_;
  VideoStreamAdapter adapter(&input_state_provider, &encoder_stats_observer_,
                             field_trials);
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  input_state_provider.SetInputState(1280 * 720, 30, kDefaultMinPixelsPerFrame);
  Adaptation adaptation = adapter.GetAdaptationDown();
  adapter.GetAdaptationDown();
  EXPECT_DEATH(adapter.ApplyAdaptation(adaptation, nullptr), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
