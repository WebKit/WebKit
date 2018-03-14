/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_buffer.h"

#include <string.h>
#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(const EchoCanceller3Config& config, size_t num_bands);
  ~RenderDelayBufferImpl() override;

  void Reset() override;
  BufferingEvent Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent PrepareCaptureProcessing() override;
  bool SetDelay(size_t delay) override;
  rtc::Optional<size_t> Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return blocks_.buffer.size() - 1 - buffer_headroom_;
  }
  RenderBuffer* GetRenderBuffer() override { return &echo_remover_buffer_; }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return low_rate_;
  }

  bool CausalDelay(size_t delay) const override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  const int sub_block_size_;
  MatrixBuffer blocks_;
  VectorBuffer spectra_;
  FftBuffer ffts_;
  rtc::Optional<size_t> delay_;
  rtc::Optional<int> internal_delay_;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer low_rate_;
  Decimator render_decimator_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  std::vector<float> render_ds_;
  const int buffer_headroom_;
  bool last_call_was_render_ = false;
  int num_api_calls_in_a_row_ = 0;
  int max_observed_jitter_ = 1;
  size_t capture_call_counter_ = 0;
  size_t render_call_counter_ = 0;

  int LowRateBufferOffset() const { return DelayEstimatorOffset(config_) >> 1; }
  int MaxExternalDelayToInternalDelay(size_t delay) const;
  void ApplyDelay(int delay);
  void InsertBlock(const std::vector<std::vector<float>>& block,
                   int previous_write);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

// Increases the write indices for the render buffers.
void IncreaseWriteIndices(int sub_block_size,
                          MatrixBuffer* blocks,
                          VectorBuffer* spectra,
                          FftBuffer* ffts,
                          DownsampledRenderBuffer* low_rate) {
  low_rate->UpdateWriteIndex(-sub_block_size);
  blocks->IncWriteIndex();
  spectra->DecWriteIndex();
  ffts->DecWriteIndex();
}

// Increases the read indices for the render buffers.
void IncreaseReadIndices(const rtc::Optional<int>& delay,
                         int sub_block_size,
                         MatrixBuffer* blocks,
                         VectorBuffer* spectra,
                         FftBuffer* ffts,
                         DownsampledRenderBuffer* low_rate) {
  RTC_DCHECK_NE(low_rate->read, low_rate->write);
  low_rate->UpdateReadIndex(-sub_block_size);

  if (blocks->read != blocks->write) {
    blocks->IncReadIndex();
    spectra->DecReadIndex();
    ffts->DecReadIndex();
  } else {
    // Only allow underrun for blocks_ when the delay is not set.
    RTC_DCHECK(!delay);
  }
}

// Checks for a render buffer overrun.
bool RenderOverrun(const MatrixBuffer& b, const DownsampledRenderBuffer& l) {
  return l.read == l.write || b.read == b.write;
}

// Checks for a render buffer underrun. If the delay is not specified, only the
// low rate buffer underrun is counted as the delay offset for the other buffers
// is unknown.
bool RenderUnderrun(const rtc::Optional<int>& delay,
                    const MatrixBuffer& b,
                    const DownsampledRenderBuffer& l) {
  return l.read == l.write || (delay && b.read == b.write);
}

// Computes the latency in the buffer (the number of unread elements).
int BufferLatency(const DownsampledRenderBuffer& l) {
  return (l.buffer.size() + l.read - l.write) % l.buffer.size();
}

// Computes the mismatch between the number of render and capture calls based on
// the known offset (achieved during reset) of the low rate buffer.
bool ApiCallSkew(const DownsampledRenderBuffer& low_rate_buffer,
                 int sub_block_size,
                 int low_rate_buffer_offset_sub_blocks) {
  int latency = BufferLatency(low_rate_buffer);
  int skew = abs(low_rate_buffer_offset_sub_blocks * sub_block_size - latency);
  int skew_limit = low_rate_buffer_offset_sub_blocks * sub_block_size;
  return skew >= skew_limit;
}

int RenderDelayBufferImpl::instance_count_ = 0;

RenderDelayBufferImpl::RenderDelayBufferImpl(const EchoCanceller3Config& config,
                                             size_t num_bands)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      config_(config),
      sub_block_size_(
          static_cast<int>(config.delay.down_sampling_factor > 0
                               ? kBlockSize / config.delay.down_sampling_factor
                               : kBlockSize)),
      blocks_(GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                       config.delay.num_filters,
                                       config.filter.main.length_blocks),
              num_bands,
              kBlockSize),
      spectra_(blocks_.buffer.size(), kFftLengthBy2Plus1),
      ffts_(blocks_.buffer.size()),
      delay_(config_.delay.min_echo_path_delay_blocks),
      echo_remover_buffer_(&blocks_, &spectra_, &ffts_),
      low_rate_(GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                         config.delay.num_filters)),
      render_decimator_(config.delay.down_sampling_factor),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_(),
      render_ds_(sub_block_size_, 0.f),
      buffer_headroom_(config.filter.main.length_blocks) {
  RTC_DCHECK_EQ(blocks_.buffer.size(), ffts_.buffer.size());
  RTC_DCHECK_EQ(spectra_.buffer.size(), ffts_.buffer.size());

  // Necessary condition to avoid unrecoverable echp due to noncausal alignment.
  RTC_DCHECK_EQ(DelayEstimatorOffset(config_), LowRateBufferOffset() * 2);
  Reset();
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

// Resets the buffer delays and clears the reported delays.
void RenderDelayBufferImpl::Reset() {
  last_call_was_render_ = false;
  num_api_calls_in_a_row_ = 1;

  // Pre-fill the low rate buffer (which is used for delay estimation) to add
  // headroom for the allowed api call jitter.
  low_rate_.read = low_rate_.OffsetIndex(
      low_rate_.write, LowRateBufferOffset() * sub_block_size_);

  // Set the render buffer delays to the default delay.
  ApplyDelay(config_.delay.default_delay);

  // Unset the delays which are set by ApplyConfig.
  delay_ = rtc::nullopt;
  internal_delay_ = rtc::nullopt;
}

// Inserts a new block into the render buffers.
RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  ++render_call_counter_;
  if (delay_) {
    if (!last_call_was_render_) {
      last_call_was_render_ = true;
      num_api_calls_in_a_row_ = 1;
    } else {
      if (++num_api_calls_in_a_row_ > max_observed_jitter_) {
        max_observed_jitter_ = num_api_calls_in_a_row_;
        RTC_LOG(LS_INFO)
            << "New max number api jitter observed at render block "
            << render_call_counter_ << ":  " << num_api_calls_in_a_row_
            << " blocks";
      }
    }
  }

  // Increase the write indices to where the new blocks should be written.
  const int previous_write = blocks_.write;
  IncreaseWriteIndices(sub_block_size_, &blocks_, &spectra_, &ffts_,
                       &low_rate_);

  // Allow overrun and do a reset when render overrun occurrs due to more render
  // data being inserted than capture data is received.
  BufferingEvent event = RenderOverrun(blocks_, low_rate_)
                             ? BufferingEvent::kRenderOverrun
                             : BufferingEvent::kNone;

  // Insert the new render block into the specified position.
  InsertBlock(block, previous_write);

  if (event != BufferingEvent::kNone) {
    Reset();
  }

  return event;
}

// Prepares the render buffers for processing another capture block.
RenderDelayBuffer::BufferingEvent
RenderDelayBufferImpl::PrepareCaptureProcessing() {
  BufferingEvent event = BufferingEvent::kNone;
  ++capture_call_counter_;

  if (delay_) {
    if (last_call_was_render_) {
      last_call_was_render_ = false;
      num_api_calls_in_a_row_ = 1;
    } else {
      if (++num_api_calls_in_a_row_ > max_observed_jitter_) {
        max_observed_jitter_ = num_api_calls_in_a_row_;
        RTC_LOG(LS_INFO)
            << "New max number api jitter observed at capture block "
            << capture_call_counter_ << ":  " << num_api_calls_in_a_row_
            << " blocks";
      }
    }
  }

  if (RenderUnderrun(internal_delay_, blocks_, low_rate_)) {
    // Don't increase the read indices if there is a render underrun.
    event = BufferingEvent::kRenderUnderrun;
  } else {
    // Increase the read indices in the render buffers to point to the most
    // recent block to use in the capture processing.
    IncreaseReadIndices(internal_delay_, sub_block_size_, &blocks_, &spectra_,
                        &ffts_, &low_rate_);

    // Check for skew in the API calls which, if too large, causes the delay
    // estimation to be noncausal. Doing this check after the render indice
    // increase saves one unit of allowed skew. Note that the skew check only
    // should need to be one-sided as one of the skew directions results in an
    // underrun.
    bool skew = ApiCallSkew(low_rate_, sub_block_size_, LowRateBufferOffset());
    event = skew ? BufferingEvent::kApiCallSkew : BufferingEvent::kNone;
  }

  if (event != BufferingEvent::kNone) {
    Reset();
  }

  return event;
}

// Sets the delay and returns a bool indicating whether the delay was changed.
bool RenderDelayBufferImpl::SetDelay(size_t delay) {
  if (delay_ && *delay_ == delay) {
    return false;
  }
  delay_ = delay;

  // Compute the internal delay and limit the delay to the allowed range.
  int internal_delay = MaxExternalDelayToInternalDelay(*delay_);
  internal_delay_ =
      std::min(MaxDelay(), static_cast<size_t>(std::max(internal_delay, 0)));

  // Apply the delay to the buffers.
  ApplyDelay(*internal_delay_);
  return true;
}

// Returns whether the specified delay is causal.
bool RenderDelayBufferImpl::CausalDelay(size_t delay) const {
  // Compute the internal delay and limit the delay to the allowed range.
  int internal_delay = MaxExternalDelayToInternalDelay(delay);
  internal_delay =
      std::min(MaxDelay(), static_cast<size_t>(std::max(internal_delay, 0)));

  return internal_delay >=
         static_cast<int>(config_.delay.min_echo_path_delay_blocks);
}

// Maps the externally computed delay to the delay used internally.
int RenderDelayBufferImpl::MaxExternalDelayToInternalDelay(
    size_t external_delay_blocks) const {
  const int latency = BufferLatency(low_rate_);
  RTC_DCHECK_LT(0, sub_block_size_);
  RTC_DCHECK_EQ(0, latency % sub_block_size_);
  int latency_blocks = latency / sub_block_size_;
  return latency_blocks + static_cast<int>(external_delay_blocks) -
         DelayEstimatorOffset(config_);
}

// Set the read indices according to the delay.
void RenderDelayBufferImpl::ApplyDelay(int delay) {
  blocks_.read = blocks_.OffsetIndex(blocks_.write, -delay);
  spectra_.read = spectra_.OffsetIndex(spectra_.write, delay);
  ffts_.read = ffts_.OffsetIndex(ffts_.write, delay);
}

// Inserts a block into the render buffers.
void RenderDelayBufferImpl::InsertBlock(
    const std::vector<std::vector<float>>& block,
    int previous_write) {
  auto& b = blocks_;
  auto& lr = low_rate_;
  auto& ds = render_ds_;
  auto& f = ffts_;
  auto& s = spectra_;
  RTC_DCHECK_EQ(block.size(), b.buffer[b.write].size());
  for (size_t k = 0; k < block.size(); ++k) {
    RTC_DCHECK_EQ(block[k].size(), b.buffer[b.write][k].size());
    std::copy(block[k].begin(), block[k].end(), b.buffer[b.write][k].begin());
  }

  render_decimator_.Decimate(block[0], ds);
  std::copy(ds.rbegin(), ds.rend(), lr.buffer.begin() + lr.write);
  fft_.PaddedFft(block[0], b.buffer[previous_write][0], &f.buffer[f.write]);
  f.buffer[f.write].Spectrum(optimization_, s.buffer[s.write]);
}

}  // namespace

int RenderDelayBuffer::RenderDelayBuffer::DelayEstimatorOffset(
    const EchoCanceller3Config& config) {
  return config.delay.api_call_jitter_blocks * 2;
}

RenderDelayBuffer* RenderDelayBuffer::Create(const EchoCanceller3Config& config,
                                             size_t num_bands) {
  return new RenderDelayBufferImpl(config, num_bands);
}

}  // namespace webrtc
