/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/echo_canceller3.h"

#include <algorithm>
#include <utility>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomic_ops.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

enum class EchoCanceller3ApiCall { kCapture, kRender };

bool DetectSaturation(rtc::ArrayView<const float> y) {
  for (auto y_k : y) {
    if (y_k >= 32700.0f || y_k <= -32700.0f) {
      return true;
    }
  }
  return false;
}

// Method for adjusting config parameter dependencies..
EchoCanceller3Config AdjustConfig(const EchoCanceller3Config& config) {
  EchoCanceller3Config adjusted_cfg = config;

  if (field_trial::IsEnabled("WebRTC-Aec3ShortHeadroomKillSwitch")) {
    // Two blocks headroom.
    adjusted_cfg.delay.delay_headroom_samples = kBlockSize * 2;
  }

  return adjusted_cfg;
}

void FillSubFrameView(
    AudioBuffer* frame,
    size_t sub_frame_index,
    std::vector<std::vector<rtc::ArrayView<float>>>* sub_frame_view) {
  RTC_DCHECK_GE(1, sub_frame_index);
  RTC_DCHECK_LE(0, sub_frame_index);
  RTC_DCHECK_EQ(frame->num_bands(), sub_frame_view->size());
  RTC_DCHECK_EQ(frame->num_channels(), (*sub_frame_view)[0].size());
  for (size_t band = 0; band < sub_frame_view->size(); ++band) {
    for (size_t channel = 0; channel < (*sub_frame_view)[0].size(); ++channel) {
      (*sub_frame_view)[band][channel] = rtc::ArrayView<float>(
          &frame->split_bands(channel)[band][sub_frame_index * kSubFrameLength],
          kSubFrameLength);
    }
  }
}

void FillSubFrameView(
    std::vector<std::vector<std::vector<float>>>* frame,
    size_t sub_frame_index,
    std::vector<std::vector<rtc::ArrayView<float>>>* sub_frame_view) {
  RTC_DCHECK_GE(1, sub_frame_index);
  RTC_DCHECK_EQ(frame->size(), sub_frame_view->size());
  RTC_DCHECK_EQ((*frame)[0].size(), (*sub_frame_view)[0].size());
  for (size_t band = 0; band < frame->size(); ++band) {
    for (size_t channel = 0; channel < (*frame)[band].size(); ++channel) {
      (*sub_frame_view)[band][channel] = rtc::ArrayView<float>(
          &(*frame)[band][channel][sub_frame_index * kSubFrameLength],
          kSubFrameLength);
    }
  }
}

void ProcessCaptureFrameContent(
    AudioBuffer* capture,
    bool level_change,
    bool saturated_microphone_signal,
    size_t sub_frame_index,
    FrameBlocker* capture_blocker,
    BlockFramer* output_framer,
    BlockProcessor* block_processor,
    std::vector<std::vector<std::vector<float>>>* block,
    std::vector<std::vector<rtc::ArrayView<float>>>* sub_frame_view) {
  FillSubFrameView(capture, sub_frame_index, sub_frame_view);
  capture_blocker->InsertSubFrameAndExtractBlock(*sub_frame_view, block);
  block_processor->ProcessCapture(level_change, saturated_microphone_signal,
                                  block);
  output_framer->InsertBlockAndExtractSubFrame(*block, sub_frame_view);
}

void ProcessRemainingCaptureFrameContent(
    bool level_change,
    bool saturated_microphone_signal,
    FrameBlocker* capture_blocker,
    BlockFramer* output_framer,
    BlockProcessor* block_processor,
    std::vector<std::vector<std::vector<float>>>* block) {
  if (!capture_blocker->IsBlockAvailable()) {
    return;
  }

  capture_blocker->ExtractBlock(block);
  block_processor->ProcessCapture(level_change, saturated_microphone_signal,
                                  block);
  output_framer->InsertBlock(*block);
}

void BufferRenderFrameContent(
    std::vector<std::vector<std::vector<float>>>* render_frame,
    size_t sub_frame_index,
    FrameBlocker* render_blocker,
    BlockProcessor* block_processor,
    std::vector<std::vector<std::vector<float>>>* block,
    std::vector<std::vector<rtc::ArrayView<float>>>* sub_frame_view) {
  FillSubFrameView(render_frame, sub_frame_index, sub_frame_view);
  render_blocker->InsertSubFrameAndExtractBlock(*sub_frame_view, block);
  block_processor->BufferRender(*block);
}

void BufferRemainingRenderFrameContent(
    FrameBlocker* render_blocker,
    BlockProcessor* block_processor,
    std::vector<std::vector<std::vector<float>>>* block) {
  if (!render_blocker->IsBlockAvailable()) {
    return;
  }
  render_blocker->ExtractBlock(block);
  block_processor->BufferRender(*block);
}

void CopyBufferIntoFrame(const AudioBuffer& buffer,
                         size_t num_bands,
                         size_t num_channels,
                         std::vector<std::vector<std::vector<float>>>* frame) {
  RTC_DCHECK_EQ(num_bands, frame->size());
  RTC_DCHECK_EQ(num_channels, (*frame)[0].size());
  RTC_DCHECK_EQ(AudioBuffer::kSplitBandSize, (*frame)[0][0].size());
  for (size_t band = 0; band < num_bands; ++band) {
    for (size_t channel = 0; channel < num_channels; ++channel) {
      rtc::ArrayView<const float> buffer_view(
          &buffer.split_bands_const(channel)[band][0],
          AudioBuffer::kSplitBandSize);
      std::copy(buffer_view.begin(), buffer_view.end(),
                (*frame)[band][channel].begin());
    }
  }
}

}  // namespace

class EchoCanceller3::RenderWriter {
 public:
  RenderWriter(ApmDataDumper* data_dumper,
               SwapQueue<std::vector<std::vector<std::vector<float>>>,
                         Aec3RenderQueueItemVerifier>* render_transfer_queue,
               size_t num_bands,
               size_t num_channels);
  ~RenderWriter();
  void Insert(const AudioBuffer& input);

 private:
  ApmDataDumper* data_dumper_;
  const size_t num_bands_;
  const size_t num_channels_;
  HighPassFilter high_pass_filter_;
  std::vector<std::vector<std::vector<float>>> render_queue_input_frame_;
  SwapQueue<std::vector<std::vector<std::vector<float>>>,
            Aec3RenderQueueItemVerifier>* render_transfer_queue_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWriter);
};

EchoCanceller3::RenderWriter::RenderWriter(
    ApmDataDumper* data_dumper,
    SwapQueue<std::vector<std::vector<std::vector<float>>>,
              Aec3RenderQueueItemVerifier>* render_transfer_queue,
    size_t num_bands,
    size_t num_channels)
    : data_dumper_(data_dumper),
      num_bands_(num_bands),
      num_channels_(num_channels),
      high_pass_filter_(num_channels),
      render_queue_input_frame_(
          num_bands_,
          std::vector<std::vector<float>>(
              num_channels_,
              std::vector<float>(AudioBuffer::kSplitBandSize, 0.f))),
      render_transfer_queue_(render_transfer_queue) {
  RTC_DCHECK(data_dumper);
}

EchoCanceller3::RenderWriter::~RenderWriter() = default;

void EchoCanceller3::RenderWriter::Insert(const AudioBuffer& input) {
  RTC_DCHECK_EQ(1, input.num_channels());
  RTC_DCHECK_EQ(AudioBuffer::kSplitBandSize, input.num_frames_per_band());
  RTC_DCHECK_EQ(num_bands_, input.num_bands());

  // TODO(bugs.webrtc.org/8759) Temporary work-around.
  if (num_bands_ != input.num_bands())
    return;

  data_dumper_->DumpWav("aec3_render_input", AudioBuffer::kSplitBandSize,
                        &input.split_bands_const(0)[0][0], 16000, 1);

  CopyBufferIntoFrame(input, num_bands_, num_channels_,
                      &render_queue_input_frame_);
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    high_pass_filter_.Process(render_queue_input_frame_[0][channel]);
  }

  static_cast<void>(render_transfer_queue_->Insert(&render_queue_input_frame_));
}

int EchoCanceller3::instance_count_ = 0;

EchoCanceller3::EchoCanceller3(const EchoCanceller3Config& config,
                               int sample_rate_hz,
                               size_t num_render_channels,
                               size_t num_capture_channels)
    : EchoCanceller3(AdjustConfig(config),
                     sample_rate_hz,
                     num_render_channels,
                     num_capture_channels,
                     std::unique_ptr<BlockProcessor>(
                         BlockProcessor::Create(AdjustConfig(config),
                                                sample_rate_hz,
                                                num_render_channels,
                                                num_capture_channels))) {}
EchoCanceller3::EchoCanceller3(const EchoCanceller3Config& config,
                               int sample_rate_hz,
                               size_t num_render_channels,
                               size_t num_capture_channels,
                               std::unique_ptr<BlockProcessor> block_processor)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      sample_rate_hz_(sample_rate_hz),
      num_bands_(NumBandsForRate(sample_rate_hz_)),
      num_render_channels_(num_render_channels),
      num_capture_channels_(num_capture_channels),
      output_framer_(num_bands_, num_capture_channels_),
      capture_blocker_(num_bands_, num_capture_channels_),
      render_blocker_(num_bands_, num_render_channels_),
      render_transfer_queue_(
          kRenderTransferQueueSizeFrames,
          std::vector<std::vector<std::vector<float>>>(
              num_bands_,
              std::vector<std::vector<float>>(
                  num_render_channels_,
                  std::vector<float>(AudioBuffer::kSplitBandSize, 0.f))),
          Aec3RenderQueueItemVerifier(num_bands_,
                                      num_render_channels_,
                                      AudioBuffer::kSplitBandSize)),
      block_processor_(std::move(block_processor)),
      render_queue_output_frame_(
          num_bands_,
          std::vector<std::vector<float>>(
              num_render_channels_,
              std::vector<float>(AudioBuffer::kSplitBandSize, 0.f))),
      render_block_(
          num_bands_,
          std::vector<std::vector<float>>(num_render_channels_,
                                          std::vector<float>(kBlockSize, 0.f))),
      capture_block_(
          num_bands_,
          std::vector<std::vector<float>>(num_capture_channels_,
                                          std::vector<float>(kBlockSize, 0.f))),
      render_sub_frame_view_(
          num_bands_,
          std::vector<rtc::ArrayView<float>>(num_render_channels_)),
      capture_sub_frame_view_(
          num_bands_,
          std::vector<rtc::ArrayView<float>>(num_capture_channels_)),
      block_delay_buffer_(num_bands_,
                          AudioBuffer::kSplitBandSize,
                          config_.delay.fixed_capture_delay_samples) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz_));

  render_writer_.reset(new RenderWriter(data_dumper_.get(),
                                        &render_transfer_queue_, num_bands_,
                                        num_render_channels_));

  RTC_DCHECK_EQ(num_bands_, std::max(sample_rate_hz_, 16000) / 16000);
  RTC_DCHECK_GE(kMaxNumBands, num_bands_);
}

EchoCanceller3::~EchoCanceller3() = default;

void EchoCanceller3::AnalyzeRender(const AudioBuffer& render) {
  RTC_DCHECK_RUNS_SERIALIZED(&render_race_checker_);
  RTC_DCHECK_EQ(render.num_channels(), num_render_channels_);
  data_dumper_->DumpRaw("aec3_call_order",
                        static_cast<int>(EchoCanceller3ApiCall::kRender));

  return render_writer_->Insert(render);
}

void EchoCanceller3::AnalyzeCapture(const AudioBuffer& capture) {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  data_dumper_->DumpWav("aec3_capture_analyze_input", capture.num_frames(),
                        capture.channels_const()[0], sample_rate_hz_, 1);

  saturated_microphone_signal_ = false;
  for (size_t channel = 0; channel < capture.num_channels(); ++channel) {
    saturated_microphone_signal_ |=
        DetectSaturation(rtc::ArrayView<const float>(
            capture.channels_const()[channel], capture.num_frames()));
    if (saturated_microphone_signal_) {
      break;
    }
  }
}

void EchoCanceller3::ProcessCapture(AudioBuffer* capture, bool level_change) {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  RTC_DCHECK(capture);
  RTC_DCHECK_EQ(1u, capture->num_channels());
  RTC_DCHECK_EQ(num_bands_, capture->num_bands());
  RTC_DCHECK_EQ(AudioBuffer::kSplitBandSize, capture->num_frames_per_band());
  RTC_DCHECK_EQ(capture->num_channels(), num_capture_channels_);
  data_dumper_->DumpRaw("aec3_call_order",
                        static_cast<int>(EchoCanceller3ApiCall::kCapture));

  // Report capture call in the metrics and periodically update API call
  // metrics.
  api_call_metrics_.ReportCaptureCall();

  // Optionally delay the capture signal.
  if (config_.delay.fixed_capture_delay_samples > 0) {
    block_delay_buffer_.DelaySignal(capture);
  }

  rtc::ArrayView<float> capture_lower_band = rtc::ArrayView<float>(
      &capture->split_bands(0)[0][0], AudioBuffer::kSplitBandSize);

  data_dumper_->DumpWav("aec3_capture_input", capture_lower_band, 16000, 1);

  EmptyRenderQueue();

  ProcessCaptureFrameContent(capture, level_change,
                             saturated_microphone_signal_, 0, &capture_blocker_,
                             &output_framer_, block_processor_.get(),
                             &capture_block_, &capture_sub_frame_view_);

  ProcessCaptureFrameContent(capture, level_change,
                             saturated_microphone_signal_, 1, &capture_blocker_,
                             &output_framer_, block_processor_.get(),
                             &capture_block_, &capture_sub_frame_view_);

  ProcessRemainingCaptureFrameContent(
      level_change, saturated_microphone_signal_, &capture_blocker_,
      &output_framer_, block_processor_.get(), &capture_block_);

  data_dumper_->DumpWav("aec3_capture_output", AudioBuffer::kSplitBandSize,
                        &capture->split_bands(0)[0][0], 16000, 1);
}

EchoControl::Metrics EchoCanceller3::GetMetrics() const {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  Metrics metrics;
  block_processor_->GetMetrics(&metrics);
  return metrics;
}

void EchoCanceller3::SetAudioBufferDelay(size_t delay_ms) {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  block_processor_->SetAudioBufferDelay(delay_ms);
}

void EchoCanceller3::EmptyRenderQueue() {
  RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
  bool frame_to_buffer =
      render_transfer_queue_.Remove(&render_queue_output_frame_);
  while (frame_to_buffer) {
    // Report render call in the metrics.
    api_call_metrics_.ReportRenderCall();

    BufferRenderFrameContent(&render_queue_output_frame_, 0, &render_blocker_,
                             block_processor_.get(), &render_block_,
                             &render_sub_frame_view_);

    BufferRenderFrameContent(&render_queue_output_frame_, 1, &render_blocker_,
                             block_processor_.get(), &render_block_,
                             &render_sub_frame_view_);

    BufferRemainingRenderFrameContent(&render_blocker_, block_processor_.get(),
                                      &render_block_);

    frame_to_buffer =
        render_transfer_queue_.Remove(&render_queue_output_frame_);
  }
}
}  // namespace webrtc
