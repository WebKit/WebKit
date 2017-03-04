/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_audio_device.h"

#include <algorithm>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/random.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"

namespace webrtc {

namespace {

constexpr int kFrameLengthMs = 10;
constexpr int kFramesPerSecond = 1000 / kFrameLengthMs;

}  // namespace
namespace test {

// Assuming 10ms audio packets..
class FakeAudioDevice::PulsedNoiseCapturer {
 public:
  PulsedNoiseCapturer(size_t num_samples_per_frame, int16_t max_amplitude)
      : fill_with_zero_(false),
        random_generator_(1),
        max_amplitude_(max_amplitude),
        random_audio_(num_samples_per_frame),
        silent_audio_(num_samples_per_frame, 0) {
    RTC_DCHECK_GT(max_amplitude, 0);
  }

  rtc::ArrayView<const int16_t> Capture() {
    fill_with_zero_ = !fill_with_zero_;
    if (!fill_with_zero_) {
      std::generate(random_audio_.begin(), random_audio_.end(), [&]() {
        return random_generator_.Rand(-max_amplitude_, max_amplitude_);
      });
    }
    return fill_with_zero_ ? silent_audio_ : random_audio_;
  }

 private:
  bool fill_with_zero_;
  Random random_generator_;
  const int16_t max_amplitude_;
  std::vector<int16_t> random_audio_;
  std::vector<int16_t> silent_audio_;
};

FakeAudioDevice::FakeAudioDevice(float speed,
                                 int sampling_frequency_in_hz,
                                 int16_t max_amplitude)
    : sampling_frequency_in_hz_(sampling_frequency_in_hz),
      num_samples_per_frame_(
          rtc::CheckedDivExact(sampling_frequency_in_hz_, kFramesPerSecond)),
      speed_(speed),
      audio_callback_(nullptr),
      rendering_(false),
      capturing_(false),
      capturer_(new FakeAudioDevice::PulsedNoiseCapturer(num_samples_per_frame_,
                                                         max_amplitude)),
      playout_buffer_(num_samples_per_frame_, 0),
      tick_(EventTimerWrapper::Create()),
      thread_(FakeAudioDevice::Run, this, "FakeAudioDevice") {
  RTC_DCHECK(
      sampling_frequency_in_hz == 8000 || sampling_frequency_in_hz == 16000 ||
      sampling_frequency_in_hz == 32000 || sampling_frequency_in_hz == 44100 ||
      sampling_frequency_in_hz == 48000);
}

FakeAudioDevice::~FakeAudioDevice() {
  StopPlayout();
  StopRecording();
  thread_.Stop();
}

int32_t FakeAudioDevice::StartPlayout() {
  rtc::CritScope cs(&lock_);
  rendering_ = true;
  return 0;
}

int32_t FakeAudioDevice::StopPlayout() {
  rtc::CritScope cs(&lock_);
  rendering_ = false;
  return 0;
}

int32_t FakeAudioDevice::StartRecording() {
  rtc::CritScope cs(&lock_);
  capturing_ = true;
  return 0;
}

int32_t FakeAudioDevice::StopRecording() {
  rtc::CritScope cs(&lock_);
  capturing_ = false;
  return 0;
}

int32_t FakeAudioDevice::Init() {
  RTC_CHECK(tick_->StartTimer(true, kFrameLengthMs / speed_));
  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
  return 0;
}

int32_t FakeAudioDevice::RegisterAudioCallback(AudioTransport* callback) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(callback || audio_callback_ != nullptr);
  audio_callback_ = callback;
  return 0;
}

bool FakeAudioDevice::Playing() const {
  rtc::CritScope cs(&lock_);
  return rendering_;
}

bool FakeAudioDevice::Recording() const {
  rtc::CritScope cs(&lock_);
  return capturing_;
}

bool FakeAudioDevice::Run(void* obj) {
  static_cast<FakeAudioDevice*>(obj)->ProcessAudio();
  return true;
}

void FakeAudioDevice::ProcessAudio() {
  {
    rtc::CritScope cs(&lock_);
    if (capturing_) {
      // Capture 10ms of audio. 2 bytes per sample.
      rtc::ArrayView<const int16_t> audio_data = capturer_->Capture();
      uint32_t new_mic_level = 0;
      audio_callback_->RecordedDataIsAvailable(
          audio_data.data(), audio_data.size(), 2, 1, sampling_frequency_in_hz_,
          0, 0, 0, false, new_mic_level);
    }
    if (rendering_) {
      size_t samples_out = 0;
      int64_t elapsed_time_ms = -1;
      int64_t ntp_time_ms = -1;
      audio_callback_->NeedMorePlayData(
          num_samples_per_frame_, 2, 1, sampling_frequency_in_hz_,
          playout_buffer_.data(), samples_out, &elapsed_time_ms, &ntp_time_ms);
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}


}  // namespace test
}  // namespace webrtc
