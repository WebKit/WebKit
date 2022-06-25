/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_ENCODER_H_
#define VIDEO_VIDEO_STREAM_ENCODER_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/adaptation/resource.h"
#include "api/sequence_checker.h"
#include "api/units/data_rate.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_stream_encoder_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/resource_adaptation_processor.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "modules/video_coding/utility/frame_dropper.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "video/adaptation/video_stream_encoder_resource_manager.h"
#include "video/encoder_bitrate_adjuster.h"
#include "video/frame_encode_metadata_writer.h"
#include "video/video_source_sink_controller.h"

namespace webrtc {

// VideoStreamEncoder represent a video encoder that accepts raw video frames as
// input and produces an encoded bit stream.
// Usage:
//  Instantiate.
//  Call SetSink.
//  Call SetSource.
//  Call ConfigureEncoder with the codec settings.
//  Call Stop() when done.
class VideoStreamEncoder : public VideoStreamEncoderInterface,
                           private EncodedImageCallback,
                           public VideoSourceRestrictionsListener {
 public:
  // TODO(bugs.webrtc.org/12000): Reporting of VideoBitrateAllocation is being
  // deprecated. Instead VideoLayersAllocation should be reported.
  enum class BitrateAllocationCallbackType {
    kVideoBitrateAllocation,
    kVideoBitrateAllocationWhenScreenSharing,
    kVideoLayersAllocation
  };
  VideoStreamEncoder(Clock* clock,
                     uint32_t number_of_cores,
                     VideoStreamEncoderObserver* encoder_stats_observer,
                     const VideoStreamEncoderSettings& settings,
                     std::unique_ptr<OveruseFrameDetector> overuse_detector,
                     TaskQueueFactory* task_queue_factory,
                     BitrateAllocationCallbackType allocation_cb_type);
  ~VideoStreamEncoder() override;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;
  std::vector<rtc::scoped_refptr<Resource>> GetAdaptationResources() override;

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override;

  void SetSink(EncoderSink* sink, bool rotation_applied) override;

  // TODO(perkj): Can we remove VideoCodec.startBitrate ?
  void SetStartBitrate(int start_bitrate_bps) override;

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length) override;

  // Permanently stop encoding. After this method has returned, it is
  // guaranteed that no encoded frames will be delivered to the sink.
  void Stop() override;

  void SendKeyFrame() override;

  void OnLossNotification(
      const VideoEncoder::LossNotification& loss_notification) override;

  void OnBitrateUpdated(DataRate target_bitrate,
                        DataRate stable_target_bitrate,
                        DataRate target_headroom,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms,
                        double cwnd_reduce_ratio) override;

  DataRate UpdateTargetBitrate(DataRate target_bitrate,
                               double cwnd_reduce_ratio);

 protected:
  // Used for testing. For example the `ScalingObserverInterface` methods must
  // be called on `encoder_queue_`.
  rtc::TaskQueue* encoder_queue() { return &encoder_queue_; }

  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) override;

  // Used for injected test resources.
  // TODO(eshr): Move all adaptation tests out of VideoStreamEncoder tests.
  void InjectAdaptationResource(rtc::scoped_refptr<Resource> resource,
                                VideoAdaptationReason reason);
  void InjectAdaptationConstraint(AdaptationConstraint* adaptation_constraint);

  void AddRestrictionsListenerForTesting(
      VideoSourceRestrictionsListener* restrictions_listener);
  void RemoveRestrictionsListenerForTesting(
      VideoSourceRestrictionsListener* restrictions_listener);

 private:
  class VideoFrameInfo {
   public:
    VideoFrameInfo(int width, int height, bool is_texture)
        : width(width), height(height), is_texture(is_texture) {}
    int width;
    int height;
    bool is_texture;
    int pixel_count() const { return width * height; }
  };

  struct EncoderRateSettings {
    EncoderRateSettings();
    EncoderRateSettings(const VideoBitrateAllocation& bitrate,
                        double framerate_fps,
                        DataRate bandwidth_allocation,
                        DataRate encoder_target,
                        DataRate stable_encoder_target);
    bool operator==(const EncoderRateSettings& rhs) const;
    bool operator!=(const EncoderRateSettings& rhs) const;

    VideoEncoder::RateControlParameters rate_control;
    // This is the scalar target bitrate before the VideoBitrateAllocator, i.e.
    // the `target_bitrate` argument of the OnBitrateUpdated() method. This is
    // needed because the bitrate allocator may truncate the total bitrate and a
    // later call to the same allocator instance, e.g.
    // |using last_encoder_rate_setings_->bitrate.get_sum_bps()|, may trick it
    // into thinking the available bitrate has decreased since the last call.
    DataRate encoder_target;
    DataRate stable_encoder_target;
  };

  class DegradationPreferenceManager;

  void ReconfigureEncoder() RTC_RUN_ON(&encoder_queue_);
  void OnEncoderSettingsChanged() RTC_RUN_ON(&encoder_queue_);

  // Implements VideoSinkInterface.
  void OnFrame(const VideoFrame& video_frame) override;
  void OnDiscardedFrame() override;

  void MaybeEncodeVideoFrame(const VideoFrame& frame,
                             int64_t time_when_posted_in_ms);

  void EncodeVideoFrame(const VideoFrame& frame,
                        int64_t time_when_posted_in_ms);
  // Indicates whether frame should be dropped because the pixel count is too
  // large for the current bitrate configuration.
  bool DropDueToSize(uint32_t pixel_count) const RTC_RUN_ON(&encoder_queue_);

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info) override;

  void OnDroppedFrame(EncodedImageCallback::DropReason reason) override;

  bool EncoderPaused() const;
  void TraceFrameDropStart();
  void TraceFrameDropEnd();

  // Returns a copy of `rate_settings` with the `bitrate` field updated using
  // the current VideoBitrateAllocator.
  EncoderRateSettings UpdateBitrateAllocation(
      const EncoderRateSettings& rate_settings) RTC_RUN_ON(&encoder_queue_);

  uint32_t GetInputFramerateFps() RTC_RUN_ON(&encoder_queue_);
  void SetEncoderRates(const EncoderRateSettings& rate_settings)
      RTC_RUN_ON(&encoder_queue_);

  void RunPostEncode(const EncodedImage& encoded_image,
                     int64_t time_sent_us,
                     int temporal_index,
                     DataSize frame_size);
  bool HasInternalSource() const RTC_RUN_ON(&encoder_queue_);
  void ReleaseEncoder() RTC_RUN_ON(&encoder_queue_);
  // After calling this function `resource_adaptation_processor_` will be null.
  void ShutdownResourceAdaptationQueue();

  void CheckForAnimatedContent(const VideoFrame& frame,
                               int64_t time_when_posted_in_ms)
      RTC_RUN_ON(&encoder_queue_);

  // TODO(bugs.webrtc.org/11341) : Remove this version of RequestEncoderSwitch.
  void QueueRequestEncoderSwitch(
      const EncoderSwitchRequestCallback::Config& conf)
      RTC_RUN_ON(&encoder_queue_);
  void QueueRequestEncoderSwitch(const webrtc::SdpVideoFormat& format)
      RTC_RUN_ON(&encoder_queue_);

  TaskQueueBase* const main_queue_;

  const uint32_t number_of_cores_;

  EncoderSink* sink_;
  const VideoStreamEncoderSettings settings_;
  const BitrateAllocationCallbackType allocation_cb_type_;
  const RateControlSettings rate_control_settings_;

  std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface> const
      encoder_selector_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;

  VideoEncoderConfig encoder_config_ RTC_GUARDED_BY(&encoder_queue_);
  std::unique_ptr<VideoEncoder> encoder_ RTC_GUARDED_BY(&encoder_queue_)
      RTC_PT_GUARDED_BY(&encoder_queue_);
  bool encoder_initialized_;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_
      RTC_GUARDED_BY(&encoder_queue_) RTC_PT_GUARDED_BY(&encoder_queue_);
  int max_framerate_ RTC_GUARDED_BY(&encoder_queue_);

  // Set when ConfigureEncoder has been called in order to lazy reconfigure the
  // encoder on the next frame.
  bool pending_encoder_reconfiguration_ RTC_GUARDED_BY(&encoder_queue_);
  // Set when configuration must create a new encoder object, e.g.,
  // because of a codec change.
  bool pending_encoder_creation_ RTC_GUARDED_BY(&encoder_queue_);

  absl::optional<VideoFrameInfo> last_frame_info_
      RTC_GUARDED_BY(&encoder_queue_);
  int crop_width_ RTC_GUARDED_BY(&encoder_queue_);
  int crop_height_ RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<uint32_t> encoder_target_bitrate_bps_
      RTC_GUARDED_BY(&encoder_queue_);
  size_t max_data_payload_length_ RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<EncoderRateSettings> last_encoder_rate_settings_
      RTC_GUARDED_BY(&encoder_queue_);
  bool encoder_paused_and_dropped_frame_ RTC_GUARDED_BY(&encoder_queue_);

  // Set to true if at least one frame was sent to encoder since last encoder
  // initialization.
  bool was_encode_called_since_last_initialization_
      RTC_GUARDED_BY(&encoder_queue_);

  bool encoder_failed_ RTC_GUARDED_BY(&encoder_queue_);
  Clock* const clock_;

  rtc::RaceChecker incoming_frame_race_checker_
      RTC_GUARDED_BY(incoming_frame_race_checker_);
  std::atomic<int> posted_frames_waiting_for_encode_;
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_ RTC_GUARDED_BY(incoming_frame_race_checker_);
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_
      RTC_GUARDED_BY(incoming_frame_race_checker_);

  int64_t last_frame_log_ms_ RTC_GUARDED_BY(incoming_frame_race_checker_);
  int captured_frame_count_ RTC_GUARDED_BY(&encoder_queue_);
  int dropped_frame_cwnd_pushback_count_ RTC_GUARDED_BY(&encoder_queue_);
  int dropped_frame_encoder_block_count_ RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<VideoFrame> pending_frame_ RTC_GUARDED_BY(&encoder_queue_);
  int64_t pending_frame_post_time_us_ RTC_GUARDED_BY(&encoder_queue_);

  VideoFrame::UpdateRect accumulated_update_rect_
      RTC_GUARDED_BY(&encoder_queue_);
  bool accumulated_update_rect_is_valid_ RTC_GUARDED_BY(&encoder_queue_);

  // Used for automatic content type detection.
  absl::optional<VideoFrame::UpdateRect> last_update_rect_
      RTC_GUARDED_BY(&encoder_queue_);
  Timestamp animation_start_time_ RTC_GUARDED_BY(&encoder_queue_);
  bool cap_resolution_due_to_video_content_ RTC_GUARDED_BY(&encoder_queue_);
  // Used to correctly ignore changes in update_rect introduced by
  // resize triggered by animation detection.
  enum class ExpectResizeState {
    kNoResize,              // Normal operation.
    kResize,                // Resize was triggered by the animation detection.
    kFirstFrameAfterResize  // Resize observed.
  } expect_resize_state_ RTC_GUARDED_BY(&encoder_queue_);

  FecControllerOverride* fec_controller_override_
      RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<int64_t> last_parameters_update_ms_
      RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<int64_t> last_encode_info_ms_ RTC_GUARDED_BY(&encoder_queue_);

  VideoEncoder::EncoderInfo encoder_info_ RTC_GUARDED_BY(&encoder_queue_);
  VideoEncoderFactory::CodecInfo codec_info_ RTC_GUARDED_BY(&encoder_queue_);
  VideoCodec send_codec_ RTC_GUARDED_BY(&encoder_queue_);

  FrameDropper frame_dropper_ RTC_GUARDED_BY(&encoder_queue_);
  // If frame dropper is not force disabled, frame dropping might still be
  // disabled if VideoEncoder::GetEncoderInfo() indicates that the encoder has a
  // trusted rate controller. This is determined on a per-frame basis, as the
  // encoder behavior might dynamically change.
  bool force_disable_frame_dropper_ RTC_GUARDED_BY(&encoder_queue_);
  RateStatistics input_framerate_ RTC_GUARDED_BY(&encoder_queue_);
  // Incremented on worker thread whenever `frame_dropper_` determines that a
  // frame should be dropped. Decremented on whichever thread runs
  // OnEncodedImage(), which is only called by one thread but not necessarily
  // the worker thread.
  std::atomic<int> pending_frame_drops_;

  // Congestion window frame drop ratio (drop 1 in every
  // cwnd_frame_drop_interval_ frames).
  absl::optional<int> cwnd_frame_drop_interval_ RTC_GUARDED_BY(&encoder_queue_);
  // Frame counter for congestion window frame drop.
  int cwnd_frame_counter_ RTC_GUARDED_BY(&encoder_queue_);

  std::unique_ptr<EncoderBitrateAdjuster> bitrate_adjuster_
      RTC_GUARDED_BY(&encoder_queue_);

  // TODO(sprang): Change actually support keyframe per simulcast stream, or
  // turn this into a simple bool `pending_keyframe_request_`.
  std::vector<VideoFrameType> next_frame_types_ RTC_GUARDED_BY(&encoder_queue_);

  FrameEncodeMetadataWriter frame_encode_metadata_writer_;

  // Experiment groups parsed from field trials for realtime video ([0]) and
  // screenshare ([1]). 0 means no group specified. Positive values are
  // experiment group numbers incremented by 1.
  const std::array<uint8_t, 2> experiment_groups_;

  struct AutomaticAnimationDetectionExperiment {
    bool enabled = false;
    int min_duration_ms = 2000;
    double min_area_ratio = 0.8;
    int min_fps = 10;
    std::unique_ptr<StructParametersParser> Parser() {
      return StructParametersParser::Create(
          "enabled", &enabled,                  //
          "min_duration_ms", &min_duration_ms,  //
          "min_area_ratio", &min_area_ratio,    //
          "min_fps", &min_fps);
    }
  };

  AutomaticAnimationDetectionExperiment
  ParseAutomatincAnimationDetectionFieldTrial() const;

  AutomaticAnimationDetectionExperiment
      automatic_animation_detection_experiment_ RTC_GUARDED_BY(&encoder_queue_);

  // Provides video stream input states: current resolution and frame rate.
  VideoStreamInputStateProvider input_state_provider_;

  std::unique_ptr<VideoStreamAdapter> video_stream_adapter_
      RTC_GUARDED_BY(&encoder_queue_);
  // Responsible for adapting input resolution or frame rate to ensure resources
  // (e.g. CPU or bandwidth) are not overused. Adding resources can occur on any
  // thread.
  std::unique_ptr<ResourceAdaptationProcessorInterface>
      resource_adaptation_processor_;
  std::unique_ptr<DegradationPreferenceManager> degradation_preference_manager_
      RTC_GUARDED_BY(&encoder_queue_);
  std::vector<AdaptationConstraint*> adaptation_constraints_
      RTC_GUARDED_BY(&encoder_queue_);
  // Handles input, output and stats reporting related to VideoStreamEncoder
  // specific resources, such as "encode usage percent" measurements and "QP
  // scaling". Also involved with various mitigations such as initial frame
  // dropping.
  // The manager primarily operates on the `encoder_queue_` but its lifetime is
  // tied to the VideoStreamEncoder (which is destroyed off the encoder queue)
  // and its resource list is accessible from any thread.
  VideoStreamEncoderResourceManager stream_resource_manager_
      RTC_GUARDED_BY(&encoder_queue_);
  std::vector<rtc::scoped_refptr<Resource>> additional_resources_
      RTC_GUARDED_BY(&encoder_queue_);
  // Carries out the VideoSourceRestrictions provided by the
  // ResourceAdaptationProcessor, i.e. reconfigures the source of video frames
  // to provide us with different resolution or frame rate.
  // This class is thread-safe.
  VideoSourceSinkController video_source_sink_controller_
      RTC_GUARDED_BY(main_queue_);

  // Default bitrate limits in EncoderInfoSettings allowed.
  const bool default_limits_allowed_;

  // QP parser is used to extract QP value from encoded frame when that is not
  // provided by encoder.
  QpParser qp_parser_;
  const bool qp_parsing_allowed_;

  // Public methods are proxied to the task queues. The queues must be destroyed
  // first to make sure no tasks run that use other members.
  rtc::TaskQueue encoder_queue_;

  // Used to cancel any potentially pending tasks to the main thread.
  ScopedTaskSafety task_safety_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoStreamEncoder);
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_ENCODER_H_
