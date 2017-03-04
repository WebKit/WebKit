/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/residual_echo_detector.h"

#include <algorithm>
#include <numeric>

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace {

float Power(rtc::ArrayView<const float> input) {
  return std::inner_product(input.begin(), input.end(), input.begin(), 0.f);
}

constexpr size_t kLookbackFrames = 650;
// TODO(ivoc): Verify the size of this buffer.
constexpr size_t kRenderBufferSize = 30;
constexpr float kAlpha = 0.001f;
// 10 seconds of data, updated every 10 ms.
constexpr size_t kAggregationBufferSize = 10 * 100;

}  // namespace

namespace webrtc {

ResidualEchoDetector::ResidualEchoDetector()
    : render_buffer_(kRenderBufferSize),
      render_power_(kLookbackFrames),
      render_power_mean_(kLookbackFrames),
      render_power_std_dev_(kLookbackFrames),
      covariances_(kLookbackFrames),
      recent_likelihood_max_(kAggregationBufferSize) {}

ResidualEchoDetector::~ResidualEchoDetector() = default;

void ResidualEchoDetector::AnalyzeRenderAudio(
    rtc::ArrayView<const float> render_audio) {
  if (render_buffer_.Size() == 0) {
    frames_since_zero_buffer_size_ = 0;
  } else if (frames_since_zero_buffer_size_ >= kRenderBufferSize) {
    // This can happen in a few cases: at the start of a call, due to a glitch
    // or due to clock drift. The excess capture value will be ignored.
    // TODO(ivoc): Include how often this happens in APM stats.
    render_buffer_.Pop();
    frames_since_zero_buffer_size_ = 0;
  }
  ++frames_since_zero_buffer_size_;
  float power = Power(render_audio);
  render_buffer_.Push(power);
}

void ResidualEchoDetector::AnalyzeCaptureAudio(
    rtc::ArrayView<const float> capture_audio) {
  if (first_process_call_) {
    // On the first process call (so the start of a call), we must flush the
    // render buffer, otherwise the render data will be delayed.
    render_buffer_.Clear();
    first_process_call_ = false;
  }

  // Get the next render value.
  const rtc::Optional<float> buffered_render_power = render_buffer_.Pop();
  if (!buffered_render_power) {
    // This can happen in a few cases: at the start of a call, due to a glitch
    // or due to clock drift. The excess capture value will be ignored.
    // TODO(ivoc): Include how often this happens in APM stats.
    return;
  }
  // Update the render statistics, and store the statistics in circular buffers.
  render_statistics_.Update(*buffered_render_power);
  RTC_DCHECK_LT(next_insertion_index_, kLookbackFrames);
  render_power_[next_insertion_index_] = *buffered_render_power;
  render_power_mean_[next_insertion_index_] = render_statistics_.mean();
  render_power_std_dev_[next_insertion_index_] =
      render_statistics_.std_deviation();

  // Get the next capture value, update capture statistics and add the relevant
  // values to the buffers.
  const float capture_power = Power(capture_audio);
  capture_statistics_.Update(capture_power);
  const float capture_mean = capture_statistics_.mean();
  const float capture_std_deviation = capture_statistics_.std_deviation();

  // Update the covariance values and determine the new echo likelihood.
  echo_likelihood_ = 0.f;
  for (size_t delay = 0; delay < covariances_.size(); ++delay) {
    const size_t read_index =
        (kLookbackFrames + next_insertion_index_ - delay) % kLookbackFrames;
    RTC_DCHECK_LT(read_index, render_power_.size());
    covariances_[delay].Update(capture_power, capture_mean,
                               capture_std_deviation, render_power_[read_index],
                               render_power_mean_[read_index],
                               render_power_std_dev_[read_index]);
    echo_likelihood_ = std::max(
        echo_likelihood_, covariances_[delay].normalized_cross_correlation());
  }
  reliability_ = (1.0f - kAlpha) * reliability_ + kAlpha * 1.0f;
  echo_likelihood_ *= reliability_;
  int echo_percentage = static_cast<int>(echo_likelihood_ * 100);
  RTC_HISTOGRAM_COUNTS("WebRTC.Audio.ResidualEchoDetector.EchoLikelihood",
                       echo_percentage, 0, 100, 100 /* number of bins */);

  // Update the buffer of recent likelihood values.
  recent_likelihood_max_.Update(echo_likelihood_);

  // Update the next insertion index.
  ++next_insertion_index_;
  next_insertion_index_ %= kLookbackFrames;
}

void ResidualEchoDetector::Initialize() {
  render_buffer_.Clear();
  std::fill(render_power_.begin(), render_power_.end(), 0.f);
  std::fill(render_power_mean_.begin(), render_power_mean_.end(), 0.f);
  std::fill(render_power_std_dev_.begin(), render_power_std_dev_.end(), 0.f);
  render_statistics_.Clear();
  capture_statistics_.Clear();
  recent_likelihood_max_.Clear();
  for (auto& cov : covariances_) {
    cov.Clear();
  }
  echo_likelihood_ = 0.f;
  next_insertion_index_ = 0;
  reliability_ = 0.f;
}

void ResidualEchoDetector::PackRenderAudioBuffer(
    AudioBuffer* audio,
    std::vector<float>* packed_buffer) {
  RTC_DCHECK_GE(160, audio->num_frames_per_band());

  packed_buffer->clear();
  packed_buffer->insert(packed_buffer->end(),
                        audio->split_bands_const_f(0)[kBand0To8kHz],
                        (audio->split_bands_const_f(0)[kBand0To8kHz] +
                         audio->num_frames_per_band()));
}

}  // namespace webrtc
