/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "webrtc/api/audio/audio_mixer.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/audio_mixer/default_output_rate_calculator.h"
#include "webrtc/test/gmock.h"

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::Return;

namespace webrtc {

namespace {

constexpr int kDefaultSampleRateHz = 48000;
constexpr int kId = 1;

// Utility function that resets the frame member variables with
// sensible defaults.
void ResetFrame(AudioFrame* frame) {
  frame->id_ = kId;
  frame->sample_rate_hz_ = kDefaultSampleRateHz;
  frame->num_channels_ = 1;

  // Frame duration 10ms.
  frame->samples_per_channel_ = kDefaultSampleRateHz / 100;
  frame->vad_activity_ = AudioFrame::kVadActive;
  frame->speech_type_ = AudioFrame::kNormalSpeech;
}

std::string ProduceDebugText(int sample_rate_hz,
                             int number_of_channels,
                             int number_of_sources) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz << " ";
  ss << "Number of channels: " << number_of_channels << " ";
  ss << "Number of sources: " << number_of_sources;
  return ss.str();
}

AudioFrame frame_for_mixing;

}  // namespace

class MockMixerAudioSource : public AudioMixer::Source {
 public:
  MockMixerAudioSource()
      : fake_audio_frame_info_(AudioMixer::Source::AudioFrameInfo::kNormal) {
    ON_CALL(*this, GetAudioFrameWithInfo(_, _))
        .WillByDefault(
            Invoke(this, &MockMixerAudioSource::FakeAudioFrameWithInfo));
    ON_CALL(*this, PreferredSampleRate())
        .WillByDefault(Return(kDefaultSampleRateHz));
  }

  MOCK_METHOD2(GetAudioFrameWithInfo,
               AudioFrameInfo(int sample_rate_hz, AudioFrame* audio_frame));

  MOCK_CONST_METHOD0(PreferredSampleRate, int());
  MOCK_CONST_METHOD0(Ssrc, int());

  AudioFrame* fake_frame() { return &fake_frame_; }
  AudioFrameInfo fake_info() { return fake_audio_frame_info_; }
  void set_fake_info(const AudioFrameInfo audio_frame_info) {
    fake_audio_frame_info_ = audio_frame_info;
  }

 private:
  AudioFrameInfo FakeAudioFrameWithInfo(int sample_rate_hz,
                                        AudioFrame* audio_frame) {
    audio_frame->CopyFrom(fake_frame_);
    audio_frame->sample_rate_hz_ = sample_rate_hz;
    audio_frame->samples_per_channel_ =
        rtc::CheckedDivExact(sample_rate_hz, 100);
    return fake_info();
  }

  AudioFrame fake_frame_;
  AudioFrameInfo fake_audio_frame_info_;
};

class CustomRateCalculator : public OutputRateCalculator {
 public:
  explicit CustomRateCalculator(int rate) : rate_(rate) {}
  int CalculateOutputRate(const std::vector<int>& preferred_rates) override {
    return rate_;
  }

 private:
  const int rate_;
};

// Creates participants from |frames| and |frame_info| and adds them
// to the mixer. Compares mixed status with |expected_status|
void MixAndCompare(
    const std::vector<AudioFrame>& frames,
    const std::vector<AudioMixer::Source::AudioFrameInfo>& frame_info,
    const std::vector<bool>& expected_status) {
  const size_t num_audio_sources = frames.size();
  RTC_DCHECK(frames.size() == frame_info.size());
  RTC_DCHECK(frame_info.size() == expected_status.size());

  const auto mixer = AudioMixerImpl::Create();
  std::vector<MockMixerAudioSource> participants(num_audio_sources);

  for (size_t i = 0; i < num_audio_sources; ++i) {
    participants[i].fake_frame()->CopyFrom(frames[i]);
    participants[i].set_fake_info(frame_info[i]);
  }

  for (size_t i = 0; i < num_audio_sources; ++i) {
    EXPECT_TRUE(mixer->AddSource(&participants[i]));
    EXPECT_CALL(participants[i], GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
        .Times(Exactly(1));
  }

  mixer->Mix(1, &frame_for_mixing);

  for (size_t i = 0; i < num_audio_sources; ++i) {
    EXPECT_EQ(expected_status[i],
              mixer->GetAudioSourceMixabilityStatusForTest(&participants[i]))
        << "Mixed status of AudioSource #" << i << " wrong.";
  }
}

void MixMonoAtGivenNativeRate(int native_sample_rate,
                              AudioFrame* mix_frame,
                              rtc::scoped_refptr<AudioMixer> mixer,
                              MockMixerAudioSource* audio_source) {
  ON_CALL(*audio_source, PreferredSampleRate())
      .WillByDefault(Return(native_sample_rate));
  audio_source->fake_frame()->sample_rate_hz_ = native_sample_rate;
  audio_source->fake_frame()->samples_per_channel_ = native_sample_rate / 100;

  mixer->Mix(1, mix_frame);
}

TEST(AudioMixer, LargestEnergyVadActiveMixed) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 3;

  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    ResetFrame(participants[i].fake_frame());

    // We set the 80-th sample value since the first 80 samples may be
    // modified by a ramped-in window.
    participants[i].fake_frame()->data_[80] = i;

    EXPECT_TRUE(mixer->AddSource(&participants[i]));
    EXPECT_CALL(participants[i], GetAudioFrameWithInfo(_, _)).Times(Exactly(1));
  }

  // Last participant gives audio frame with passive VAD, although it has the
  // largest energy.
  participants[kAudioSources - 1].fake_frame()->vad_activity_ =
      AudioFrame::kVadPassive;

  AudioFrame audio_frame;
  mixer->Mix(1,  // number of channels
             &audio_frame);

  for (int i = 0; i < kAudioSources; ++i) {
    bool is_mixed =
        mixer->GetAudioSourceMixabilityStatusForTest(&participants[i]);
    if (i == kAudioSources - 1 ||
        i < kAudioSources - 1 -
                AudioMixerImpl::kMaximumAmountOfMixedAudioSources) {
      EXPECT_FALSE(is_mixed) << "Mixing status of AudioSource #" << i
                             << " wrong.";
    } else {
      EXPECT_TRUE(is_mixed) << "Mixing status of AudioSource #" << i
                            << " wrong.";
    }
  }
}

TEST(AudioMixer, FrameNotModifiedForSingleParticipant) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());
  const size_t n_samples = participant.fake_frame()->samples_per_channel_;

  // Modify the frame so that it's not zero.
  for (size_t j = 0; j < n_samples; ++j) {
    participant.fake_frame()->data_[j] = static_cast<int16_t>(j);
  }

  EXPECT_TRUE(mixer->AddSource(&participant));
  EXPECT_CALL(participant, GetAudioFrameWithInfo(_, _)).Times(Exactly(2));

  AudioFrame audio_frame;
  // Two mix iteration to compare after the ramp-up step.
  for (int i = 0; i < 2; ++i) {
    mixer->Mix(1,  // number of channels
               &audio_frame);
  }

  EXPECT_EQ(
      0, memcmp(participant.fake_frame()->data_, audio_frame.data_, n_samples));
}

TEST(AudioMixer, SourceAtNativeRateShouldNeverResample) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource audio_source;
  ResetFrame(audio_source.fake_frame());

  mixer->AddSource(&audio_source);

  for (auto frequency : {8000, 16000, 32000, 48000}) {
    EXPECT_CALL(audio_source, GetAudioFrameWithInfo(frequency, _))
        .Times(Exactly(1));

    MixMonoAtGivenNativeRate(frequency, &frame_for_mixing, mixer,
                             &audio_source);
  }
}

TEST(AudioMixer, MixerShouldMixAtNativeSourceRate) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource audio_source;
  ResetFrame(audio_source.fake_frame());

  mixer->AddSource(&audio_source);

  for (auto frequency : {8000, 16000, 32000, 48000}) {
    MixMonoAtGivenNativeRate(frequency, &frame_for_mixing, mixer,
                             &audio_source);

    EXPECT_EQ(frequency, frame_for_mixing.sample_rate_hz_);
  }
}

TEST(AudioMixer, MixerShouldAlwaysMixAtNativeRate) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());
  mixer->AddSource(&participant);

  const int needed_frequency = 44100;
  ON_CALL(participant, PreferredSampleRate())
      .WillByDefault(Return(needed_frequency));

  // We expect mixing frequency to be native and >= needed_frequency.
  const int expected_mix_frequency = 48000;
  EXPECT_CALL(participant, GetAudioFrameWithInfo(expected_mix_frequency, _))
      .Times(Exactly(1));
  participant.fake_frame()->sample_rate_hz_ = expected_mix_frequency;
  participant.fake_frame()->samples_per_channel_ = expected_mix_frequency / 100;

  mixer->Mix(1, &frame_for_mixing);

  EXPECT_EQ(48000, frame_for_mixing.sample_rate_hz_);
}

// Check that the mixing rate is always >= participants preferred rate.
TEST(AudioMixer, ShouldNotCauseQualityLossForMultipleSources) {
  const auto mixer = AudioMixerImpl::Create();

  std::vector<MockMixerAudioSource> audio_sources(2);
  const std::vector<int> source_sample_rates = {8000, 16000};
  for (int i = 0; i < 2; ++i) {
    auto& source = audio_sources[i];
    ResetFrame(source.fake_frame());
    mixer->AddSource(&source);
    const auto sample_rate = source_sample_rates[i];
    EXPECT_CALL(source, PreferredSampleRate()).WillOnce(Return(sample_rate));

    EXPECT_CALL(source, GetAudioFrameWithInfo(testing::Ge(sample_rate), _));
  }
  mixer->Mix(1, &frame_for_mixing);
}

TEST(AudioMixer, ParticipantNumberOfChannels) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());

  EXPECT_TRUE(mixer->AddSource(&participant));
  for (size_t number_of_channels : {1, 2}) {
    EXPECT_CALL(participant, GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
        .Times(Exactly(1));
    mixer->Mix(number_of_channels, &frame_for_mixing);
    EXPECT_EQ(number_of_channels, frame_for_mixing.num_channels_);
  }
}

// Maximal amount of participants are mixed one iteration, then
// another participant with higher energy is added.
TEST(AudioMixer, RampedOutSourcesShouldNotBeMarkedMixed) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 1;

  const auto mixer = AudioMixerImpl::Create();
  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    ResetFrame(participants[i].fake_frame());
    // Set the participant audio energy to increase with the index
    // |i|.
    participants[i].fake_frame()->data_[0] = 100 * i;
  }

  // Add all participants but the loudest for mixing.
  for (int i = 0; i < kAudioSources - 1; ++i) {
    EXPECT_TRUE(mixer->AddSource(&participants[i]));
    EXPECT_CALL(participants[i], GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
        .Times(Exactly(1));
  }

  // First mixer iteration
  mixer->Mix(1, &frame_for_mixing);

  // All participants but the loudest should have been mixed.
  for (int i = 0; i < kAudioSources - 1; ++i) {
    EXPECT_TRUE(mixer->GetAudioSourceMixabilityStatusForTest(&participants[i]))
        << "Mixed status of AudioSource #" << i << " wrong.";
  }

  // Add new participant with higher energy.
  EXPECT_TRUE(mixer->AddSource(&participants[kAudioSources - 1]));
  for (int i = 0; i < kAudioSources; ++i) {
    EXPECT_CALL(participants[i], GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
        .Times(Exactly(1));
  }

  mixer->Mix(1, &frame_for_mixing);

  // The most quiet participant should not have been mixed.
  EXPECT_FALSE(mixer->GetAudioSourceMixabilityStatusForTest(&participants[0]))
      << "Mixed status of AudioSource #0 wrong.";

  // The loudest participants should have been mixed.
  for (int i = 1; i < kAudioSources; ++i) {
    EXPECT_EQ(true,
              mixer->GetAudioSourceMixabilityStatusForTest(&participants[i]))
        << "Mixed status of AudioSource #" << i << " wrong.";
  }
}

// This test checks that the initialization and participant addition
// can be done on a different thread.
TEST(AudioMixer, ConstructFromOtherThread) {
  std::unique_ptr<rtc::Thread> init_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> participant_thread = rtc::Thread::Create();
  init_thread->Start();
  const auto mixer = init_thread->Invoke<rtc::scoped_refptr<AudioMixer>>(
      RTC_FROM_HERE,
      // Since AudioMixerImpl::Create is overloaded, we have to
      // specify the type of which version we want.
      static_cast<rtc::scoped_refptr<AudioMixerImpl>(*)()>(
          &AudioMixerImpl::Create));
  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());

  participant_thread->Start();
  EXPECT_TRUE(participant_thread->Invoke<int>(
      RTC_FROM_HERE,
      rtc::Bind(&AudioMixer::AddSource, mixer.get(), &participant)));

  EXPECT_CALL(participant, GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
      .Times(Exactly(1));

  // Do one mixer iteration
  mixer->Mix(1, &frame_for_mixing);
}

TEST(AudioMixer, MutedShouldMixAfterUnmuted) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frame_info[0] = AudioMixer::Source::AudioFrameInfo::kMuted;
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, PassiveShouldMixAfterNormal) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frames[0].vad_activity_ = AudioFrame::kVadPassive;
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, ActiveShouldMixBeforeLoud) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frames[0].vad_activity_ = AudioFrame::kVadPassive;
  std::fill(frames[0].data_, frames[0].data_ + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, UnmutedShouldMixBeforeLoud) {
  constexpr int kAudioSources =
      AudioMixerImpl::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frame_info[0] = AudioMixer::Source::AudioFrameInfo::kMuted;
  std::fill(frames[0].data_, frames[0].data_ + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, MixingRateShouldBeDecidedByRateCalculator) {
  constexpr int kOutputRate = 22000;
  const auto mixer =
      AudioMixerImpl::Create(std::unique_ptr<OutputRateCalculator>(
                                 new CustomRateCalculator(kOutputRate)),
                             true);
  MockMixerAudioSource audio_source;
  mixer->AddSource(&audio_source);
  ResetFrame(audio_source.fake_frame());

  EXPECT_CALL(audio_source, GetAudioFrameWithInfo(kOutputRate, _))
      .Times(Exactly(1));

  mixer->Mix(1, &frame_for_mixing);
}

TEST(AudioMixer, ZeroSourceRateShouldBeDecidedByRateCalculator) {
  constexpr int kOutputRate = 8000;
  const auto mixer =
      AudioMixerImpl::Create(std::unique_ptr<OutputRateCalculator>(
                                 new CustomRateCalculator(kOutputRate)),
                             true);

  mixer->Mix(1, &frame_for_mixing);

  EXPECT_EQ(kOutputRate, frame_for_mixing.sample_rate_hz_);
}

TEST(AudioMixer, NoLimiterBasicApiCalls) {
  const auto mixer = AudioMixerImpl::Create(
      std::unique_ptr<OutputRateCalculator>(new DefaultOutputRateCalculator()),
      false);
  mixer->Mix(1, &frame_for_mixing);
}

TEST(AudioMixer, AnyRateIsPossibleWithNoLimiter) {
  // No APM limiter means no AudioProcessing::NativeRate restriction
  // on mixing rate. The rate has to be divisible by 100 since we use
  // 10 ms frames, though.
  for (const auto rate : {8000, 20000, 24000, 32000, 44100}) {
    for (const size_t number_of_channels : {1, 2}) {
      for (const auto number_of_sources : {0, 1, 2, 3, 4}) {
        SCOPED_TRACE(
            ProduceDebugText(rate, number_of_sources, number_of_sources));
        const auto mixer =
            AudioMixerImpl::Create(std::unique_ptr<OutputRateCalculator>(
                                       new CustomRateCalculator(rate)),
                                   false);

        std::vector<MockMixerAudioSource> sources(number_of_sources);
        for (auto& source : sources) {
          mixer->AddSource(&source);
        }

        mixer->Mix(number_of_channels, &frame_for_mixing);
        EXPECT_EQ(rate, frame_for_mixing.sample_rate_hz_);
        EXPECT_EQ(number_of_channels, frame_for_mixing.num_channels_);
      }
    }
  }
}
}  // namespace webrtc
