/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_DEFINES_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_DEFINES_H_

#include <stddef.h>

#include "webrtc/typedefs.h"

namespace webrtc {

static const int kAdmMaxDeviceNameSize = 128;
static const int kAdmMaxFileNameSize = 512;
static const int kAdmMaxGuidSize = 128;

static const int kAdmMinPlayoutBufferSizeMs = 10;
static const int kAdmMaxPlayoutBufferSizeMs = 250;

// ----------------------------------------------------------------------------
//  AudioDeviceObserver
// ----------------------------------------------------------------------------

class AudioDeviceObserver {
 public:
  enum ErrorCode { kRecordingError = 0, kPlayoutError = 1 };
  enum WarningCode { kRecordingWarning = 0, kPlayoutWarning = 1 };

  virtual void OnErrorIsReported(const ErrorCode error) = 0;
  virtual void OnWarningIsReported(const WarningCode warning) = 0;

 protected:
  virtual ~AudioDeviceObserver() {}
};

// ----------------------------------------------------------------------------
//  AudioTransport
// ----------------------------------------------------------------------------

class AudioTransport {
 public:
  virtual int32_t RecordedDataIsAvailable(const void* audioSamples,
                                          const size_t nSamples,
                                          const size_t nBytesPerSample,
                                          const size_t nChannels,
                                          const uint32_t samplesPerSec,
                                          const uint32_t totalDelayMS,
                                          const int32_t clockDrift,
                                          const uint32_t currentMicLevel,
                                          const bool keyPressed,
                                          uint32_t& newMicLevel) = 0;

  virtual int32_t NeedMorePlayData(const size_t nSamples,
                                   const size_t nBytesPerSample,
                                   const size_t nChannels,
                                   const uint32_t samplesPerSec,
                                   void* audioSamples,
                                   size_t& nSamplesOut,
                                   int64_t* elapsed_time_ms,
                                   int64_t* ntp_time_ms) = 0;

  // Method to push the captured audio data to the specific VoE channel.
  // The data will not undergo audio processing.
  // |voe_channel| is the id of the VoE channel which is the sink to the
  // capture data.
  virtual void PushCaptureData(int voe_channel,
                               const void* audio_data,
                               int bits_per_sample,
                               int sample_rate,
                               size_t number_of_channels,
                               size_t number_of_frames) = 0;

  // Method to pull mixed render audio data from all active VoE channels.
  // The data will not be passed as reference for audio processing internally.
  // TODO(xians): Support getting the unmixed render data from specific VoE
  // channel.
  virtual void PullRenderData(int bits_per_sample,
                              int sample_rate,
                              size_t number_of_channels,
                              size_t number_of_frames,
                              void* audio_data,
                              int64_t* elapsed_time_ms,
                              int64_t* ntp_time_ms) = 0;

 protected:
  virtual ~AudioTransport() {}
};

// Helper class for storage of fundamental audio parameters such as sample rate,
// number of channels, native buffer size etc.
// Note that one audio frame can contain more than one channel sample and each
// sample is assumed to be a 16-bit PCM sample. Hence, one audio frame in
// stereo contains 2 * (16/8) = 4 bytes of data.
class AudioParameters {
 public:
  // This implementation does only support 16-bit PCM samples.
  static const size_t kBitsPerSample = 16;
  AudioParameters()
      : sample_rate_(0),
        channels_(0),
        frames_per_buffer_(0),
        frames_per_10ms_buffer_(0) {}
  AudioParameters(int sample_rate, size_t channels, size_t frames_per_buffer)
      : sample_rate_(sample_rate),
        channels_(channels),
        frames_per_buffer_(frames_per_buffer),
        frames_per_10ms_buffer_(static_cast<size_t>(sample_rate / 100)) {}
  void reset(int sample_rate, size_t channels, size_t frames_per_buffer) {
    sample_rate_ = sample_rate;
    channels_ = channels;
    frames_per_buffer_ = frames_per_buffer;
    frames_per_10ms_buffer_ = static_cast<size_t>(sample_rate / 100);
  }
  size_t bits_per_sample() const { return kBitsPerSample; }
  void reset(int sample_rate, size_t channels, double ms_per_buffer) {
    reset(sample_rate, channels,
          static_cast<size_t>(sample_rate * ms_per_buffer + 0.5));
  }
  void reset(int sample_rate, size_t channels) {
    reset(sample_rate, channels, static_cast<size_t>(0));
  }
  int sample_rate() const { return sample_rate_; }
  size_t channels() const { return channels_; }
  size_t frames_per_buffer() const { return frames_per_buffer_; }
  size_t frames_per_10ms_buffer() const { return frames_per_10ms_buffer_; }
  size_t GetBytesPerFrame() const { return channels_ * kBitsPerSample / 8; }
  size_t GetBytesPerBuffer() const {
    return frames_per_buffer_ * GetBytesPerFrame();
  }
  // The WebRTC audio device buffer (ADB) only requires that the sample rate
  // and number of channels are configured. Hence, to be "valid", only these
  // two attributes must be set.
  bool is_valid() const { return ((sample_rate_ > 0) && (channels_ > 0)); }
  // Most platforms also require that a native buffer size is defined.
  // An audio parameter instance is considered to be "complete" if it is both
  // "valid" (can be used by the ADB) and also has a native frame size.
  bool is_complete() const { return (is_valid() && (frames_per_buffer_ > 0)); }
  size_t GetBytesPer10msBuffer() const {
    return frames_per_10ms_buffer_ * GetBytesPerFrame();
  }
  double GetBufferSizeInMilliseconds() const {
    if (sample_rate_ == 0)
      return 0.0;
    return frames_per_buffer_ / (sample_rate_ / 1000.0);
  }
  double GetBufferSizeInSeconds() const {
    if (sample_rate_ == 0)
      return 0.0;
    return static_cast<double>(frames_per_buffer_) / (sample_rate_);
  }

 private:
  int sample_rate_;
  size_t channels_;
  size_t frames_per_buffer_;
  size_t frames_per_10ms_buffer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_DEFINES_H_
