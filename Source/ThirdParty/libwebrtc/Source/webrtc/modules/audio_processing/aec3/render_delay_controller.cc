/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/render_delay_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/echo_path_delay_estimator.h"
#include "modules/audio_processing/aec3/render_delay_controller_metrics.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

namespace {

class RenderDelayControllerImpl final : public RenderDelayController {
 public:
  RenderDelayControllerImpl(const EchoCanceller3Config& config,
                            int sample_rate_hz);
  ~RenderDelayControllerImpl() override;
  void Reset() override;
  void SetDelay(size_t render_delay) override;
  size_t GetDelay(const DownsampledRenderBuffer& render_buffer,
                  rtc::ArrayView<const float> capture) override;
  rtc::Optional<size_t> AlignmentHeadroomSamples() const override {
    return headroom_samples_;
  }

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const size_t default_delay_;
  size_t delay_;
  size_t blocks_since_last_delay_estimate_ = 300000;
  int echo_path_delay_samples_;
  size_t align_call_counter_ = 0;
  rtc::Optional<size_t> headroom_samples_;
  std::vector<float> capture_delay_buffer_;
  int capture_delay_buffer_index_ = 0;
  RenderDelayControllerMetrics metrics_;
  EchoPathDelayEstimator delay_estimator_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayControllerImpl);
};

size_t ComputeNewBufferDelay(size_t current_delay,
                             size_t echo_path_delay_samples) {
  // The below division is not exact and the truncation is intended.
  const int echo_path_delay_blocks = echo_path_delay_samples / kBlockSize;
  constexpr int kDelayHeadroomBlocks = 1;

  // Compute the buffer delay increase required to achieve the desired latency.
  size_t new_delay = std::max(echo_path_delay_blocks - kDelayHeadroomBlocks, 0);

  // Add hysteresis.
  if (new_delay == current_delay + 1) {
    new_delay = current_delay;
  }

  return new_delay;
}

int RenderDelayControllerImpl::instance_count_ = 0;

RenderDelayControllerImpl::RenderDelayControllerImpl(
    const EchoCanceller3Config& config,
    int sample_rate_hz)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      default_delay_(
          std::max(config.delay.default_delay, kMinEchoPathDelayBlocks)),
      delay_(default_delay_),
      echo_path_delay_samples_(default_delay_ * kBlockSize),
      capture_delay_buffer_(kBlockSize * (kMaxApiCallsJitterBlocks + 2), 0.f),
      delay_estimator_(data_dumper_.get(), config) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz));
  delay_estimator_.LogDelayEstimationProperties(sample_rate_hz,
                                                capture_delay_buffer_.size());
}

RenderDelayControllerImpl::~RenderDelayControllerImpl() = default;

void RenderDelayControllerImpl::Reset() {
  delay_ = default_delay_;
  blocks_since_last_delay_estimate_ = 300000;
  echo_path_delay_samples_ = delay_ * kBlockSize;
  align_call_counter_ = 0;
  headroom_samples_ = rtc::nullopt;
  std::fill(capture_delay_buffer_.begin(), capture_delay_buffer_.end(), 0.f);
  delay_estimator_.Reset();
}

void RenderDelayControllerImpl::SetDelay(size_t render_delay) {
  if (delay_ != render_delay) {
    // If a the delay set does not match the actual delay, reset the delay
    // controller.
    Reset();
    delay_ = render_delay;
  }
}

size_t RenderDelayControllerImpl::GetDelay(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());

  ++align_call_counter_;

  // Estimate the delay with a delayed capture signal in order to catch
  // noncausal delays.
  RTC_DCHECK_LT(capture_delay_buffer_index_ + kBlockSize - 1,
                capture_delay_buffer_.size());
  const rtc::Optional<size_t> echo_path_delay_samples_shifted =
      delay_estimator_.EstimateDelay(
          render_buffer,
          rtc::ArrayView<const float>(
              &capture_delay_buffer_[capture_delay_buffer_index_], kBlockSize));
  std::copy(capture.begin(), capture.end(),
            capture_delay_buffer_.begin() + capture_delay_buffer_index_);
  capture_delay_buffer_index_ =
      (capture_delay_buffer_index_ + kBlockSize) % capture_delay_buffer_.size();

  if (echo_path_delay_samples_shifted) {
    blocks_since_last_delay_estimate_ = 0;

    // Correct for the capture signal delay.
    const int echo_path_delay_samples_corrected =
        static_cast<int>(*echo_path_delay_samples_shifted) -
        static_cast<int>(capture_delay_buffer_.size());
    echo_path_delay_samples_ = std::max(0, echo_path_delay_samples_corrected);

    // Compute and set new render delay buffer delay.
    const size_t new_delay =
        ComputeNewBufferDelay(delay_, echo_path_delay_samples_);
    if (align_call_counter_ > kNumBlocksPerSecond) {
      delay_ = new_delay;

      // Update render delay buffer headroom.
      if (echo_path_delay_samples_corrected >= 0) {
        const int headroom = echo_path_delay_samples_ - delay_ * kBlockSize;
        RTC_DCHECK_LE(0, headroom);
        headroom_samples_ = headroom;
      } else {
        headroom_samples_ = rtc::nullopt;
      }
    }

    metrics_.Update(echo_path_delay_samples_, delay_);
  } else {
    metrics_.Update(rtc::nullopt, delay_);
  }

  data_dumper_->DumpRaw("aec3_render_delay_controller_delay", 1,
                        &echo_path_delay_samples_);
  data_dumper_->DumpRaw("aec3_render_delay_controller_buffer_delay", delay_);

  return delay_;
}

}  // namespace

RenderDelayController* RenderDelayController::Create(
    const EchoCanceller3Config& config,
    int sample_rate_hz) {
  return new RenderDelayControllerImpl(config, sample_rate_hz);
}

}  // namespace webrtc
