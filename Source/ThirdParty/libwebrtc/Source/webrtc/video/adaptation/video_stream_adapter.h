/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
#define VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_stream_encoder_observer.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "video/adaptation/adaptation_counters.h"

namespace webrtc {

// Owns the VideoSourceRestriction for a single stream and is responsible for
// adapting it up or down when told to do so. This class serves the following
// purposes:
// 1. Keep track of a stream's restrictions.
// 2. Provide valid ways to adapt up or down the stream's restrictions.
// 3. Modify the stream's restrictions in one of the valid ways.
class VideoStreamAdapter {
 public:
  enum class SetDegradationPreferenceResult {
    kRestrictionsNotCleared,
    kRestrictionsCleared,
  };

  enum class VideoInputMode {
    kNoVideo,
    kNormalVideo,
    kScreenshareVideo,
  };

  enum class AdaptationAction {
    kIncreaseResolution,
    kDecreaseResolution,
    kIncreaseFrameRate,
    kDecreaseFrameRate,
  };

  // Describes an adaptation step: increasing or decreasing resolution or frame
  // rate to a given value.
  // TODO(https://crbug.com/webrtc/11393): Make these private implementation
  // details, and expose something that allows you to inspect the
  // VideoSourceRestrictions instead. The adaptation steps could be expressed as
  // a graph, for instance.
  struct AdaptationTarget {
    AdaptationTarget(AdaptationAction action, int value);
    // Which action the VideoSourceRestrictor needs to take.
    const AdaptationAction action;
    // Target pixel count or frame rate depending on |action|.
    const int value;

    // Allow this struct to be instantiated as an optional, even though it's in
    // a private namespace.
    friend class absl::optional<AdaptationTarget>;
  };

  VideoStreamAdapter();
  ~VideoStreamAdapter();

  VideoSourceRestrictions source_restrictions() const;
  const AdaptationCounters& adaptation_counters() const;
  // TODO(hbos): Can we get rid of any external dependencies on
  // BalancedDegradationPreference? How the adaptor generates possible next
  // steps for adaptation should be an implementation detail. Can the relevant
  // information be inferred from GetAdaptUpTarget()/GetAdaptDownTarget()?
  const BalancedDegradationSettings& balanced_settings() const;
  void ClearRestrictions();

  // TODO(hbos): Setting the degradation preference should not clear
  // restrictions! This is not defined in the spec and is unexpected, there is a
  // tiny risk that people would discover and rely on this behavior.
  SetDegradationPreferenceResult SetDegradationPreference(
      DegradationPreference degradation_preference);

  // Returns a target that we are guaranteed to be able to adapt to, or null if
  // adaptation is not desired or not possible.
  absl::optional<AdaptationTarget> GetAdaptUpTarget(
      const absl::optional<EncoderSettings>& encoder_settings,
      absl::optional<uint32_t> encoder_target_bitrate_bps,
      VideoInputMode input_mode,
      int input_pixels,
      int input_fps,
      AdaptationObserverInterface::AdaptReason reason) const;
  // TODO(https://crbug.com/webrtc/11393): Remove the dependency on
  // |encoder_stats_observer| - simply checking which adaptation target is
  // available should not have side-effects.
  absl::optional<AdaptationTarget> GetAdaptDownTarget(
      const absl::optional<EncoderSettings>& encoder_settings,
      VideoInputMode input_mode,
      int input_pixels,
      int input_fps,
      VideoStreamEncoderObserver* encoder_stats_observer) const;
  // Applies the |target| to |source_restrictor_|.
  // TODO(hbos): Delete ResourceListenerResponse!
  ResourceListenerResponse ApplyAdaptationTarget(
      const AdaptationTarget& target,
      const absl::optional<EncoderSettings>& encoder_settings,
      VideoInputMode input_mode,
      int input_pixels,
      int input_fps);

 private:
  class VideoSourceRestrictor;

  // The input frame rate and resolution at the time of an adaptation in the
  // direction described by |mode_| (up or down).
  // TODO(https://crbug.com/webrtc/11393): Can this be renamed? Can this be
  // merged with AdaptationTarget?
  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;

    // This is a static method rather than an anonymous namespace function due
    // to namespace visiblity.
    static Mode GetModeFromAdaptationAction(AdaptationAction action);
  };

  // Reinterprets "balanced + screenshare" as "maintain-resolution".
  // TODO(hbos): Don't do this. This is not what "balanced" means. If the
  // application wants to maintain resolution it should set that degradation
  // preference rather than depend on non-standard behaviors.
  DegradationPreference EffectiveDegradationPreference(
      VideoInputMode input_mode) const;

  // Owner and modifier of the VideoSourceRestriction of this stream adaptor.
  const std::unique_ptr<VideoSourceRestrictor> source_restrictor_;
  // Decides the next adaptation target in DegradationPreference::BALANCED.
  const BalancedDegradationSettings balanced_settings_;
  // When deciding the next target up or down, different strategies are used
  // depending on the DegradationPreference.
  // https://w3c.github.io/mst-content-hint/#dom-rtcdegradationpreference
  DegradationPreference degradation_preference_;

  // The input frame rate, resolution and adaptation direction of the last
  // ApplyAdaptationTarget(). Used to avoid adapting twice if a recent
  // adaptation has not had an effect on the input frame rate or resolution yet.
  // TODO(hbos): Can we implement a more general "cooldown" mechanism of
  // resources intead? If we already have adapted it seems like we should wait
  // a while before adapting again, so that we are not acting on usage
  // measurements that are made obsolete/unreliable by an "ongoing" adaptation.
  absl::optional<AdaptationRequest> last_adaptation_request_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
