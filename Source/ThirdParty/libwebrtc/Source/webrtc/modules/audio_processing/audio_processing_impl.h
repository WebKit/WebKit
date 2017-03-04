/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/function_view.h"
#include "webrtc/base/gtest_prod_util.h"
#include "webrtc/base/ignore_wundef.h"
#include "webrtc/base/swap_queue.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/render_queue_item_verifier.h"
#include "webrtc/modules/audio_processing/rms_level.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/modules/audio_processing/debug.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

namespace webrtc {

class AgcManagerDirect;
class AudioConverter;

class NonlinearBeamformer;

class AudioProcessingImpl : public AudioProcessing {
 public:
  // Methods forcing APM to run in a single-threaded manner.
  // Acquires both the render and capture locks.
  explicit AudioProcessingImpl(const webrtc::Config& config);
  // AudioProcessingImpl takes ownership of beamformer.
  AudioProcessingImpl(const webrtc::Config& config,
                      NonlinearBeamformer* beamformer);
  ~AudioProcessingImpl() override;
  int Initialize() override;
  int Initialize(int capture_input_sample_rate_hz,
                 int capture_output_sample_rate_hz,
                 int render_sample_rate_hz,
                 ChannelLayout capture_input_layout,
                 ChannelLayout capture_output_layout,
                 ChannelLayout render_input_layout) override;
  int Initialize(const ProcessingConfig& processing_config) override;
  void ApplyConfig(const AudioProcessing::Config& config) override;
  void SetExtraOptions(const webrtc::Config& config) override;
  void UpdateHistogramsOnCallEnd() override;
  int StartDebugRecording(const char filename[kMaxFilenameSize],
                          int64_t max_log_size_bytes) override;
  int StartDebugRecording(FILE* handle, int64_t max_log_size_bytes) override;
  int StartDebugRecording(FILE* handle) override;
  int StartDebugRecordingForPlatformFile(rtc::PlatformFile handle) override;
  int StopDebugRecording() override;

  // Capture-side exclusive methods possibly running APM in a
  // multi-threaded manner. Acquire the capture lock.
  int ProcessStream(AudioFrame* frame) override;
  int ProcessStream(const float* const* src,
                    size_t samples_per_channel,
                    int input_sample_rate_hz,
                    ChannelLayout input_layout,
                    int output_sample_rate_hz,
                    ChannelLayout output_layout,
                    float* const* dest) override;
  int ProcessStream(const float* const* src,
                    const StreamConfig& input_config,
                    const StreamConfig& output_config,
                    float* const* dest) override;
  void set_output_will_be_muted(bool muted) override;
  int set_stream_delay_ms(int delay) override;
  void set_delay_offset_ms(int offset) override;
  int delay_offset_ms() const override;
  void set_stream_key_pressed(bool key_pressed) override;

  // Render-side exclusive methods possibly running APM in a
  // multi-threaded manner. Acquire the render lock.
  int ProcessReverseStream(AudioFrame* frame) override;
  int AnalyzeReverseStream(const float* const* data,
                           size_t samples_per_channel,
                           int sample_rate_hz,
                           ChannelLayout layout) override;
  int ProcessReverseStream(const float* const* src,
                           const StreamConfig& input_config,
                           const StreamConfig& output_config,
                           float* const* dest) override;

  // Methods only accessed from APM submodules or
  // from AudioProcessing tests in a single-threaded manner.
  // Hence there is no need for locks in these.
  int proc_sample_rate_hz() const override;
  int proc_split_sample_rate_hz() const override;
  size_t num_input_channels() const override;
  size_t num_proc_channels() const override;
  size_t num_output_channels() const override;
  size_t num_reverse_channels() const override;
  int stream_delay_ms() const override;
  bool was_stream_delay_set() const override
      EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  AudioProcessingStatistics GetStatistics() const override;

  // Methods returning pointers to APM submodules.
  // No locks are aquired in those, as those locks
  // would offer no protection (the submodules are
  // created only once in a single-treaded manner
  // during APM creation).
  EchoCancellation* echo_cancellation() const override;
  EchoControlMobile* echo_control_mobile() const override;
  GainControl* gain_control() const override;
  // TODO(peah): Deprecate this API call.
  HighPassFilter* high_pass_filter() const override;
  LevelEstimator* level_estimator() const override;
  NoiseSuppression* noise_suppression() const override;
  VoiceDetection* voice_detection() const override;

  // TODO(peah): Remove these two methods once the new API allows that.
  void MutateConfig(rtc::FunctionView<void(AudioProcessing::Config*)> mutator);
  AudioProcessing::Config GetConfig() const;

 protected:
  // Overridden in a mock.
  virtual int InitializeLocked()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);

 private:
  // TODO(peah): These friend classes should be removed as soon as the new
  // parameter setting scheme allows.
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, DefaultBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, ValidConfigBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, InValidConfigBehavior);
  struct ApmPublicSubmodules;
  struct ApmPrivateSubmodules;

  // Submodule interface implementations.
  std::unique_ptr<HighPassFilter> high_pass_filter_impl_;

  class ApmSubmoduleStates {
   public:
    ApmSubmoduleStates();
    // Updates the submodule state and returns true if it has changed.
    bool Update(bool low_cut_filter_enabled,
                bool echo_canceller_enabled,
                bool mobile_echo_controller_enabled,
                bool residual_echo_detector_enabled,
                bool noise_suppressor_enabled,
                bool intelligibility_enhancer_enabled,
                bool beamformer_enabled,
                bool adaptive_gain_controller_enabled,
                bool level_controller_enabled,
                bool echo_canceller3_enabled,
                bool voice_activity_detector_enabled,
                bool level_estimator_enabled,
                bool transient_suppressor_enabled);
    bool CaptureMultiBandSubModulesActive() const;
    bool CaptureMultiBandProcessingActive() const;
    bool RenderMultiBandSubModulesActive() const;
    bool RenderMultiBandProcessingActive() const;

   private:
    bool low_cut_filter_enabled_ = false;
    bool echo_canceller_enabled_ = false;
    bool mobile_echo_controller_enabled_ = false;
    bool residual_echo_detector_enabled_ = false;
    bool noise_suppressor_enabled_ = false;
    bool intelligibility_enhancer_enabled_ = false;
    bool beamformer_enabled_ = false;
    bool adaptive_gain_controller_enabled_ = false;
    bool level_controller_enabled_ = false;
    bool echo_canceller3_enabled_ = false;
    bool level_estimator_enabled_ = false;
    bool voice_activity_detector_enabled_ = false;
    bool transient_suppressor_enabled_ = false;
    bool first_update_ = true;
  };

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // State for the debug dump.
  struct ApmDebugDumpThreadState {
    ApmDebugDumpThreadState();
    ~ApmDebugDumpThreadState();
    std::unique_ptr<audioproc::Event> event_msg;  // Protobuf message.
    std::string event_str;  // Memory for protobuf serialization.

    // Serialized string of last saved APM configuration.
    std::string last_serialized_config;
  };

  struct ApmDebugDumpState {
    ApmDebugDumpState();
    ~ApmDebugDumpState();
    // Number of bytes that can still be written to the log before the maximum
    // size is reached. A value of <= 0 indicates that no limit is used.
    int64_t num_bytes_left_for_log_ = -1;
    std::unique_ptr<FileWrapper> debug_file;
    ApmDebugDumpThreadState render;
    ApmDebugDumpThreadState capture;
  };
#endif

  // Method for modifying the formats struct that are called from both
  // the render and capture threads. The check for whether modifications
  // are needed is done while holding the render lock only, thereby avoiding
  // that the capture thread blocks the render thread.
  // The struct is modified in a single-threaded manner by holding both the
  // render and capture locks.
  int MaybeInitialize(const ProcessingConfig& config, bool force_initialization)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  int MaybeInitializeRender(const ProcessingConfig& processing_config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  int MaybeInitializeCapture(const ProcessingConfig& processing_config,
                             bool force_initialization)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Method for updating the state keeping track of the active submodules.
  // Returns a bool indicating whether the state has changed.
  bool UpdateActiveSubmoduleStates() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Methods requiring APM running in a single-threaded manner.
  // Are called with both the render and capture locks already
  // acquired.
  void InitializeTransient()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeBeamformer()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeIntelligibility()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  int InitializeLocked(const ProcessingConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeLevelController() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeResidualEchoDetector()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void InitializeLowCutFilter() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void InitializeEchoCanceller3() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  void EmptyQueuedRenderAudio();
  void AllocateRenderQueue()
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);
  void QueueRenderAudio(AudioBuffer* audio)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

  // Capture-side exclusive methods possibly running APM in a multi-threaded
  // manner that are called with the render lock already acquired.
  int ProcessCaptureStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);
  void MaybeUpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Render-side exclusive methods possibly running APM in a multi-threaded
  // manner that are called with the render lock already acquired.
  // TODO(ekm): Remove once all clients updated to new interface.
  int AnalyzeReverseStreamLocked(const float* const* src,
                                 const StreamConfig& input_config,
                                 const StreamConfig& output_config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_render_);
  int ProcessRenderStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_render_);

// Debug dump methods that are internal and called without locks.
// TODO(peah): Make thread safe.
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // TODO(andrew): make this more graceful. Ideally we would split this stuff
  // out into a separate class with an "enabled" and "disabled" implementation.
  static int WriteMessageToDebugFile(FileWrapper* debug_file,
                                     int64_t* filesize_limit_bytes,
                                     rtc::CriticalSection* crit_debug,
                                     ApmDebugDumpThreadState* debug_state);
  int WriteInitMessage() EXCLUSIVE_LOCKS_REQUIRED(crit_render_, crit_capture_);

  // Writes Config message. If not |forced|, only writes the current config if
  // it is different from the last saved one; if |forced|, writes the config
  // regardless of the last saved.
  int WriteConfigMessage(bool forced) EXCLUSIVE_LOCKS_REQUIRED(crit_capture_)
      EXCLUSIVE_LOCKS_REQUIRED(crit_capture_);

  // Critical section.
  rtc::CriticalSection crit_debug_;

  // Debug dump state.
  ApmDebugDumpState debug_dump_;
#endif

  // Critical sections.
  rtc::CriticalSection crit_render_ ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection crit_capture_;

  // Struct containing the Config specifying the behavior of APM.
  AudioProcessing::Config config_;

  // Class containing information about what submodules are active.
  ApmSubmoduleStates submodule_states_;

  // Structs containing the pointers to the submodules.
  std::unique_ptr<ApmPublicSubmodules> public_submodules_;
  std::unique_ptr<ApmPrivateSubmodules> private_submodules_;

  // State that is written to while holding both the render and capture locks
  // but can be read without any lock being held.
  // As this is only accessed internally of APM, and all internal methods in APM
  // either are holding the render or capture locks, this construct is safe as
  // it is not possible to read the variables while writing them.
  struct ApmFormatState {
    ApmFormatState()
        :  // Format of processing streams at input/output call sites.
          api_format({{{kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false},
                       {kSampleRate16kHz, 1, false}}}),
          render_processing_format(kSampleRate16kHz, 1) {}
    ProcessingConfig api_format;
    StreamConfig render_processing_format;
  } formats_;

  // APM constants.
  const struct ApmConstants {
    ApmConstants(int agc_startup_min_volume,
                 int agc_clipped_level_min,
                 bool use_experimental_agc)
        :  // Format of processing streams at input/output call sites.
          agc_startup_min_volume(agc_startup_min_volume),
          agc_clipped_level_min(agc_clipped_level_min),
          use_experimental_agc(use_experimental_agc) {}
    int agc_startup_min_volume;
    int agc_clipped_level_min;
    bool use_experimental_agc;
  } constants_;

  struct ApmCaptureState {
    ApmCaptureState(bool transient_suppressor_enabled,
                    const std::vector<Point>& array_geometry,
                    SphericalPointf target_direction);
    ~ApmCaptureState();
    int aec_system_delay_jumps;
    int delay_offset_ms;
    bool was_stream_delay_set;
    int last_stream_delay_ms;
    int last_aec_system_delay_ms;
    int stream_delay_jumps;
    bool output_will_be_muted;
    bool key_pressed;
    bool transient_suppressor_enabled;
    std::vector<Point> array_geometry;
    SphericalPointf target_direction;
    std::unique_ptr<AudioBuffer> capture_audio;
    // Only the rate and samples fields of capture_processing_format_ are used
    // because the capture processing number of channels is mutable and is
    // tracked by the capture_audio_.
    StreamConfig capture_processing_format;
    int split_rate;
  } capture_ GUARDED_BY(crit_capture_);

  struct ApmCaptureNonLockedState {
    ApmCaptureNonLockedState(bool beamformer_enabled,
                             bool intelligibility_enabled)
        : capture_processing_format(kSampleRate16kHz),
          split_rate(kSampleRate16kHz),
          stream_delay_ms(0),
          beamformer_enabled(beamformer_enabled),
          intelligibility_enabled(intelligibility_enabled) {}
    // Only the rate and samples fields of capture_processing_format_ are used
    // because the forward processing number of channels is mutable and is
    // tracked by the capture_audio_.
    StreamConfig capture_processing_format;
    int split_rate;
    int stream_delay_ms;
    bool beamformer_enabled;
    bool intelligibility_enabled;
    bool level_controller_enabled = false;
    bool echo_canceller3_enabled = false;
  } capture_nonlocked_;

  struct ApmRenderState {
    ApmRenderState();
    ~ApmRenderState();
    std::unique_ptr<AudioConverter> render_converter;
    std::unique_ptr<AudioBuffer> render_audio;
  } render_ GUARDED_BY(crit_render_);

  size_t aec_render_queue_element_max_size_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_) = 0;
  std::vector<float> aec_render_queue_buffer_ GUARDED_BY(crit_render_);
  std::vector<float> aec_capture_queue_buffer_ GUARDED_BY(crit_capture_);

  size_t aecm_render_queue_element_max_size_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_) = 0;
  std::vector<int16_t> aecm_render_queue_buffer_ GUARDED_BY(crit_render_);
  std::vector<int16_t> aecm_capture_queue_buffer_ GUARDED_BY(crit_capture_);

  size_t agc_render_queue_element_max_size_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_) = 0;
  std::vector<int16_t> agc_render_queue_buffer_ GUARDED_BY(crit_render_);
  std::vector<int16_t> agc_capture_queue_buffer_ GUARDED_BY(crit_capture_);

  size_t red_render_queue_element_max_size_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_) = 0;
  std::vector<float> red_render_queue_buffer_ GUARDED_BY(crit_render_);
  std::vector<float> red_capture_queue_buffer_ GUARDED_BY(crit_capture_);

  RmsLevel capture_input_rms_ GUARDED_BY(crit_capture_);
  RmsLevel capture_output_rms_ GUARDED_BY(crit_capture_);
  int capture_rms_interval_counter_ GUARDED_BY(crit_capture_) = 0;

  // Lock protection not needed.
  std::unique_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      aec_render_signal_queue_;
  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      aecm_render_signal_queue_;
  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      agc_render_signal_queue_;
  std::unique_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      red_render_signal_queue_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
