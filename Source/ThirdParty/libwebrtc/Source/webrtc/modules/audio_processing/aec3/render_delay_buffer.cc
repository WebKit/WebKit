/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"

#include <string.h>
#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {
namespace {

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(size_t size_blocks,
                        size_t num_bands,
                        size_t max_api_jitter_blocks);
  ~RenderDelayBufferImpl() override;

  bool Insert(std::vector<std::vector<float>>* block) override;
  const std::vector<std::vector<float>>& GetNext() override;
  void SetDelay(size_t delay) override;
  size_t Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return buffer_.size() - max_api_jitter_blocks_;
  }
  bool IsBlockAvailable() const override { return insert_surplus_ > 0; }
  size_t MaxApiJitter() const override { return max_api_jitter_blocks_; }

 private:
  const size_t max_api_jitter_blocks_;
  std::vector<std::vector<std::vector<float>>> buffer_;
  size_t last_insert_index_ = 0;
  size_t delay_ = 0;
  size_t insert_surplus_ = 0;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

RenderDelayBufferImpl::RenderDelayBufferImpl(size_t size_blocks,
                                             size_t num_bands,
                                             size_t max_api_jitter_blocks)
    : max_api_jitter_blocks_(max_api_jitter_blocks),
      buffer_(size_blocks + max_api_jitter_blocks_,
              std::vector<std::vector<float>>(
                  num_bands,
                  std::vector<float>(kBlockSize, 0.f))) {}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

bool RenderDelayBufferImpl::Insert(std::vector<std::vector<float>>* block) {
  RTC_DCHECK_EQ(block->size(), buffer_[0].size());
  RTC_DCHECK_EQ((*block)[0].size(), buffer_[0][0].size());

  if (insert_surplus_ == max_api_jitter_blocks_) {
    return false;
  }
  last_insert_index_ = (last_insert_index_ + 1) % buffer_.size();
  block->swap(buffer_[last_insert_index_]);

  ++insert_surplus_;

  return true;
}

const std::vector<std::vector<float>>& RenderDelayBufferImpl::GetNext() {
  RTC_DCHECK(IsBlockAvailable());
  const size_t extract_index_ =
      (last_insert_index_ - delay_ - insert_surplus_ + 1 + buffer_.size()) %
      buffer_.size();
  RTC_DCHECK_LE(0, extract_index_);
  RTC_DCHECK_GT(buffer_.size(), extract_index_);

  RTC_DCHECK_LT(0, insert_surplus_);
  --insert_surplus_;

  return buffer_[extract_index_];
}

void RenderDelayBufferImpl::SetDelay(size_t delay) {
  RTC_DCHECK_GE(MaxDelay(), delay);
  delay_ = delay;
}

}  // namespace

RenderDelayBuffer* RenderDelayBuffer::Create(size_t size_blocks,
                                             size_t num_bands,
                                             size_t max_api_jitter_blocks) {
  return new RenderDelayBufferImpl(size_blocks, num_bands,
                                   max_api_jitter_blocks);
}

}  // namespace webrtc
