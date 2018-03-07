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
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

class ApiCallJitterBuffer {
 public:
  explicit ApiCallJitterBuffer(size_t num_bands) {
    buffer_.fill(std::vector<std::vector<float>>(
        num_bands, std::vector<float>(kBlockSize, 0.f)));
  }

  ~ApiCallJitterBuffer() = default;

  void Reset() {
    size_ = 0;
    last_insert_index_ = 0;
  }

  void Insert(const std::vector<std::vector<float>>& block) {
    RTC_DCHECK_LT(size_, buffer_.size());
    last_insert_index_ = (last_insert_index_ + 1) % buffer_.size();
    RTC_DCHECK_EQ(buffer_[last_insert_index_].size(), block.size());
    RTC_DCHECK_EQ(buffer_[last_insert_index_][0].size(), block[0].size());
    for (size_t k = 0; k < block.size(); ++k) {
      std::copy(block[k].begin(), block[k].end(),
                buffer_[last_insert_index_][k].begin());
    }
    ++size_;
  }

  void Remove(std::vector<std::vector<float>>* block) {
    RTC_DCHECK_LT(0, size_);
    --size_;
    const size_t extract_index =
        (last_insert_index_ - size_ + buffer_.size()) % buffer_.size();
    for (size_t k = 0; k < block->size(); ++k) {
      std::copy(buffer_[extract_index][k].begin(),
                buffer_[extract_index][k].end(), (*block)[k].begin());
    }
  }

  size_t Size() const { return size_; }
  bool Full() const { return size_ >= (buffer_.size()); }
  bool Empty() const { return size_ == 0; }

 private:
  std::array<std::vector<std::vector<float>>, kMaxApiCallsJitterBlocks> buffer_;
  size_t size_ = 0;
  int last_insert_index_ = 0;
};

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(size_t num_bands,
                        size_t down_sampling_factor,
                        size_t downsampled_render_buffer_size,
                        size_t render_delay_buffer_size);
  ~RenderDelayBufferImpl() override;

  void Reset() override;
  bool Insert(const std::vector<std::vector<float>>& block) override;
  bool UpdateBuffers() override;
  void SetDelay(size_t delay) override;
  size_t Delay() const override { return delay_; }

  const RenderBuffer& GetRenderBuffer() const override { return fft_buffer_; }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return downsampled_render_buffer_;
  }

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const size_t down_sampling_factor_;
  const size_t sub_block_size_;
  std::vector<std::vector<std::vector<float>>> buffer_;
  size_t delay_ = 0;
  size_t last_insert_index_ = 0;
  RenderBuffer fft_buffer_;
  DownsampledRenderBuffer downsampled_render_buffer_;
  Decimator render_decimator_;
  ApiCallJitterBuffer api_call_jitter_buffer_;
  const std::vector<std::vector<float>> zero_block_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

int RenderDelayBufferImpl::instance_count_ = 0;

RenderDelayBufferImpl::RenderDelayBufferImpl(
    size_t num_bands,
    size_t down_sampling_factor,
    size_t downsampled_render_buffer_size,
    size_t render_delay_buffer_size)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      down_sampling_factor_(down_sampling_factor),
      sub_block_size_(down_sampling_factor_ > 0
                          ? kBlockSize / down_sampling_factor
                          : kBlockSize),
      buffer_(
          render_delay_buffer_size,
          std::vector<std::vector<float>>(num_bands,
                                          std::vector<float>(kBlockSize, 0.f))),
      fft_buffer_(
          optimization_,
          num_bands,
          std::max(kUnknownDelayRenderWindowSize, kAdaptiveFilterLength),
          std::vector<size_t>(1, kAdaptiveFilterLength)),
      downsampled_render_buffer_(downsampled_render_buffer_size),
      render_decimator_(down_sampling_factor_),
      api_call_jitter_buffer_(num_bands),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)) {
  RTC_DCHECK_LT(buffer_.size(), downsampled_render_buffer_.buffer.size());
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

void RenderDelayBufferImpl::Reset() {
  // Empty all data in the buffers.
  delay_ = 0;
  last_insert_index_ = 0;
  downsampled_render_buffer_.position = 0;
  std::fill(downsampled_render_buffer_.buffer.begin(),
            downsampled_render_buffer_.buffer.end(), 0.f);
  fft_buffer_.Clear();
  api_call_jitter_buffer_.Reset();
  for (auto& c : buffer_) {
    for (auto& b : c) {
      std::fill(b.begin(), b.end(), 0.f);
    }
  }
}

bool RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  RTC_DCHECK_EQ(block.size(), buffer_[0].size());
  RTC_DCHECK_EQ(block[0].size(), buffer_[0][0].size());

  if (api_call_jitter_buffer_.Full()) {
    // Report buffer overrun and let the caller handle the overrun.
    return false;
  }
  api_call_jitter_buffer_.Insert(block);

  return true;
}

bool RenderDelayBufferImpl::UpdateBuffers() {
  bool underrun = true;
  // Update the buffers with a new block if such is available, otherwise insert
  // a block of silence.
  if (api_call_jitter_buffer_.Size() > 0) {
    last_insert_index_ = (last_insert_index_ + 1) % buffer_.size();
    api_call_jitter_buffer_.Remove(&buffer_[last_insert_index_]);
    underrun = false;
  }

  downsampled_render_buffer_.position =
      (downsampled_render_buffer_.position - sub_block_size_ +
       downsampled_render_buffer_.buffer.size()) %
      downsampled_render_buffer_.buffer.size();

  rtc::ArrayView<const float> input(
      underrun ? zero_block_[0].data() : buffer_[last_insert_index_][0].data(),
      kBlockSize);
  rtc::ArrayView<float> output(downsampled_render_buffer_.buffer.data() +
                                   downsampled_render_buffer_.position,
                               sub_block_size_);
  data_dumper_->DumpWav("aec3_render_decimator_input", input.size(),
                        input.data(), 16000, 1);
  render_decimator_.Decimate(input, output);
  data_dumper_->DumpWav("aec3_render_decimator_output", output.size(),
                        output.data(), 16000 / down_sampling_factor_, 1);
  for (size_t k = 0; k < output.size() / 2; ++k) {
    float tmp = output[k];
    output[k] = output[output.size() - 1 - k];
    output[output.size() - 1 - k] = tmp;
  }

  if (underrun) {
    fft_buffer_.Insert(zero_block_);
  } else {
    fft_buffer_.Insert(buffer_[(last_insert_index_ - delay_ + buffer_.size()) %
                               buffer_.size()]);
  }
  return !underrun;
}

void RenderDelayBufferImpl::SetDelay(size_t delay) {
  if (delay_ == delay) {
    return;
  }

  // If there is a new delay set, clear the fft buffer.
  fft_buffer_.Clear();

  if ((buffer_.size() - 1) < delay) {
    // If the desired delay is larger than the delay buffer, shorten the delay
    // buffer size to achieve the desired alignment with the available buffer
    // size.
    downsampled_render_buffer_.position =
        (downsampled_render_buffer_.position +
         sub_block_size_ * (delay - (buffer_.size() - 1))) %
        downsampled_render_buffer_.buffer.size();

    last_insert_index_ =
        (last_insert_index_ - (delay - (buffer_.size() - 1)) + buffer_.size()) %
        buffer_.size();
    delay_ = buffer_.size() - 1;
  } else {
    delay_ = delay;
  }
}

}  // namespace

RenderDelayBuffer* RenderDelayBuffer::Create(
    size_t num_bands,
    size_t down_sampling_factor,
    size_t downsampled_render_buffer_size,
    size_t render_delay_buffer_size) {
  return new RenderDelayBufferImpl(num_bands, down_sampling_factor,
                                   downsampled_render_buffer_size,
                                   render_delay_buffer_size);
}

}  // namespace webrtc
