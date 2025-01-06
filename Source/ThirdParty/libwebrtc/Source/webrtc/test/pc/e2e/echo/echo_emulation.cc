/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/echo/echo_emulation.h"

#include <limits>
#include <utility>

#include "api/test/pclf/media_configuration.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kSingleBufferDurationMs = 10;

}  // namespace

EchoEmulatingCapturer::EchoEmulatingCapturer(
    std::unique_ptr<TestAudioDeviceModule::Capturer> capturer,
    EchoEmulationConfig config)
    : delegate_(std::move(capturer)),
      config_(config),
      renderer_queue_(2 * config_.echo_delay.ms() / kSingleBufferDurationMs),
      queue_input_(TestAudioDeviceModule::SamplesPerFrame(
                       delegate_->SamplingFrequency()) *
                   delegate_->NumChannels()),
      queue_output_(TestAudioDeviceModule::SamplesPerFrame(
                        delegate_->SamplingFrequency()) *
                    delegate_->NumChannels()) {
  renderer_thread_.Detach();
  capturer_thread_.Detach();
}

void EchoEmulatingCapturer::OnAudioRendered(
    rtc::ArrayView<const int16_t> data) {
  RTC_DCHECK_RUN_ON(&renderer_thread_);
  if (!recording_started_) {
    // Because rendering can start before capturing in the beginning we can have
    // a set of empty audio data frames. So we will skip them and will start
    // fill the queue only after 1st non-empty audio data frame will arrive.
    bool is_empty = true;
    for (auto d : data) {
      if (d != 0) {
        is_empty = false;
        break;
      }
    }
    if (is_empty) {
      return;
    }
    recording_started_ = true;
  }
  queue_input_.assign(data.begin(), data.end());
  if (!renderer_queue_.Insert(&queue_input_)) {
    RTC_LOG(LS_WARNING) << "Echo queue is full";
  }
}

bool EchoEmulatingCapturer::Capture(rtc::BufferT<int16_t>* buffer) {
  RTC_DCHECK_RUN_ON(&capturer_thread_);
  bool result = delegate_->Capture(buffer);
  // Now we have to reduce input signal to avoid saturation when mixing in the
  // fake echo.
  for (size_t i = 0; i < buffer->size(); ++i) {
    (*buffer)[i] /= 2;
  }

  // When we accumulated enough delay in the echo buffer we will pop from
  // that buffer on each ::Capture(...) call. If the buffer become empty it
  // will mean some bug, so we will crash during removing item from the queue.
  if (!delay_accumulated_) {
    delay_accumulated_ =
        renderer_queue_.SizeAtLeast() >=
        static_cast<size_t>(config_.echo_delay.ms() / kSingleBufferDurationMs);
  }

  if (delay_accumulated_) {
    RTC_CHECK(renderer_queue_.Remove(&queue_output_));
    for (size_t i = 0; i < buffer->size() && i < queue_output_.size(); ++i) {
      int32_t res = (*buffer)[i] + queue_output_[i];
      if (res < std::numeric_limits<int16_t>::min()) {
        res = std::numeric_limits<int16_t>::min();
      }
      if (res > std::numeric_limits<int16_t>::max()) {
        res = std::numeric_limits<int16_t>::max();
      }
      (*buffer)[i] = static_cast<int16_t>(res);
    }
  }

  return result;
}

EchoEmulatingRenderer::EchoEmulatingRenderer(
    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer,
    EchoEmulatingCapturer* echo_emulating_capturer)
    : delegate_(std::move(renderer)),
      echo_emulating_capturer_(echo_emulating_capturer) {
  RTC_DCHECK(echo_emulating_capturer_);
}

bool EchoEmulatingRenderer::Render(rtc::ArrayView<const int16_t> data) {
  if (data.size() > 0) {
    echo_emulating_capturer_->OnAudioRendered(data);
  }
  return delegate_->Render(data);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
