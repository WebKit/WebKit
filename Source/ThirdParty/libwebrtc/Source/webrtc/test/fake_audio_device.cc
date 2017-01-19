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

#include "webrtc/base/platform_thread.h"
#include "webrtc/modules/media_file/media_file_utility.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {

FakeAudioDevice::FakeAudioDevice(Clock* clock,
                                 const std::string& filename,
                                 float speed)
    : audio_callback_(NULL),
      capturing_(false),
      captured_audio_(),
      playout_buffer_(),
      speed_(speed),
      last_playout_ms_(-1),
      clock_(clock, speed),
      tick_(EventTimerWrapper::Create()),
      thread_(FakeAudioDevice::Run, this, "FakeAudioDevice"),
      file_utility_(new ModuleFileUtility(0)),
      input_stream_(FileWrapper::Create()) {
  memset(captured_audio_, 0, sizeof(captured_audio_));
  memset(playout_buffer_, 0, sizeof(playout_buffer_));
  // Open audio input file as read-only and looping.
  EXPECT_TRUE(input_stream_->OpenFile(filename.c_str(), true)) << filename;
}

FakeAudioDevice::~FakeAudioDevice() {
  Stop();

  thread_.Stop();
}

int32_t FakeAudioDevice::Init() {
  rtc::CritScope cs(&lock_);
  if (file_utility_->InitPCMReading(*input_stream_.get()) != 0)
    return -1;

  if (!tick_->StartTimer(true, 10 / speed_))
    return -1;
  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
  return 0;
}

int32_t FakeAudioDevice::RegisterAudioCallback(AudioTransport* callback) {
  rtc::CritScope cs(&lock_);
  audio_callback_ = callback;
  return 0;
}

bool FakeAudioDevice::Playing() const {
  rtc::CritScope cs(&lock_);
  return capturing_;
}

int32_t FakeAudioDevice::PlayoutDelay(uint16_t* delay_ms) const {
  *delay_ms = 0;
  return 0;
}

bool FakeAudioDevice::Recording() const {
  rtc::CritScope cs(&lock_);
  return capturing_;
}

bool FakeAudioDevice::Run(void* obj) {
  static_cast<FakeAudioDevice*>(obj)->CaptureAudio();
  return true;
}

void FakeAudioDevice::CaptureAudio() {
  {
    rtc::CritScope cs(&lock_);
    if (capturing_) {
      int bytes_read = file_utility_->ReadPCMData(
          *input_stream_.get(), captured_audio_, kBufferSizeBytes);
      if (bytes_read <= 0)
        return;
      // 2 bytes per sample.
      size_t num_samples = static_cast<size_t>(bytes_read / 2);
      uint32_t new_mic_level;
      EXPECT_EQ(0,
                audio_callback_->RecordedDataIsAvailable(captured_audio_,
                                                         num_samples,
                                                         2,
                                                         1,
                                                         kFrequencyHz,
                                                         0,
                                                         0,
                                                         0,
                                                         false,
                                                         new_mic_level));
      size_t samples_needed = kFrequencyHz / 100;
      int64_t now_ms = clock_.TimeInMilliseconds();
      uint32_t time_since_last_playout_ms = now_ms - last_playout_ms_;
      if (last_playout_ms_ > 0 && time_since_last_playout_ms > 0) {
        samples_needed = std::min(
            static_cast<size_t>(kFrequencyHz / time_since_last_playout_ms),
            kBufferSizeBytes / 2);
      }
      size_t samples_out = 0;
      int64_t elapsed_time_ms = -1;
      int64_t ntp_time_ms = -1;
      EXPECT_EQ(0,
                audio_callback_->NeedMorePlayData(samples_needed,
                                                  2,
                                                  1,
                                                  kFrequencyHz,
                                                  playout_buffer_,
                                                  samples_out,
                                                  &elapsed_time_ms,
                                                  &ntp_time_ms));
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}

void FakeAudioDevice::Start() {
  rtc::CritScope cs(&lock_);
  capturing_ = true;
}

void FakeAudioDevice::Stop() {
  rtc::CritScope cs(&lock_);
  capturing_ = false;
}
}  // namespace test
}  // namespace webrtc
