/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AEC_DUMP_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AEC_DUMP_H_

#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"

namespace webrtc {

class AudioFrame;

// Struct for passing current config from APM without having to
// include protobuf headers.
struct InternalAPMConfig {
  InternalAPMConfig();
  InternalAPMConfig(const InternalAPMConfig&);
  InternalAPMConfig(InternalAPMConfig&&);

  InternalAPMConfig& operator=(const InternalAPMConfig&);
  InternalAPMConfig& operator=(InternalAPMConfig&&) = delete;

  bool operator==(const InternalAPMConfig& other);

  bool aec_enabled = false;
  bool aec_delay_agnostic_enabled = false;
  bool aec_drift_compensation_enabled = false;
  bool aec_extended_filter_enabled = false;
  int aec_suppression_level = 0;
  bool aecm_enabled = false;
  bool aecm_comfort_noise_enabled = false;
  int aecm_routing_mode = 0;
  bool agc_enabled = false;
  int agc_mode = 0;
  bool agc_limiter_enabled = false;
  bool hpf_enabled = false;
  bool ns_enabled = false;
  int ns_level = 0;
  bool transient_suppression_enabled = false;
  bool intelligibility_enhancer_enabled = false;
  bool noise_robust_agc_enabled = false;
  std::string experiments_description = "";
};

struct InternalAPMStreamsConfig {
  int input_sample_rate = 0;
  int output_sample_rate = 0;
  int render_input_sample_rate = 0;
  int render_output_sample_rate = 0;

  size_t input_num_channels = 0;
  size_t output_num_channels = 0;
  size_t render_input_num_channels = 0;
  size_t render_output_num_channels = 0;
};

// Class to pass audio data in float** format. This is to avoid
// dependence on AudioBuffer, and avoid problems associated with
// rtc::ArrayView<rtc::ArrayView>.
class FloatAudioFrame {
 public:
  // |num_channels| and |channel_size| describe the float**
  // |audio_samples|. |audio_samples| is assumed to point to a
  // two-dimensional |num_channels * channel_size| array of floats.
  FloatAudioFrame(const float* const* audio_samples,
                  size_t num_channels,
                  size_t channel_size)
      : audio_samples_(audio_samples),
        num_channels_(num_channels),
        channel_size_(channel_size) {}

  FloatAudioFrame() = delete;

  size_t num_channels() const { return num_channels_; }

  rtc::ArrayView<const float> channel(size_t idx) const {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<const float>(audio_samples_[idx], channel_size_);
  }

 private:
  const float* const* audio_samples_;
  size_t num_channels_;
  size_t channel_size_;
};

// An interface for recording configuration and input/output streams
// of the Audio Processing Module. The recordings are called
// 'aec-dumps' and are stored in a protobuf format defined in
// debug.proto.
// The Write* methods are always safe to call concurrently or
// otherwise for all implementing subclasses. The intended mode of
// operation is to create a protobuf object from the input, and send
// it away to be written to file asynchronously.
class AecDump {
 public:
  struct AudioProcessingState {
    int delay;
    int drift;
    int level;
    bool keypress;
  };

  virtual ~AecDump() = default;

  // Logs Event::Type INIT message.
  virtual void WriteInitMessage(
      const InternalAPMStreamsConfig& streams_config) = 0;

  // Logs Event::Type STREAM message. To log an input/output pair,
  // call the AddCapture* and AddAudioProcessingState methods followed
  // by a WriteCaptureStreamMessage call.
  virtual void AddCaptureStreamInput(const FloatAudioFrame& src) = 0;
  virtual void AddCaptureStreamOutput(const FloatAudioFrame& src) = 0;
  virtual void AddCaptureStreamInput(const AudioFrame& frame) = 0;
  virtual void AddCaptureStreamOutput(const AudioFrame& frame) = 0;
  virtual void AddAudioProcessingState(const AudioProcessingState& state) = 0;
  virtual void WriteCaptureStreamMessage() = 0;

  // Logs Event::Type REVERSE_STREAM message.
  virtual void WriteRenderStreamMessage(const AudioFrame& frame) = 0;
  virtual void WriteRenderStreamMessage(const FloatAudioFrame& src) = 0;

  // Logs Event::Type CONFIG message.
  virtual void WriteConfig(const InternalAPMConfig& config) = 0;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AEC_DUMP_H_
