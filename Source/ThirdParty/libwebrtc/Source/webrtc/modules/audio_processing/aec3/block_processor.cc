/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/block_processor.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/block_processor_metrics.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

class BlockProcessorImpl final : public BlockProcessor {
 public:
  BlockProcessorImpl(int sample_rate_hz,
                     std::unique_ptr<RenderDelayBuffer> render_buffer,
                     std::unique_ptr<RenderDelayController> delay_controller,
                     std::unique_ptr<EchoRemover> echo_remover);

  ~BlockProcessorImpl() override;

  void ProcessCapture(bool echo_path_gain_change,
                      bool capture_signal_saturation,
                      std::vector<std::vector<float>>* capture_block) override;

  bool BufferRender(std::vector<std::vector<float>>* block) override;

  void UpdateEchoLeakageStatus(bool leakage_detected) override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const size_t sample_rate_hz_;
  std::unique_ptr<RenderDelayBuffer> render_buffer_;
  std::unique_ptr<RenderDelayController> delay_controller_;
  std::unique_ptr<EchoRemover> echo_remover_;
  BlockProcessorMetrics metrics_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BlockProcessorImpl);
};

constexpr size_t kRenderBufferSize = 250;
int BlockProcessorImpl::instance_count_ = 0;
constexpr size_t kMaxApiJitter = 30;

BlockProcessorImpl::BlockProcessorImpl(
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer,
    std::unique_ptr<RenderDelayController> delay_controller,
    std::unique_ptr<EchoRemover> echo_remover)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      sample_rate_hz_(sample_rate_hz),
      render_buffer_(std::move(render_buffer)),
      delay_controller_(std::move(delay_controller)),
      echo_remover_(std::move(echo_remover)) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz_));
}

BlockProcessorImpl::~BlockProcessorImpl() = default;

void BlockProcessorImpl::ProcessCapture(
    bool echo_path_gain_change,
    bool capture_signal_saturation,
    std::vector<std::vector<float>>* capture_block) {
  RTC_DCHECK(capture_block);
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), capture_block->size());
  RTC_DCHECK_EQ(kBlockSize, (*capture_block)[0].size());

  const size_t delay = delay_controller_->GetDelay((*capture_block)[0]);
  const bool render_delay_change = delay != render_buffer_->Delay();

  if (render_delay_change) {
    render_buffer_->SetDelay(delay);
  }

  if (render_buffer_->IsBlockAvailable()) {
    auto& render_block = render_buffer_->GetNext();
    echo_remover_->ProcessBlock(
        delay_controller_->AlignmentHeadroomSamples(),
        EchoPathVariability(echo_path_gain_change, render_delay_change),
        capture_signal_saturation, render_block, capture_block);
    metrics_.UpdateCapture(false);
  } else {
    metrics_.UpdateCapture(true);
  }
}

bool BlockProcessorImpl::BufferRender(std::vector<std::vector<float>>* block) {
  RTC_DCHECK(block);
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), block->size());
  RTC_DCHECK_EQ(kBlockSize, (*block)[0].size());

  const bool delay_controller_overrun =
      !delay_controller_->AnalyzeRender((*block)[0]);
  const bool render_buffer_overrun = !render_buffer_->Insert(block);
  if (delay_controller_overrun || render_buffer_overrun) {
    metrics_.UpdateRender(true);
    return false;
  }
  metrics_.UpdateRender(false);
  return true;
}

void BlockProcessorImpl::UpdateEchoLeakageStatus(bool leakage_detected) {
  echo_remover_->UpdateEchoLeakageStatus(leakage_detected);
}

}  // namespace

BlockProcessor* BlockProcessor::Create(int sample_rate_hz) {
  std::unique_ptr<RenderDelayBuffer> render_buffer(RenderDelayBuffer::Create(
      kRenderBufferSize, NumBandsForRate(sample_rate_hz), kMaxApiJitter));
  std::unique_ptr<RenderDelayController> delay_controller(
      RenderDelayController::Create(sample_rate_hz, *render_buffer));
  std::unique_ptr<EchoRemover> echo_remover(
      EchoRemover::Create(sample_rate_hz));
  return Create(sample_rate_hz, std::move(render_buffer),
                std::move(delay_controller), std::move(echo_remover));
}

BlockProcessor* BlockProcessor::Create(
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer) {
  std::unique_ptr<RenderDelayController> delay_controller(
      RenderDelayController::Create(sample_rate_hz, *render_buffer));
  std::unique_ptr<EchoRemover> echo_remover(
      EchoRemover::Create(sample_rate_hz));
  return Create(sample_rate_hz, std::move(render_buffer),
                std::move(delay_controller), std::move(echo_remover));
}

BlockProcessor* BlockProcessor::Create(
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer,
    std::unique_ptr<RenderDelayController> delay_controller,
    std::unique_ptr<EchoRemover> echo_remover) {
  return new BlockProcessorImpl(sample_rate_hz, std::move(render_buffer),
                                std::move(delay_controller),
                                std::move(echo_remover));
}

}  // namespace webrtc
