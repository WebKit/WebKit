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

#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_stream_encoder_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/utility/frame_dropper.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "rtc_base/experiments/quality_scaler_settings.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"
#include "video/encoder_bitrate_adjuster.h"
#include "video/frame_encode_metadata_writer.h"
#include "video/overuse_frame_detector.h"

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
                           // Protected only to provide access to tests.
                           protected AdaptationObserverInterface {
 public:
  VideoStreamEncoder(Clock* clock,
                     uint32_t number_of_cores,
                     VideoStreamEncoderObserver* encoder_stats_observer,
                     const VideoStreamEncoderSettings& settings,
                     std::unique_ptr<OveruseFrameDetector> overuse_detector,
                     TaskQueueFactory* task_queue_factory);
  ~VideoStreamEncoder() override;

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override;

  void SetSink(EncoderSink* sink, bool rotation_applied) override;

  // TODO(perkj): Can we remove VideoCodec.startBitrate ?
  void SetStartBitrate(int start_bitrate_bps) override;

  void SetBitrateAllocationObserver(
      VideoBitrateAllocationObserver* bitrate_observer) override;

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
                        DataRate target_headroom,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms) override;

 protected:
  // Used for testing. For example the |ScalingObserverInterface| methods must
  // be called on |encoder_queue_|.
  rtc::TaskQueue* encoder_queue() { return &encoder_queue_; }

  // AdaptationObserverInterface implementation.
  // These methods are protected for easier testing.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

 private:
  class VideoSourceProxy;

  class VideoFrameInfo {
   public:
    VideoFrameInfo(int width, int height, bool is_texture)
        : width(width), height(height), is_texture(is_texture) {}
    int width;
    int height;
    bool is_texture;
    int pixel_count() const { return width * height; }
  };

  struct EncoderRateSettings : public VideoEncoder::RateControlParameters {
    EncoderRateSettings();
    EncoderRateSettings(const VideoBitrateAllocation& bitrate,
                        double framerate_fps,
                        DataRate bandwidth_allocation,
                        DataRate encoder_target);
    bool operator==(const EncoderRateSettings& rhs) const;
    bool operator!=(const EncoderRateSettings& rhs) const;

    // This is the scalar target bitrate before the VideoBitrateAllocator, i.e.
    // the |target_bitrate| argument of the OnBitrateUpdated() method. This is
    // needed because the bitrate allocator may truncate the total bitrate and a
    // later call to the same allocator instance, e.g.
    // |using last_encoder_rate_setings_->bitrate.get_sum_bps()|, may trick it
    // into thinking the available bitrate has decreased since the last call.
    DataRate encoder_target;
  };

  void ConfigureEncoderOnTaskQueue(VideoEncoderConfig config,
                                   size_t max_data_payload_length);
  void ReconfigureEncoder() RTC_RUN_ON(&encoder_queue_);

  void ConfigureQualityScaler(const VideoEncoder::EncoderInfo& encoder_info);

  // Implements VideoSinkInterface.
  void OnFrame(const VideoFrame& video_frame) override;
  void OnDiscardedFrame() override;

  void MaybeEncodeVideoFrame(const VideoFrame& frame,
                             int64_t time_when_posted_in_ms);

  void EncodeVideoFrame(const VideoFrame& frame,
                        int64_t time_when_posted_in_ms);
  // Indicates wether frame should be dropped because the pixel count is too
  // large for the current bitrate configuration.
  bool DropDueToSize(uint32_t pixel_count) const RTC_RUN_ON(&encoder_queue_);

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void OnDroppedFrame(EncodedImageCallback::DropReason reason) override;

  bool EncoderPaused() const;
  void TraceFrameDropStart();
  void TraceFrameDropEnd();

  // Returns a copy of |rate_settings| with the |bitrate| field updated using
  // the current VideoBitrateAllocator, and notifies any listeners of the new
  // allocation.
  EncoderRateSettings UpdateBitrateAllocationAndNotifyObserver(
      const EncoderRateSettings& rate_settings) RTC_RUN_ON(&encoder_queue_);

  uint32_t GetInputFramerateFps() RTC_RUN_ON(&encoder_queue_);
  void SetEncoderRates(const EncoderRateSettings& rate_settings)
      RTC_RUN_ON(&encoder_queue_);

  // Class holding adaptation information.
  class AdaptCounter final {
   public:
    AdaptCounter();
    ~AdaptCounter();

    // Get number of adaptation downscales for |reason|.
    VideoStreamEncoderObserver::AdaptationSteps Counts(int reason) const;

    std::string ToString() const;

    void IncrementFramerate(int reason);
    void IncrementResolution(int reason);
    void DecrementFramerate(int reason);
    void DecrementResolution(int reason);
    void DecrementFramerate(int reason, int cur_fps);

    // Gets the total number of downgrades (for all adapt reasons).
    int FramerateCount() const;
    int ResolutionCount() const;

    // Gets the total number of downgrades for |reason|.
    int FramerateCount(int reason) const;
    int ResolutionCount(int reason) const;
    int TotalCount(int reason) const;

   private:
    std::string ToString(const std::vector<int>& counters) const;
    int Count(const std::vector<int>& counters) const;
    void MoveCount(std::vector<int>* counters, int from_reason);

    // Degradation counters holding number of framerate/resolution reductions
    // per adapt reason.
    std::vector<int> fps_counters_;
    std::vector<int> resolution_counters_;
  };

  AdaptCounter& GetAdaptCounter() RTC_RUN_ON(&encoder_queue_);
  const AdaptCounter& GetConstAdaptCounter() RTC_RUN_ON(&encoder_queue_);
  void UpdateAdaptationStats(AdaptReason reason) RTC_RUN_ON(&encoder_queue_);
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptReason reason) RTC_RUN_ON(&encoder_queue_);
  void RunPostEncode(EncodedImage encoded_image,
                     int64_t time_sent_us,
                     int temporal_index);
  bool HasInternalSource() const RTC_RUN_ON(&encoder_queue_);
  void ReleaseEncoder() RTC_RUN_ON(&encoder_queue_);

  rtc::Event shutdown_event_;

  const uint32_t number_of_cores_;
  // Counts how many frames we've dropped in the initial framedrop phase.
  int initial_framedrop_;
  const bool initial_framedrop_on_bwe_enabled_;
  bool has_seen_first_significant_bwe_change_ = false;

  const bool quality_scaling_experiment_enabled_;

  const std::unique_ptr<VideoSourceProxy> source_proxy_;
  EncoderSink* sink_;
  const VideoStreamEncoderSettings settings_;
  const RateControlSettings rate_control_settings_;
  const QualityScalerSettings quality_scaler_settings_;

  const std::unique_ptr<OveruseFrameDetector> overuse_detector_
      RTC_PT_GUARDED_BY(&encoder_queue_);
  std::unique_ptr<QualityScaler> quality_scaler_ RTC_GUARDED_BY(&encoder_queue_)
      RTC_PT_GUARDED_BY(&encoder_queue_);

  VideoStreamEncoderObserver* const encoder_stats_observer_;
  // |thread_checker_| checks that public methods that are related to lifetime
  // of VideoStreamEncoder are called on the same thread.
  rtc::ThreadChecker thread_checker_;

  VideoEncoderConfig encoder_config_ RTC_GUARDED_BY(&encoder_queue_);
  std::unique_ptr<VideoEncoder> encoder_ RTC_GUARDED_BY(&encoder_queue_)
      RTC_PT_GUARDED_BY(&encoder_queue_);
  bool encoder_initialized_;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_
      RTC_GUARDED_BY(&encoder_queue_) RTC_PT_GUARDED_BY(&encoder_queue_);
  // The maximum frame rate of the current codec configuration, as determined
  // at the last ReconfigureEncoder() call.
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
  uint32_t encoder_start_bitrate_bps_ RTC_GUARDED_BY(&encoder_queue_);
  int set_start_bitrate_bps_ RTC_GUARDED_BY(&encoder_queue_);
  int64_t set_start_bitrate_time_ms_ RTC_GUARDED_BY(&encoder_queue_);
  bool has_seen_first_bwe_drop_ RTC_GUARDED_BY(&encoder_queue_);
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
  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const DegradationPreference, AdaptCounter> adapt_counters_
      RTC_GUARDED_BY(&encoder_queue_);
  // Set depending on degradation preferences.
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(&encoder_queue_);

  const BalancedDegradationSettings balanced_settings_;

  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;
  };
  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  absl::optional<AdaptationRequest> last_adaptation_request_
      RTC_GUARDED_BY(&encoder_queue_);

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
  int dropped_frame_count_ RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<VideoFrame> pending_frame_ RTC_GUARDED_BY(&encoder_queue_);
  int64_t pending_frame_post_time_us_ RTC_GUARDED_BY(&encoder_queue_);

  VideoFrame::UpdateRect accumulated_update_rect_
      RTC_GUARDED_BY(&encoder_queue_);

  VideoBitrateAllocationObserver* bitrate_observer_
      RTC_GUARDED_BY(&encoder_queue_);
  FecControllerOverride* fec_controller_override_
      RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<int64_t> last_parameters_update_ms_
      RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<int64_t> last_encode_info_ms_ RTC_GUARDED_BY(&encoder_queue_);

  VideoEncoder::EncoderInfo encoder_info_ RTC_GUARDED_BY(&encoder_queue_);
  absl::optional<VideoEncoder::ResolutionBitrateLimits> encoder_bitrate_limits_
      RTC_GUARDED_BY(&encoder_queue_);
  VideoEncoderFactory::CodecInfo codec_info_ RTC_GUARDED_BY(&encoder_queue_);
  VideoCodec send_codec_ RTC_GUARDED_BY(&encoder_queue_);

  FrameDropper frame_dropper_ RTC_GUARDED_BY(&encoder_queue_);
  // If frame dropper is not force disabled, frame dropping might still be
  // disabled if VideoEncoder::GetEncoderInfo() indicates that the encoder has a
  // trusted rate controller. This is determined on a per-frame basis, as the
  // encoder behavior might dynamically change.
  bool force_disable_frame_dropper_ RTC_GUARDED_BY(&encoder_queue_);
  RateStatistics input_framerate_ RTC_GUARDED_BY(&encoder_queue_);
  // Incremented on worker thread whenever |frame_dropper_| determines that a
  // frame should be dropped. Decremented on whichever thread runs
  // OnEncodedImage(), which is only called by one thread but not necessarily
  // the worker thread.
  std::atomic<int> pending_frame_drops_;

  std::unique_ptr<EncoderBitrateAdjuster> bitrate_adjuster_
      RTC_GUARDED_BY(&encoder_queue_);

  // TODO(sprang): Change actually support keyframe per simulcast stream, or
  // turn this into a simple bool |pending_keyframe_request_|.
  std::vector<VideoFrameType> next_frame_types_ RTC_GUARDED_BY(&encoder_queue_);

  FrameEncodeMetadataWriter frame_encode_metadata_writer_;

  // Experiment groups parsed from field trials for realtime video ([0]) and
  // screenshare ([1]). 0 means no group specified. Positive values are
  // experiment group numbers incremented by 1.
  const std::array<uint8_t, 2> experiment_groups_;

  // TODO(philipel): Remove this lock and run on |encoder_queue_| instead.
  rtc::CriticalSection encoded_image_lock_;

  int64_t next_frame_id_ RTC_GUARDED_BY(encoded_image_lock_);

  // This array is used as a map from simulcast id to an encoder's buffer
  // state. For every buffer of the encoder we keep track of the last frame id
  // that updated that buffer.
  std::array<std::array<int64_t, kMaxEncoderBuffers>, kMaxSimulcastStreams>
      encoder_buffer_state_ RTC_GUARDED_BY(encoded_image_lock_);

  // All public methods are proxied to |encoder_queue_|. It must must be
  // destroyed first to make sure no tasks are run that use other members.
  rtc::TaskQueue encoder_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoStreamEncoder);
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_ENCODER_H_
