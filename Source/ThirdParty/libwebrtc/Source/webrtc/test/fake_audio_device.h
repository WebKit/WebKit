/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_FAKE_AUDIO_DEVICE_H_
#define TEST_FAKE_AUDIO_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class EventTimerWrapper;

namespace test {

// FakeAudioDevice implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
class FakeAudioDevice : public FakeAudioDeviceModule {
 public:
  // Returns the number of samples that Capturers and Renderers with this
  // sampling frequency will work with every time Capture or Render is called.
  static size_t SamplesPerFrame(int sampling_frequency_in_hz);

  class Capturer {
   public:
    virtual ~Capturer() {}
    // Returns the sampling frequency in Hz of the audio data that this
    // capturer produces.
    virtual int SamplingFrequency() const = 0;
    // Replaces the contents of |buffer| with 10ms of captured audio data
    // (see FakeAudioDevice::SamplesPerFrame). Returns true if the capturer can
    // keep producing data, or false when the capture finishes.
    virtual bool Capture(rtc::BufferT<int16_t>* buffer) = 0;
  };

  class Renderer {
   public:
    virtual ~Renderer() {}
    // Returns the sampling frequency in Hz of the audio data that this
    // renderer receives.
    virtual int SamplingFrequency() const = 0;
    // Renders the passed audio data and returns true if the renderer wants
    // to keep receiving data, or false otherwise.
    virtual bool Render(rtc::ArrayView<const int16_t> data) = 0;
  };

  // Creates a new FakeAudioDevice. When capturing or playing, 10 ms audio
  // frames will be processed every 10ms / |speed|.
  // |capturer| is an object that produces audio data. Can be nullptr if this
  // device is never used for recording.
  // |renderer| is an object that receives audio data that would have been
  // played out. Can be nullptr if this device is never used for playing.
  // Use one of the Create... functions to get these instances.
  FakeAudioDevice(std::unique_ptr<Capturer> capturer,
                  std::unique_ptr<Renderer> renderer,
                  float speed = 1);
  ~FakeAudioDevice() override;

  // Returns a Capturer instance that generates a signal where every second
  // frame is zero and every second frame is evenly distributed random noise
  // with max amplitude |max_amplitude|.
  static std::unique_ptr<Capturer> CreatePulsedNoiseCapturer(
      int16_t max_amplitude, int sampling_frequency_in_hz);

  // Returns a Capturer instance that gets its data from a file.
  static std::unique_ptr<Capturer> CreateWavFileReader(
      std::string filename, int sampling_frequency_in_hz);

  // Returns a Capturer instance that gets its data from a file.
  // Automatically detects sample rate.
  static std::unique_ptr<Capturer> CreateWavFileReader(std::string filename);

  // Returns a Renderer instance that writes its data to a file.
  static std::unique_ptr<Renderer> CreateWavFileWriter(
      std::string filename, int sampling_frequency_in_hz);

  // Returns a Renderer instance that writes its data to a WAV file, cutting
  // off silence at the beginning (not necessarily perfect silence, see
  // kAmplitudeThreshold) and at the end (only actual 0 samples in this case).
  static std::unique_ptr<Renderer> CreateBoundedWavFileWriter(
      std::string filename, int sampling_frequency_in_hz);

  // Returns a Renderer instance that does nothing with the audio data.
  static std::unique_ptr<Renderer> CreateDiscardRenderer(
      int sampling_frequency_in_hz);

  int32_t Init() override;
  int32_t RegisterAudioCallback(AudioTransport* callback) override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;

  bool Playing() const override;
  bool Recording() const override;

  // Blocks until the Renderer refuses to receive data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForPlayoutEnd(int timeout_ms = rtc::Event::kForever);
  // Blocks until the Recorder stops producing data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForRecordingEnd(int timeout_ms = rtc::Event::kForever);

 private:
  static bool Run(void* obj);
  void ProcessAudio();

  const std::unique_ptr<Capturer> capturer_ RTC_GUARDED_BY(lock_);
  const std::unique_ptr<Renderer> renderer_ RTC_GUARDED_BY(lock_);
  const float speed_;

  rtc::CriticalSection lock_;
  AudioTransport* audio_callback_ RTC_GUARDED_BY(lock_);
  bool rendering_ RTC_GUARDED_BY(lock_);
  bool capturing_ RTC_GUARDED_BY(lock_);
  rtc::Event done_rendering_;
  rtc::Event done_capturing_;

  std::vector<int16_t> playout_buffer_ RTC_GUARDED_BY(lock_);
  rtc::BufferT<int16_t> recording_buffer_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<EventTimerWrapper> tick_;
  rtc::PlatformThread thread_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_AUDIO_DEVICE_H_
