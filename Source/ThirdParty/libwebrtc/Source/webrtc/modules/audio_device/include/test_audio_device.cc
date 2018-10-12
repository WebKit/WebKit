/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "common_audio/wav_file.h"
#include "modules/audio_device/include/audio_device_default.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

namespace {

constexpr int kFrameLengthUs = 10000;
constexpr int kFramesPerSecond = rtc::kNumMicrosecsPerSec / kFrameLengthUs;

// TestAudioDeviceModule implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
class TestAudioDeviceModuleImpl
    : public webrtc_impl::AudioDeviceModuleDefault<TestAudioDeviceModule> {
 public:
  // Creates a new TestAudioDeviceModule. When capturing or playing, 10 ms audio
  // frames will be processed every 10ms / |speed|.
  // |capturer| is an object that produces audio data. Can be nullptr if this
  // device is never used for recording.
  // |renderer| is an object that receives audio data that would have been
  // played out. Can be nullptr if this device is never used for playing.
  // Use one of the Create... functions to get these instances.
  TestAudioDeviceModuleImpl(std::unique_ptr<Capturer> capturer,
                            std::unique_ptr<Renderer> renderer,
                            float speed = 1)
      : capturer_(std::move(capturer)),
        renderer_(std::move(renderer)),
        process_interval_us_(kFrameLengthUs / speed),
        audio_callback_(nullptr),
        rendering_(false),
        capturing_(false),
        done_rendering_(true, true),
        done_capturing_(true, true),
        stop_thread_(false) {
    auto good_sample_rate = [](int sr) {
      return sr == 8000 || sr == 16000 || sr == 32000 || sr == 44100 ||
             sr == 48000;
    };

    if (renderer_) {
      const int sample_rate = renderer_->SamplingFrequency();
      playout_buffer_.resize(
          SamplesPerFrame(sample_rate) * renderer_->NumChannels(), 0);
      RTC_CHECK(good_sample_rate(sample_rate));
    }
    if (capturer_) {
      RTC_CHECK(good_sample_rate(capturer_->SamplingFrequency()));
    }
  }

  ~TestAudioDeviceModuleImpl() {
    StopPlayout();
    StopRecording();
    if (thread_) {
      {
        rtc::CritScope cs(&lock_);
        stop_thread_ = true;
      }
      thread_->Stop();
    }
  }

  int32_t Init() {
    thread_ = absl::make_unique<rtc::PlatformThread>(
        TestAudioDeviceModuleImpl::Run, this, "TestAudioDeviceModuleImpl",
        rtc::kHighPriority);
    thread_->Start();
    return 0;
  }

  int32_t RegisterAudioCallback(AudioTransport* callback) {
    rtc::CritScope cs(&lock_);
    RTC_DCHECK(callback || audio_callback_);
    audio_callback_ = callback;
    return 0;
  }

  int32_t StartPlayout() {
    rtc::CritScope cs(&lock_);
    RTC_CHECK(renderer_);
    rendering_ = true;
    done_rendering_.Reset();
    return 0;
  }

  int32_t StopPlayout() {
    rtc::CritScope cs(&lock_);
    rendering_ = false;
    done_rendering_.Set();
    return 0;
  }

  int32_t StartRecording() {
    rtc::CritScope cs(&lock_);
    RTC_CHECK(capturer_);
    capturing_ = true;
    done_capturing_.Reset();
    return 0;
  }

  int32_t StopRecording() {
    rtc::CritScope cs(&lock_);
    capturing_ = false;
    done_capturing_.Set();
    return 0;
  }

  bool Playing() const {
    rtc::CritScope cs(&lock_);
    return rendering_;
  }

  bool Recording() const {
    rtc::CritScope cs(&lock_);
    return capturing_;
  }

  // Blocks until the Renderer refuses to receive data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForPlayoutEnd(int timeout_ms = rtc::Event::kForever) {
    return done_rendering_.Wait(timeout_ms);
  }

  // Blocks until the Recorder stops producing data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForRecordingEnd(int timeout_ms = rtc::Event::kForever) {
    return done_capturing_.Wait(timeout_ms);
  }

 private:
  void ProcessAudio() {
    int64_t time_us = rtc::TimeMicros();
    bool logged_once = false;
    for (;;) {
      {
        rtc::CritScope cs(&lock_);
        if (stop_thread_) {
          return;
        }
        if (capturing_) {
          // Capture 10ms of audio. 2 bytes per sample.
          const bool keep_capturing = capturer_->Capture(&recording_buffer_);
          uint32_t new_mic_level = 0;
          if (recording_buffer_.size() > 0) {
            audio_callback_->RecordedDataIsAvailable(
                recording_buffer_.data(), recording_buffer_.size(), 2,
                capturer_->NumChannels(), capturer_->SamplingFrequency(), 0, 0,
                0, false, new_mic_level);
          }
          if (!keep_capturing) {
            capturing_ = false;
            done_capturing_.Set();
          }
        }
        if (rendering_) {
          size_t samples_out = 0;
          int64_t elapsed_time_ms = -1;
          int64_t ntp_time_ms = -1;
          const int sampling_frequency = renderer_->SamplingFrequency();
          audio_callback_->NeedMorePlayData(
              SamplesPerFrame(sampling_frequency), 2, renderer_->NumChannels(),
              sampling_frequency, playout_buffer_.data(), samples_out,
              &elapsed_time_ms, &ntp_time_ms);
          const bool keep_rendering =
              renderer_->Render(rtc::ArrayView<const int16_t>(
                  playout_buffer_.data(), samples_out));
          if (!keep_rendering) {
            rendering_ = false;
            done_rendering_.Set();
          }
        }
      }
      time_us += process_interval_us_;

      int64_t time_left_us = time_us - rtc::TimeMicros();
      if (time_left_us < 0) {
        if (!logged_once) {
          RTC_LOG(LS_ERROR) << "ProcessAudio is too slow";
          logged_once = true;
        }
      } else {
        while (time_left_us > 1000) {
          if (rtc::Thread::SleepMs(time_left_us / 1000))
            break;
          time_left_us = time_us - rtc::TimeMicros();
        }
      }
    }
  }

  static void Run(void* obj) {
    static_cast<TestAudioDeviceModuleImpl*>(obj)->ProcessAudio();
  }

  const std::unique_ptr<Capturer> capturer_ RTC_GUARDED_BY(lock_);
  const std::unique_ptr<Renderer> renderer_ RTC_GUARDED_BY(lock_);
  const int64_t process_interval_us_;

  rtc::CriticalSection lock_;
  AudioTransport* audio_callback_ RTC_GUARDED_BY(lock_);
  bool rendering_ RTC_GUARDED_BY(lock_);
  bool capturing_ RTC_GUARDED_BY(lock_);
  rtc::Event done_rendering_;
  rtc::Event done_capturing_;

  std::vector<int16_t> playout_buffer_ RTC_GUARDED_BY(lock_);
  rtc::BufferT<int16_t> recording_buffer_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<rtc::PlatformThread> thread_;
  bool stop_thread_ RTC_GUARDED_BY(lock_);
};

// A fake capturer that generates pulses with random samples between
// -max_amplitude and +max_amplitude.
class PulsedNoiseCapturerImpl final
    : public TestAudioDeviceModule::PulsedNoiseCapturer {
 public:
  // Assuming 10ms audio packets.
  PulsedNoiseCapturerImpl(int16_t max_amplitude,
                          int sampling_frequency_in_hz,
                          int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        fill_with_zero_(false),
        random_generator_(1),
        max_amplitude_(max_amplitude),
        num_channels_(num_channels) {
    RTC_DCHECK_GT(max_amplitude, 0);
  }

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  int NumChannels() const override { return num_channels_; }

  bool Capture(rtc::BufferT<int16_t>* buffer) override {
    fill_with_zero_ = !fill_with_zero_;
    int16_t max_amplitude;
    {
      rtc::CritScope cs(&lock_);
      max_amplitude = max_amplitude_;
    }
    buffer->SetData(
        TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz_) *
            num_channels_,
        [&](rtc::ArrayView<int16_t> data) {
          if (fill_with_zero_) {
            std::fill(data.begin(), data.end(), 0);
          } else {
            std::generate(data.begin(), data.end(), [&]() {
              return random_generator_.Rand(-max_amplitude, max_amplitude);
            });
          }
          return data.size();
        });
    return true;
  }

  void SetMaxAmplitude(int16_t amplitude) override {
    rtc::CritScope cs(&lock_);
    max_amplitude_ = amplitude;
  }

 private:
  int sampling_frequency_in_hz_;
  bool fill_with_zero_;
  Random random_generator_;
  rtc::CriticalSection lock_;
  int16_t max_amplitude_ RTC_GUARDED_BY(lock_);
  const int num_channels_;
};

class WavFileReader final : public TestAudioDeviceModule::Capturer {
 public:
  WavFileReader(std::string filename,
                int sampling_frequency_in_hz,
                int num_channels)
      : WavFileReader(absl::make_unique<WavReader>(filename),
                      sampling_frequency_in_hz,
                      num_channels) {}

  WavFileReader(rtc::PlatformFile file,
                int sampling_frequency_in_hz,
                int num_channels)
      : WavFileReader(absl::make_unique<WavReader>(file),
                      sampling_frequency_in_hz,
                      num_channels) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  int NumChannels() const override { return num_channels_; }

  bool Capture(rtc::BufferT<int16_t>* buffer) override {
    buffer->SetData(
        TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz_) *
            num_channels_,
        [&](rtc::ArrayView<int16_t> data) {
          return wav_reader_->ReadSamples(data.size(), data.data());
        });
    return buffer->size() > 0;
  }

 private:
  WavFileReader(std::unique_ptr<WavReader> wav_reader,
                int sampling_frequency_in_hz,
                int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        num_channels_(num_channels),
        wav_reader_(std::move(wav_reader)) {
    RTC_CHECK_EQ(wav_reader_->sample_rate(), sampling_frequency_in_hz);
    RTC_CHECK_EQ(wav_reader_->num_channels(), num_channels);
  }

  int sampling_frequency_in_hz_;
  const int num_channels_;
  std::unique_ptr<WavReader> wav_reader_;
};

class WavFileWriter final : public TestAudioDeviceModule::Renderer {
 public:
  WavFileWriter(std::string filename,
                int sampling_frequency_in_hz,
                int num_channels)
      : WavFileWriter(absl::make_unique<WavWriter>(filename,
                                                   sampling_frequency_in_hz,
                                                   num_channels),
                      sampling_frequency_in_hz,
                      num_channels) {}

  WavFileWriter(rtc::PlatformFile file,
                int sampling_frequency_in_hz,
                int num_channels)
      : WavFileWriter(absl::make_unique<WavWriter>(file,
                                                   sampling_frequency_in_hz,
                                                   num_channels),
                      sampling_frequency_in_hz,
                      num_channels) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  int NumChannels() const override { return num_channels_; }

  bool Render(rtc::ArrayView<const int16_t> data) override {
    wav_writer_->WriteSamples(data.data(), data.size());
    return true;
  }

 private:
  WavFileWriter(std::unique_ptr<WavWriter> wav_writer,
                int sampling_frequency_in_hz,
                int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_writer_(std::move(wav_writer)),
        num_channels_(num_channels) {}

  int sampling_frequency_in_hz_;
  std::unique_ptr<WavWriter> wav_writer_;
  const int num_channels_;
};

class BoundedWavFileWriter : public TestAudioDeviceModule::Renderer {
 public:
  BoundedWavFileWriter(std::string filename,
                       int sampling_frequency_in_hz,
                       int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_writer_(filename, sampling_frequency_in_hz, num_channels),
        num_channels_(num_channels),
        silent_audio_(
            TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz) *
                num_channels,
            0),
        started_writing_(false),
        trailing_zeros_(0) {}

  BoundedWavFileWriter(rtc::PlatformFile file,
                       int sampling_frequency_in_hz,
                       int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_writer_(file, sampling_frequency_in_hz, num_channels),
        num_channels_(num_channels),
        silent_audio_(
            TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz) *
                num_channels,
            0),
        started_writing_(false),
        trailing_zeros_(0) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  int NumChannels() const override { return num_channels_; }

  bool Render(rtc::ArrayView<const int16_t> data) override {
    const int16_t kAmplitudeThreshold = 5;

    const int16_t* begin = data.begin();
    const int16_t* end = data.end();
    if (!started_writing_) {
      // Cut off silence at the beginning.
      while (begin < end) {
        if (std::abs(*begin) > kAmplitudeThreshold) {
          started_writing_ = true;
          break;
        }
        ++begin;
      }
    }
    if (started_writing_) {
      // Cut off silence at the end.
      while (begin < end) {
        if (*(end - 1) != 0) {
          break;
        }
        --end;
      }
      if (begin < end) {
        // If it turns out that the silence was not final, need to write all the
        // skipped zeros and continue writing audio.
        while (trailing_zeros_ > 0) {
          const size_t zeros_to_write =
              std::min(trailing_zeros_, silent_audio_.size());
          wav_writer_.WriteSamples(silent_audio_.data(), zeros_to_write);
          trailing_zeros_ -= zeros_to_write;
        }
        wav_writer_.WriteSamples(begin, end - begin);
      }
      // Save the number of zeros we skipped in case this needs to be restored.
      trailing_zeros_ += data.end() - end;
    }
    return true;
  }

 private:
  int sampling_frequency_in_hz_;
  WavWriter wav_writer_;
  const int num_channels_;
  std::vector<int16_t> silent_audio_;
  bool started_writing_;
  size_t trailing_zeros_;
};

class DiscardRenderer final : public TestAudioDeviceModule::Renderer {
 public:
  explicit DiscardRenderer(int sampling_frequency_in_hz, int num_channels)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        num_channels_(num_channels) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  int NumChannels() const override { return num_channels_; }

  bool Render(rtc::ArrayView<const int16_t> data) override { return true; }

 private:
  int sampling_frequency_in_hz_;
  const int num_channels_;
};

}  // namespace

size_t TestAudioDeviceModule::SamplesPerFrame(int sampling_frequency_in_hz) {
  return rtc::CheckedDivExact(sampling_frequency_in_hz, kFramesPerSecond);
}

rtc::scoped_refptr<TestAudioDeviceModule>
TestAudioDeviceModule::CreateTestAudioDeviceModule(
    std::unique_ptr<Capturer> capturer,
    std::unique_ptr<Renderer> renderer,
    float speed) {
  return new rtc::RefCountedObject<TestAudioDeviceModuleImpl>(
      std::move(capturer), std::move(renderer), speed);
}

std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer>
TestAudioDeviceModule::CreatePulsedNoiseCapturer(int16_t max_amplitude,
                                                 int sampling_frequency_in_hz,
                                                 int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer>(
      new PulsedNoiseCapturerImpl(max_amplitude, sampling_frequency_in_hz,
                                  num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateDiscardRenderer(int sampling_frequency_in_hz,
                                             int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new DiscardRenderer(sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(std::string filename,
                                           int sampling_frequency_in_hz,
                                           int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(filename, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(std::string filename) {
  WavReader reader(filename);
  int sampling_frequency_in_hz = reader.sample_rate();
  int num_channels = rtc::checked_cast<int>(reader.num_channels());
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(filename, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateWavFileWriter(std::string filename,
                                           int sampling_frequency_in_hz,
                                           int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new WavFileWriter(filename, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateBoundedWavFileWriter(std::string filename,
                                                  int sampling_frequency_in_hz,
                                                  int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new BoundedWavFileWriter(filename, sampling_frequency_in_hz,
                               num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(rtc::PlatformFile file,
                                           int sampling_frequency_in_hz,
                                           int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(file, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(rtc::PlatformFile file) {
  WavReader reader(file);
  int sampling_frequency_in_hz = reader.sample_rate();
  int num_channels = rtc::checked_cast<int>(reader.num_channels());
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(file, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateWavFileWriter(rtc::PlatformFile file,
                                           int sampling_frequency_in_hz,
                                           int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new WavFileWriter(file, sampling_frequency_in_hz, num_channels));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateBoundedWavFileWriter(rtc::PlatformFile file,
                                                  int sampling_frequency_in_hz,
                                                  int num_channels) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new BoundedWavFileWriter(file, sampling_frequency_in_hz, num_channels));
}

}  // namespace webrtc
