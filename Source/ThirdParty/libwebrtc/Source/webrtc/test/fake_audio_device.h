/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_
#define WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class EventTimerWrapper;

namespace test {

// FakeAudioDevice implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
class FakeAudioDevice : public FakeAudioDeviceModule {
 public:
  // Creates a new FakeAudioDevice. When capturing or playing, 10 ms audio
  // frames will be processed every 100ms / |speed|.
  // |sampling_frequency_in_hz| can be 8, 16, 32, 44.1 or 48kHz.
  // When recording is started, it will generates a signal where every second
  // frame is zero and every second frame is evenly distributed random noise
  // with max amplitude |max_amplitude|.
  FakeAudioDevice(float speed,
                  int sampling_frequency_in_hz,
                  int16_t max_amplitude);
  ~FakeAudioDevice() override;

 private:
  int32_t Init() override;
  int32_t RegisterAudioCallback(AudioTransport* callback) override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;

  bool Playing() const override;
  bool Recording() const override;

  static bool Run(void* obj);
  void ProcessAudio();

  const int sampling_frequency_in_hz_;
  const size_t num_samples_per_frame_;
  const float speed_;

  rtc::CriticalSection lock_;
  AudioTransport* audio_callback_ GUARDED_BY(lock_);
  bool rendering_ GUARDED_BY(lock_);
  bool capturing_ GUARDED_BY(lock_);

  class PulsedNoiseCapturer;
  const std::unique_ptr<PulsedNoiseCapturer> capturer_ GUARDED_BY(lock_);

  std::vector<int16_t> playout_buffer_ GUARDED_BY(lock_);

  std::unique_ptr<EventTimerWrapper> tick_;
  rtc::PlatformThread thread_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FAKE_AUDIO_DEVICE_H_
