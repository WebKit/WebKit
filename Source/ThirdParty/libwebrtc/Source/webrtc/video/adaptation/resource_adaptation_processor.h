/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
#define VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "rtc_base/experiments/quality_rampup_experiment.h"
#include "rtc_base/experiments/quality_scaler_settings.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "video/adaptation/adaptation_counters.h"
#include "video/adaptation/encode_usage_resource.h"
#include "video/adaptation/overuse_frame_detector.h"
#include "video/adaptation/quality_scaler_resource.h"
#include "video/adaptation/video_stream_adapter.h"

namespace webrtc {

// The assumed input frame size if we have not yet received a frame.
// TODO(hbos): This is 144p - why are we assuming super low quality? Seems like
// a bad heuristic.
extern const int kDefaultInputPixelsWidth;
extern const int kDefaultInputPixelsHeight;

// This class is used by the VideoStreamEncoder and is responsible for adapting
// resolution up or down based on encode usage percent. It keeps track of video
// source settings, adaptation counters and may get influenced by
// VideoStreamEncoder's quality scaler through AdaptUp() and AdaptDown() calls.
//
// This class is single-threaded. The caller is responsible for ensuring safe
// usage.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly in video_stream_encoder_unittest.cc and other tests exercising
// VideoStreamEncoder.
class ResourceAdaptationProcessor : public ResourceAdaptationProcessorInterface,
                                    public ResourceListener {
 public:
  // The processor can be constructed on any sequence, but must be initialized
  // and used on a single sequence, e.g. the encoder queue.
  ResourceAdaptationProcessor(
      Clock* clock,
      bool experiment_cpu_load_estimator,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      VideoStreamEncoderObserver* encoder_stats_observer,
      ResourceAdaptationProcessorListener* adaptation_listener);
  ~ResourceAdaptationProcessor() override;

  DegradationPreference degradation_preference() const {
    return degradation_preference_;
  }

  // ResourceAdaptationProcessorInterface implementation.
  void StartResourceAdaptation(
      ResourceAdaptationProcessorListener* adaptation_listener) override;
  void StopResourceAdaptation() override;
  // Uses a default AdaptReason of kCpu.
  void AddResource(Resource* resource) override;
  void AddResource(Resource* resource,
                   AdaptationObserverInterface::AdaptReason reason);
  void SetHasInputVideo(bool has_input_video) override;
  void SetDegradationPreference(
      DegradationPreference degradation_preference) override;
  void SetEncoderSettings(EncoderSettings encoder_settings) override;
  void SetStartBitrate(DataRate start_bitrate) override;
  void SetTargetBitrate(DataRate target_bitrate) override;
  void SetEncoderRates(
      const VideoEncoder::RateControlParameters& encoder_rates) override;

  void OnFrame(const VideoFrame& frame) override;
  void OnFrameDroppedDueToSize() override;
  void OnMaybeEncodeFrame() override;
  void OnEncodeStarted(const VideoFrame& cropped_frame,
                       int64_t time_when_first_seen_us) override;
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us,
                         absl::optional<int> encode_duration_us) override;
  void OnFrameDropped(EncodedImageCallback::DropReason reason) override;

  // TODO(hbos): Is dropping initial frames really just a special case of "don't
  // encode frames right now"? Can this be part of VideoSourceRestrictions,
  // which handles the output of the rest of the encoder settings? This is
  // something we'll need to support for "disable video due to overuse", not
  // initial frames.
  bool DropInitialFrames() const;

  // TODO(eshr): This can be made private if we configure on
  // SetDegredationPreference and SetEncoderSettings.
  // (https://crbug.com/webrtc/11338)
  void ConfigureQualityScaler(const VideoEncoder::EncoderInfo& encoder_info);

  // ResourceUsageListener implementation.
  ResourceListenerResponse OnResourceUsageStateMeasured(
      const Resource& resource) override;

  // For reasons of adaptation and statistics, we not only count the total
  // number of adaptations, but we also count the number of adaptations per
  // reason.
  // This method takes the new total number of adaptations and allocates that to
  // the "active" count - number of adaptations for the current reason.
  // The "other" count is the number of adaptations for the other reason.
  // This must be called for each adaptation step made.
  static void OnAdaptationCountChanged(
      const AdaptationCounters& adaptation_count,
      AdaptationCounters* active_count,
      AdaptationCounters* other_active);

 private:
  class InitialFrameDropper;

  enum class State { kStopped, kStarted };

  // Performs the adaptation by getting the next target, applying it and
  // informing listeners of the new VideoSourceRestriction and adapt counters.
  void OnResourceUnderuse(AdaptationObserverInterface::AdaptReason reason);
  ResourceListenerResponse OnResourceOveruse(
      AdaptationObserverInterface::AdaptReason reason);

  CpuOveruseOptions GetCpuOveruseOptions() const;
  int LastInputFrameSizeOrDefault() const;
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptationObserverInterface::AdaptReason reason);
  VideoStreamAdapter::VideoInputMode GetVideoInputMode() const;

  // Makes |video_source_restrictions_| up-to-date and informs the
  // |adaptation_listener_| if restrictions are changed, allowing the listener
  // to reconfigure the source accordingly.
  void MaybeUpdateVideoSourceRestrictions();
  // Calculates an up-to-date value of the target frame rate and informs the
  // |encode_usage_resource_| of the new value.
  void MaybeUpdateTargetFrameRate();

  // Use nullopt to disable quality scaling.
  void UpdateQualityScalerSettings(
      absl::optional<VideoEncoder::QpThresholds> qp_thresholds);

  void UpdateAdaptationStats(AdaptationObserverInterface::AdaptReason reason);

  // Checks to see if we should execute the quality rampup experiment. The
  // experiment resets all video restrictions at the start of the call in the
  // case the bandwidth estimate is high enough.
  // TODO(https://crbug.com/webrtc/11222) Move experiment details into an inner
  // class.
  void MaybePerformQualityRampupExperiment();
  void ResetVideoSourceRestrictions();

  std::string ActiveCountsToString() const;

  ResourceAdaptationProcessorListener* const adaptation_listener_;
  Clock* clock_;
  State state_;
  const bool experiment_cpu_load_estimator_;
  // The restrictions that |adaptation_listener_| is informed of.
  VideoSourceRestrictions video_source_restrictions_;
  bool has_input_video_;
  // TODO(https://crbug.com/webrtc/11393): DegradationPreference has mostly
  // moved to VideoStreamAdapter. Move it entirely and delete it from this
  // class. If the responsibility of generating next steps for adaptations is
  // owned by the adapter, this class has no buisness relying on implementation
  // details of the adapter.
  DegradationPreference degradation_preference_;
  // Keeps track of source restrictions that this adaptation processor outputs.
  const std::unique_ptr<VideoStreamAdapter> stream_adapter_;
  const std::unique_ptr<EncodeUsageResource> encode_usage_resource_;
  const std::unique_ptr<QualityScalerResource> quality_scaler_resource_;
  const std::unique_ptr<InitialFrameDropper> initial_frame_dropper_;
  const bool quality_scaling_experiment_enabled_;
  absl::optional<int> last_input_frame_size_;
  absl::optional<double> target_frame_rate_;
  // This is the last non-zero target bitrate for the encoder.
  absl::optional<uint32_t> encoder_target_bitrate_bps_;
  absl::optional<VideoEncoder::RateControlParameters> encoder_rates_;
  bool quality_rampup_done_;
  QualityRampupExperiment quality_rampup_experiment_;
  absl::optional<EncoderSettings> encoder_settings_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;

  // Ties a resource to a reason for statistical reporting. This AdaptReason is
  // also used by this module to make decisions about how to adapt up/down.
  struct ResourceAndReason {
    ResourceAndReason(Resource* resource,
                      AdaptationObserverInterface::AdaptReason reason)
        : resource(resource), reason(reason) {}
    virtual ~ResourceAndReason() = default;

    Resource* const resource;
    const AdaptationObserverInterface::AdaptReason reason;
  };
  std::vector<ResourceAndReason> resources_;
  // One AdaptationCounter for each reason, tracking the number of times we have
  // adapted for each reason. The sum of active_counts_ MUST always equal the
  // total adaptation provided by the VideoSourceRestrictions.
  // TODO(https://crbug.com/webrtc/11392): Move all active count logic to
  // encoder_stats_observer_; Counters used for deciding if the video resolution
  // or framerate is currently restricted, and if so, why, on a per degradation
  // preference basis.
  std::array<AdaptationCounters, AdaptationObserverInterface::kScaleReasonSize>
      active_counts_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
