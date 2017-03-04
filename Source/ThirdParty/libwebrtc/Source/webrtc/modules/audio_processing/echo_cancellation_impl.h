/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_

#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class AudioBuffer;

class EchoCancellationImpl : public EchoCancellation {
 public:
  EchoCancellationImpl(rtc::CriticalSection* crit_render,
                       rtc::CriticalSection* crit_capture);
  ~EchoCancellationImpl() override;

  void ProcessRenderAudio(rtc::ArrayView<const float> packed_render_audio);
  int ProcessCaptureAudio(AudioBuffer* audio, int stream_delay_ms);

  // EchoCancellation implementation.
  bool is_enabled() const override;
  int stream_drift_samples() const override;
  SuppressionLevel suppression_level() const override;
  bool is_drift_compensation_enabled() const override;

  void Initialize(int sample_rate_hz,
                  size_t num_reverse_channels_,
                  size_t num_output_channels_,
                  size_t num_proc_channels_);
  void SetExtraOptions(const webrtc::Config& config);
  bool is_delay_agnostic_enabled() const;
  bool is_extended_filter_enabled() const;
  std::string GetExperimentsDescription();
  bool is_refined_adaptive_filter_enabled() const;

  // Returns the system delay of the first AEC component.
  int GetSystemDelayInSamples() const;

  static void PackRenderAudioBuffer(const AudioBuffer* audio,
                                    size_t num_output_channels,
                                    size_t num_channels,
                                    std::vector<float>* packed_buffer);
  static size_t NumCancellersRequired(size_t num_output_channels,
                                      size_t num_reverse_channels);

  // Enable logging of various AEC statistics.
  int enable_metrics(bool enable) override;

  // Provides various statistics about the AEC.
  int GetMetrics(Metrics* metrics) override;

  // Enable logging of delay metrics.
  int enable_delay_logging(bool enable) override;

  // Provides delay metrics.
  int GetDelayMetrics(int* median,
                      int* std,
                      float* fraction_poor_delays) override;

 private:
  class Canceller;
  struct StreamProperties;

  // EchoCancellation implementation.
  int Enable(bool enable) override;
  int enable_drift_compensation(bool enable) override;
  void set_stream_drift_samples(int drift) override;
  int set_suppression_level(SuppressionLevel level) override;
  bool are_metrics_enabled() const override;
  bool stream_has_echo() const override;
  bool is_delay_logging_enabled() const override;
  int GetDelayMetrics(int* median, int* std) override;

  struct AecCore* aec_core() const override;

  void AllocateRenderQueue();
  int Configure();

  rtc::CriticalSection* const crit_render_ ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection* const crit_capture_;

  bool enabled_ = false;
  bool drift_compensation_enabled_ GUARDED_BY(crit_capture_);
  bool metrics_enabled_ GUARDED_BY(crit_capture_);
  SuppressionLevel suppression_level_ GUARDED_BY(crit_capture_);
  int stream_drift_samples_ GUARDED_BY(crit_capture_);
  bool was_stream_drift_set_ GUARDED_BY(crit_capture_);
  bool stream_has_echo_ GUARDED_BY(crit_capture_);
  bool delay_logging_enabled_ GUARDED_BY(crit_capture_);
  bool extended_filter_enabled_ GUARDED_BY(crit_capture_);
  bool delay_agnostic_enabled_ GUARDED_BY(crit_capture_);
  bool refined_adaptive_filter_enabled_ GUARDED_BY(crit_capture_) = false;

  std::vector<std::unique_ptr<Canceller>> cancellers_;
  std::unique_ptr<StreamProperties> stream_properties_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoCancellationImpl);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
