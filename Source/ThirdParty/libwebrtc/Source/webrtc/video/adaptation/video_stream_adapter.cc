/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_adapter.h"

#include <algorithm>
#include <limits>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace {

const int kMinFramerateFps = 2;

int MinPixelsPerFrame(const absl::optional<EncoderSettings>& encoder_settings) {
  return encoder_settings.has_value()
             ? encoder_settings->encoder_info()
                   .scaling_settings.min_pixels_per_frame
             : kDefaultMinPixelsPerFrame;
}

// Generate suggested higher and lower frame rates and resolutions, to be
// applied to the VideoSourceRestrictor. These are used in "maintain-resolution"
// and "maintain-framerate". The "balanced" degradation preference also makes
// use of BalancedDegradationPreference when generating suggestions. The
// VideoSourceRestrictor decidedes whether or not a proposed adaptation is
// valid.

// For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
int GetLowerFrameRateThan(int fps) {
  RTC_DCHECK(fps != std::numeric_limits<int>::max());
  return (fps * 2) / 3;
}
// TODO(hbos): Use absl::optional<> instead?
int GetHigherFrameRateThan(int fps) {
  return fps != std::numeric_limits<int>::max()
             ? (fps * 3) / 2
             : std::numeric_limits<int>::max();
}

// For resolution, the steps we take are 3/5 (down) and 5/3 (up).
// Notice the asymmetry of which restriction property is set depending on if
// we are adapting up or down:
// - VideoSourceRestrictor::DecreaseResolution() sets the max_pixels_per_frame()
//   to the desired target and target_pixels_per_frame() to null.
// - VideoSourceRestrictor::IncreaseResolutionTo() sets the
//   target_pixels_per_frame() to the desired target, and max_pixels_per_frame()
//   is set according to VideoSourceRestrictor::GetIncreasedMaxPixelsWanted().
int GetLowerResolutionThan(int pixel_count) {
  RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
  return (pixel_count * 3) / 5;
}
// TODO(hbos): Use absl::optional<> instead?
int GetHigherResolutionThan(int pixel_count) {
  return pixel_count != std::numeric_limits<int>::max()
             ? (pixel_count * 5) / 3
             : std::numeric_limits<int>::max();
}

// One of the conditions used in VideoStreamAdapter::GetAdaptUpTarget().
// TODO(hbos): Whether or not we can adapt up due to encoder settings and
// bitrate should be expressed as a bandwidth-related Resource.
bool CanAdaptUpResolution(
    const absl::optional<EncoderSettings>& encoder_settings,
    absl::optional<uint32_t> encoder_target_bitrate_bps,
    int input_pixels) {
  uint32_t bitrate_bps = encoder_target_bitrate_bps.value_or(0);
  absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder_settings.has_value()
          ? encoder_settings->encoder_info()
                .GetEncoderBitrateLimitsForResolution(
                    GetHigherResolutionThan(input_pixels))
          : absl::nullopt;
  if (!bitrate_limits.has_value() || bitrate_bps == 0) {
    return true;  // No limit configured or bitrate provided.
  }
  RTC_DCHECK_GE(bitrate_limits->frame_size_pixels, input_pixels);
  return bitrate_bps >=
         static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
}

}  // namespace

VideoStreamAdapter::AdaptationTarget::AdaptationTarget(AdaptationAction action,
                                                       int value)
    : action(action), value(value) {}

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions.
class VideoStreamAdapter::VideoSourceRestrictor {
 public:
  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() const {
    return source_restrictions_;
  }
  const AdaptationCounters& adaptation_counters() const { return adaptations_; }
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
    adaptations_ = AdaptationCounters();
  }

  bool CanDecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame;
  }
  void DecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels, min_pixels_per_frame));
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    source_restrictions_.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations_.resolution_adaptations;
  }

  bool CanIncreaseResolutionTo(int target_pixels) {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }
  void IncreaseResolutionTo(int target_pixels) {
    RTC_DCHECK(CanIncreaseResolutionTo(target_pixels));
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    --adaptations_.resolution_adaptations;
    RTC_DCHECK_GE(adaptations_.resolution_adaptations, 0);
  }

  bool CanDecreaseFrameRateTo(int max_frame_rate) {
    const int fps_wanted = std::max(kMinFramerateFps, max_frame_rate);
    return fps_wanted < rtc::dchecked_cast<int>(
                            source_restrictions_.max_frame_rate().value_or(
                                std::numeric_limits<int>::max()));
  }
  void DecreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate));
    max_frame_rate = std::max(kMinFramerateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    ++adaptations_.fps_adaptations;
  }

  bool CanIncreaseFrameRateTo(int max_frame_rate) {
    return max_frame_rate > rtc::dchecked_cast<int>(
                                source_restrictions_.max_frame_rate().value_or(
                                    std::numeric_limits<int>::max()));
  }
  void IncreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanIncreaseFrameRateTo(max_frame_rate));
    RTC_LOG(LS_INFO) << "Scaling up framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    --adaptations_.fps_adaptations;
    RTC_DCHECK_GE(adaptations_.fps_adaptations, 0);
  }

 private:
  static int GetIncreasedMaxPixelsWanted(int target_pixels) {
    if (target_pixels == std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    // When we decrease resolution, we go down to at most 3/5 of current pixels.
    // Thus to increase resolution, we need 3/5 to get back to where we started.
    // When going up, the desired max_pixels_per_frame() has to be significantly
    // higher than the target because the source's native resolutions might not
    // match the target. We pick 12/5 of the target.
    //
    // (This value was historically 4 times the old target, which is (3/5)*4 of
    // the new target - or 12/5 - assuming the target is adjusted according to
    // the above steps.)
    RTC_DCHECK(target_pixels != std::numeric_limits<int>::max());
    return (target_pixels * 12) / 5;
  }

  VideoSourceRestrictions source_restrictions_;
  AdaptationCounters adaptations_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

// static
VideoStreamAdapter::AdaptationRequest::Mode
VideoStreamAdapter::AdaptationRequest::GetModeFromAdaptationAction(
    VideoStreamAdapter::AdaptationAction action) {
  switch (action) {
    case AdaptationAction::kIncreaseResolution:
      return AdaptationRequest::Mode::kAdaptUp;
    case AdaptationAction::kDecreaseResolution:
      return AdaptationRequest::Mode::kAdaptDown;
    case AdaptationAction::kIncreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptUp;
    case AdaptationAction::kDecreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptDown;
  }
}

VideoStreamAdapter::VideoStreamAdapter()
    : source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      balanced_settings_(),
      degradation_preference_(DegradationPreference::DISABLED),
      last_adaptation_request_(absl::nullopt) {}

VideoStreamAdapter::~VideoStreamAdapter() {}

VideoSourceRestrictions VideoStreamAdapter::source_restrictions() const {
  return source_restrictor_->source_restrictions();
}

const AdaptationCounters& VideoStreamAdapter::adaptation_counters() const {
  return source_restrictor_->adaptation_counters();
}

const BalancedDegradationSettings& VideoStreamAdapter::balanced_settings()
    const {
  return balanced_settings_;
}

void VideoStreamAdapter::ClearRestrictions() {
  source_restrictor_->ClearRestrictions();
  last_adaptation_request_.reset();
}

VideoStreamAdapter::SetDegradationPreferenceResult
VideoStreamAdapter::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  bool did_clear = false;
  if (degradation_preference_ != degradation_preference) {
    if (degradation_preference == DegradationPreference::BALANCED ||
        degradation_preference_ == DegradationPreference::BALANCED) {
      ClearRestrictions();
      did_clear = true;
    }
  }
  degradation_preference_ = degradation_preference;
  return did_clear ? SetDegradationPreferenceResult::kRestrictionsCleared
                   : SetDegradationPreferenceResult::kRestrictionsNotCleared;
}

absl::optional<VideoStreamAdapter::AdaptationTarget>
VideoStreamAdapter::GetAdaptUpTarget(
    const absl::optional<EncoderSettings>& encoder_settings,
    absl::optional<uint32_t> encoder_target_bitrate_bps,
    VideoInputMode input_mode,
    int input_pixels,
    int input_fps,
    AdaptationObserverInterface::AdaptReason reason) const {
  // Preconditions for being able to adapt up:
  if (input_mode == VideoInputMode::kNoVideo)
    return absl::nullopt;
  // 1. We shouldn't adapt up if we're currently waiting for a previous upgrade
  // to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  bool last_adaptation_was_up =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;
  if (last_adaptation_was_up &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels <= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }
  // 2. We shouldn't adapt up if BalancedSettings doesn't allow it, which is
  // only applicable if reason is kQuality and preference is BALANCED.
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      EffectiveDegradationPreference(input_mode) ==
          DegradationPreference::BALANCED &&
      !balanced_settings_.CanAdaptUp(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels,
          encoder_target_bitrate_bps.value_or(0))) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference(input_mode)) {
    case DegradationPreference::BALANCED: {
      // Attempt to increase target frame rate.
      int target_fps = balanced_settings_.MaxFps(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels);
      if (source_restrictor_->CanIncreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kIncreaseFrameRate,
                                target_fps);
      }
      // Fall-through to maybe-adapting resolution, unless |balanced_settings_|
      // forbids it based on bitrate.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels,
              encoder_target_bitrate_bps.value_or(0))) {
        return absl::nullopt;
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Don't adapt resolution if CanAdaptUpResolution() forbids it based on
      // bitrate and limits specified by encoder capabilities.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !CanAdaptUpResolution(encoder_settings, encoder_target_bitrate_bps,
                                input_pixels)) {
        return absl::nullopt;
      }
      // Attempt to increase pixel count.
      int target_pixels = input_pixels;
      if (source_restrictor_->adaptation_counters().resolution_adaptations ==
          1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        target_pixels = std::numeric_limits<int>::max();
      }
      target_pixels = GetHigherResolutionThan(target_pixels);
      if (!source_restrictor_->CanIncreaseResolutionTo(target_pixels))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int target_fps = input_fps;
      if (source_restrictor_->adaptation_counters().fps_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        target_fps = std::numeric_limits<int>::max();
      }
      target_fps = GetHigherFrameRateThan(target_fps);
      if (!source_restrictor_->CanIncreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      return absl::nullopt;
  }
}

absl::optional<VideoStreamAdapter::AdaptationTarget>
VideoStreamAdapter::GetAdaptDownTarget(
    const absl::optional<EncoderSettings>& encoder_settings,
    VideoInputMode input_mode,
    int input_pixels,
    int input_fps,
    VideoStreamEncoderObserver* encoder_stats_observer) const {
  const int min_pixels_per_frame = MinPixelsPerFrame(encoder_settings);
  // Preconditions for being able to adapt down:
  if (input_mode == VideoInputMode::kNoVideo)
    return absl::nullopt;
  // 1. We are not disabled.
  // TODO(hbos): Don't support DISABLED, it doesn't exist in the spec and it
  // causes scaling due to bandwidth constraints (QualityScalerResource) to be
  // ignored, not just CPU signals. This is not a use case we want to support
  // long-term; remove this enum value.
  if (degradation_preference_ == DegradationPreference::DISABLED)
    return absl::nullopt;
  bool last_adaptation_was_down =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;
  // 2. We shouldn't adapt down if our frame rate is below the minimum or if its
  // currently unknown.
  if (EffectiveDegradationPreference(input_mode) ==
      DegradationPreference::MAINTAIN_RESOLUTION) {
    // TODO(hbos): This usage of |last_adaptation_was_down| looks like a mistake
    // - delete it.
    if (input_fps <= 0 ||
        (last_adaptation_was_down && input_fps < kMinFramerateFps)) {
      return absl::nullopt;
    }
  }
  // 3. We shouldn't adapt down if we're currently waiting for a previous
  // downgrade to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  if (last_adaptation_was_down &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels >= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference(input_mode)) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int target_fps = balanced_settings_.MinFps(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels);
      if (source_restrictor_->CanDecreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kDecreaseFrameRate,
                                target_fps);
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      int target_pixels = GetLowerResolutionThan(input_pixels);
      // TODO(https://crbug.com/webrtc/11393): Move this logic to
      // ApplyAdaptationTarget() or elsewhere - simply checking which adaptation
      // target is available should not have side-effects.
      if (target_pixels < min_pixels_per_frame)
        encoder_stats_observer->OnMinPixelLimitReached();
      if (!source_restrictor_->CanDecreaseResolutionTo(target_pixels,
                                                       min_pixels_per_frame)) {
        return absl::nullopt;
      }
      return AdaptationTarget(AdaptationAction::kDecreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      int target_fps = GetLowerFrameRateThan(input_fps);
      if (!source_restrictor_->CanDecreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kDecreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return absl::nullopt;
  }
}

ResourceListenerResponse VideoStreamAdapter::ApplyAdaptationTarget(
    const AdaptationTarget& target,
    const absl::optional<EncoderSettings>& encoder_settings,
    VideoInputMode input_mode,
    int input_pixels,
    int input_fps) {
  const int min_pixels_per_frame = MinPixelsPerFrame(encoder_settings);
  // Remember the input pixels and fps of this adaptation. Used to avoid
  // adapting again before this adaptation has had an effect.
  last_adaptation_request_.emplace(AdaptationRequest{
      input_pixels, input_fps,
      AdaptationRequest::GetModeFromAdaptationAction(target.action)});
  // Adapt!
  switch (target.action) {
    case AdaptationAction::kIncreaseResolution:
      source_restrictor_->IncreaseResolutionTo(target.value);
      break;
    case AdaptationAction::kDecreaseResolution:
      source_restrictor_->DecreaseResolutionTo(target.value,
                                               min_pixels_per_frame);
      break;
    case AdaptationAction::kIncreaseFrameRate:
      source_restrictor_->IncreaseFrameRateTo(target.value);
      // TODO(https://crbug.com/webrtc/11222): Don't adapt in two steps.
      // GetAdaptUpTarget() should tell us the correct value, but BALANCED logic
      // in DecrementFramerate() makes it hard to predict whether this will be
      // the last step. Remove the dependency on GetConstAdaptCounter().
      if (EffectiveDegradationPreference(input_mode) ==
              DegradationPreference::BALANCED &&
          source_restrictor_->adaptation_counters().fps_adaptations == 0 &&
          target.value != std::numeric_limits<int>::max()) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        source_restrictor_->IncreaseFrameRateTo(
            std::numeric_limits<int>::max());
      }
      break;
    case AdaptationAction::kDecreaseFrameRate:
      source_restrictor_->DecreaseFrameRateTo(target.value);
      break;
  }
  // In BALANCED, if requested FPS is higher or close to input FPS to the target
  // we tell the QualityScaler to increase its frequency.
  // TODO(hbos): Don't have QualityScaler-specific logic here. If the
  // QualityScaler wants to add special logic depending on what effects
  // adaptation had, it should listen to changes to the VideoSourceRestrictions
  // instead.
  if (EffectiveDegradationPreference(input_mode) ==
          DegradationPreference::BALANCED &&
      target.action ==
          VideoStreamAdapter::AdaptationAction::kDecreaseFrameRate) {
    absl::optional<int> min_diff = balanced_settings_.MinFpsDiff(input_pixels);
    if (min_diff && input_fps > 0) {
      int fps_diff = input_fps - target.value;
      if (fps_diff < min_diff.value()) {
        return ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
      }
    }
  }
  return ResourceListenerResponse::kNothing;
}

DegradationPreference VideoStreamAdapter::EffectiveDegradationPreference(
    VideoInputMode input_mode) const {
  // Balanced mode for screenshare works via automatic animation detection:
  // Resolution is capped for fullscreen animated content.
  // Adapatation is done only via framerate downgrade.
  // Thus effective degradation preference is MAINTAIN_RESOLUTION.
  return (input_mode == VideoInputMode::kScreenshareVideo &&
          degradation_preference_ == DegradationPreference::BALANCED)
             ? DegradationPreference::MAINTAIN_RESOLUTION
             : degradation_preference_;
}

}  // namespace webrtc
