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
#include <numeric>
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

constexpr int kSkewHistorySizeLog2 = 8;

// Estimator of API call skew between render and capture.
class SkewEstimator {
 public:
  // Resets the estimation.
  void Reset() {
    skew_ = 0;
    next_index_ = 0;
    sufficient_skew_stored_ = false;
  }

  // Updates the skew data for a render call.
  void LogRenderCall() { ++skew_; }

  // Updates and computes the skew at a capture call. Returns an optional which
  // is non-null if a reliable skew has been found.
  rtc::Optional<int> GetSkewFromCapture() {
    --skew_;

    skew_history_[next_index_] = skew_;
    if (++next_index_ == skew_history_.size()) {
      next_index_ = 0;
      sufficient_skew_stored_ = true;
    }

    if (!sufficient_skew_stored_) {
      return rtc::nullopt;
    }

    return std::accumulate(skew_history_.begin(), skew_history_.end(), 0) >>
           kSkewHistorySizeLog2;
  }

 private:
  int skew_ = 0;
  std::array<int, 1 << kSkewHistorySizeLog2> skew_history_;
  size_t next_index_ = 0;
  bool sufficient_skew_stored_ = false;
};

class RenderDelayControllerImpl final : public RenderDelayController {
 public:
  RenderDelayControllerImpl(const EchoCanceller3Config& config,
                            int non_causal_offset,
                            int sample_rate_hz);
  ~RenderDelayControllerImpl() override;
  void Reset() override;
  void LogRenderCall() override;
  rtc::Optional<DelayEstimate> GetDelay(
      const DownsampledRenderBuffer& render_buffer,
      rtc::ArrayView<const float> capture) override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const int delay_headroom_blocks_;
  const int hysteresis_limit_1_blocks_;
  const int hysteresis_limit_2_blocks_;
  rtc::Optional<DelayEstimate> delay_;
  EchoPathDelayEstimator delay_estimator_;
  std::vector<float> delay_buf_;
  int delay_buf_index_ = 0;
  RenderDelayControllerMetrics metrics_;
  SkewEstimator skew_estimator_;
  rtc::Optional<DelayEstimate> delay_samples_;
  rtc::Optional<int> skew_;
  int delay_change_counter_ = 0;
  size_t soft_reset_counter_ = 0;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayControllerImpl);
};

DelayEstimate ComputeBufferDelay(
    const rtc::Optional<DelayEstimate>& current_delay,
    int delay_headroom_blocks,
    int hysteresis_limit_1_blocks,
    int hysteresis_limit_2_blocks,
    int offset_blocks,
    DelayEstimate estimated_delay) {
  // The below division is not exact and the truncation is intended.
  const int echo_path_delay_blocks = estimated_delay.delay >> kBlockSizeLog2;

  // Compute the buffer delay increase required to achieve the desired latency.
  size_t new_delay_blocks = std::max(
      echo_path_delay_blocks + offset_blocks - delay_headroom_blocks, 0);

  // Add hysteresis.
  if (current_delay) {
    size_t current_delay_blocks = current_delay->delay;
    if (new_delay_blocks > current_delay_blocks) {
      if (new_delay_blocks <=
          current_delay_blocks + hysteresis_limit_1_blocks) {
        new_delay_blocks = current_delay_blocks;
      }
    } else if (new_delay_blocks < current_delay_blocks) {
      size_t hysteresis_limit = std::max(
          static_cast<int>(current_delay_blocks) - hysteresis_limit_2_blocks,
          0);
      if (new_delay_blocks >= hysteresis_limit) {
        new_delay_blocks = current_delay_blocks;
      }
    }
  }

  return DelayEstimate(estimated_delay.quality, new_delay_blocks);
}

int RenderDelayControllerImpl::instance_count_ = 0;

RenderDelayControllerImpl::RenderDelayControllerImpl(
    const EchoCanceller3Config& config,
    int non_causal_offset,
    int sample_rate_hz)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      delay_headroom_blocks_(
          static_cast<int>(config.delay.delay_headroom_blocks)),
      hysteresis_limit_1_blocks_(
          static_cast<int>(config.delay.hysteresis_limit_1_blocks)),
      hysteresis_limit_2_blocks_(
          static_cast<int>(config.delay.hysteresis_limit_2_blocks)),
      delay_estimator_(data_dumper_.get(), config),
      delay_buf_(kBlockSize * non_causal_offset, 0.f) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz));
  delay_estimator_.LogDelayEstimationProperties(sample_rate_hz,
                                                delay_buf_.size());
}

RenderDelayControllerImpl::~RenderDelayControllerImpl() = default;

void RenderDelayControllerImpl::Reset() {
  delay_ = rtc::nullopt;
  delay_samples_ = rtc::nullopt;
  skew_ = rtc::nullopt;
  std::fill(delay_buf_.begin(), delay_buf_.end(), 0.f);
  delay_estimator_.Reset(false);
  skew_estimator_.Reset();
  delay_change_counter_ = 0;
  soft_reset_counter_ = 0;
}

void RenderDelayControllerImpl::LogRenderCall() {
  skew_estimator_.LogRenderCall();
}

rtc::Optional<DelayEstimate> RenderDelayControllerImpl::GetDelay(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());

  // Estimate the delay with a delayed capture.
  RTC_DCHECK_LT(delay_buf_index_ + kBlockSize - 1, delay_buf_.size());
  rtc::ArrayView<const float> capture_delayed(&delay_buf_[delay_buf_index_],
                                              kBlockSize);
  auto delay_samples =
      delay_estimator_.EstimateDelay(render_buffer, capture_delayed);

  std::copy(capture.begin(), capture.end(),
            delay_buf_.begin() + delay_buf_index_);
  delay_buf_index_ = (delay_buf_index_ + kBlockSize) % delay_buf_.size();

  // Compute the latest skew update.
  rtc::Optional<int> skew = skew_estimator_.GetSkewFromCapture();

  if (delay_samples) {
    if (!delay_samples_ || delay_samples->delay != delay_samples_->delay) {
      delay_change_counter_ = 0;
    }
    delay_samples_ = delay_samples;
  }

  if (delay_change_counter_ < 2 * kNumBlocksPerSecond) {
    ++delay_change_counter_;
    // If a new delay estimate is recently obtained, store the skew for that.
    skew_ = skew;
  } else {
    // A reliable skew should have been obtained after 2 seconds.
    RTC_DCHECK(skew_);
    RTC_DCHECK(skew);
  }

  ++soft_reset_counter_;
  int offset_blocks = 0;
  if (skew_ && skew && delay_samples_ &&
      delay_samples_->quality == DelayEstimate::Quality::kRefined) {
    // Compute the skew offset and add a margin.
    offset_blocks = *skew_ - *skew;
    if (offset_blocks != 0 && soft_reset_counter_ > 10 * kNumBlocksPerSecond) {
      // Soft reset the delay estimator if there is a significant offset
      // detected.
      delay_estimator_.Reset(true);
      soft_reset_counter_ = 0;
    }
  }

  if (delay_samples_) {
    // Compute the render delay buffer delay.
    delay_ = ComputeBufferDelay(
        delay_, delay_headroom_blocks_, hysteresis_limit_1_blocks_,
        hysteresis_limit_2_blocks_, offset_blocks, *delay_samples_);
  }

  metrics_.Update(delay_samples_ ? rtc::Optional<size_t>(delay_samples_->delay)
                                 : rtc::nullopt,
                  delay_ ? delay_->delay : 0);

  data_dumper_->DumpRaw("aec3_render_delay_controller_delay",
                        delay_samples ? delay_samples->delay : 0);
  data_dumper_->DumpRaw("aec3_render_delay_controller_buffer_delay",
                        delay_ ? delay_->delay : 0);

  data_dumper_->DumpRaw("aec3_render_delay_controller_new_skew",
                        skew ? *skew : 0);
  data_dumper_->DumpRaw("aec3_render_delay_controller_old_skew",
                        skew_ ? *skew_ : 0);
  data_dumper_->DumpRaw("aec3_render_delay_controller_offset", offset_blocks);

  return delay_;
}

}  // namespace

RenderDelayController* RenderDelayController::Create(
    const EchoCanceller3Config& config,
    int non_causal_offset,
    int sample_rate_hz) {
  return new RenderDelayControllerImpl(config, non_causal_offset,
                                       sample_rate_hz);
}

}  // namespace webrtc
