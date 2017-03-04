/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_

#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/modules/audio_processing/echo_detector/circular_buffer.h"
#include "webrtc/modules/audio_processing/echo_detector/mean_variance_estimator.h"
#include "webrtc/modules/audio_processing/echo_detector/moving_max.h"
#include "webrtc/modules/audio_processing/echo_detector/normalized_covariance_estimator.h"

namespace webrtc {

class AudioBuffer;
class EchoDetector;

class ResidualEchoDetector {
 public:
  ResidualEchoDetector();
  ~ResidualEchoDetector();

  // This function should be called while holding the render lock.
  void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio);

  // This function should be called while holding the capture lock.
  void AnalyzeCaptureAudio(rtc::ArrayView<const float> capture_audio);

  // This function should be called while holding the capture lock.
  void Initialize();

  // This function is for testing purposes only.
  void SetReliabilityForTest(float value) { reliability_ = value; }

  static void PackRenderAudioBuffer(AudioBuffer* audio,
                                    std::vector<float>* packed_buffer);

  // This function should be called while holding the capture lock.
  float echo_likelihood() const { return echo_likelihood_; }

  float echo_likelihood_recent_max() const {
    return recent_likelihood_max_.max();
  }

 private:
  // Keep track if the |Process| function has been previously called.
  bool first_process_call_ = true;
  // Buffer for storing the power of incoming farend buffers. This is needed for
  // cases where calls to BufferFarend and Process are jittery.
  CircularBuffer render_buffer_;
  // Count how long ago it was that the size of |render_buffer_| was zero. This
  // value is also reset to zero when clock drift is detected and a value from
  // the renderbuffer is discarded, even though the buffer is not actually zero
  // at that point. This is done to avoid repeatedly removing elements in this
  // situation.
  size_t frames_since_zero_buffer_size_ = 0;

  // Circular buffers containing delayed versions of the power, mean and
  // standard deviation, for calculating the delayed covariance values.
  std::vector<float> render_power_;
  std::vector<float> render_power_mean_;
  std::vector<float> render_power_std_dev_;
  // Covariance estimates for different delay values.
  std::vector<NormalizedCovarianceEstimator> covariances_;
  // Index where next element should be inserted in all of the above circular
  // buffers.
  size_t next_insertion_index_ = 0;

  MeanVarianceEstimator render_statistics_;
  MeanVarianceEstimator capture_statistics_;
  // Current echo likelihood.
  float echo_likelihood_ = 0.f;
  // Reliability of the current likelihood.
  float reliability_ = 0.f;
  MovingMax recent_likelihood_max_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_
