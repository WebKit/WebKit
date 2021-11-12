/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/audio_mixer_impl.h"

#include <string.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/units/timestamp.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace webrtc {

namespace {

constexpr int kDefaultSampleRateHz = 48000;

// Utility function that resets the frame member variables with
// sensible defaults.
void ResetFrame(AudioFrame* frame) {
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
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz << " ";
  ss << "Number of channels: " << number_of_channels << " ";
  ss << "Number of sources: " << number_of_sources;
  return ss.Release();
}

AudioFrame frame_for_mixing;

}  // namespace

class MockMixerAudioSource : public ::testing::NiceMock<AudioMixer::Source> {
 public:
  MockMixerAudioSource()
      : fake_audio_frame_info_(AudioMixer::Source::AudioFrameInfo::kNormal) {
    ON_CALL(*this, GetAudioFrameWithInfo(_, _))
        .WillByDefault(
            Invoke(this, &MockMixerAudioSource::FakeAudioFrameWithInfo));
    ON_CALL(*this, PreferredSampleRate())
        .WillByDefault(Return(kDefaultSampleRateHz));
  }

  MOCK_METHOD(AudioFrameInfo,
              GetAudioFrameWithInfo,
              (int sample_rate_hz, AudioFrame* audio_frame),
              (override));

  MOCK_METHOD(int, PreferredSampleRate, (), (const, override));
  MOCK_METHOD(int, Ssrc, (), (const, override));

  AudioFrame* fake_frame() { return &fake_frame_; }
  AudioFrameInfo fake_info() { return fake_audio_frame_info_; }
  void set_fake_info(const AudioFrameInfo audio_frame_info) {
    fake_audio_frame_info_ = audio_frame_info;
  }

  void set_packet_infos(const RtpPacketInfos& packet_infos) {
    packet_infos_ = packet_infos;
  }

 private:
  AudioFrameInfo FakeAudioFrameWithInfo(int sample_rate_hz,
                                        AudioFrame* audio_frame) {
    audio_frame->CopyFrom(fake_frame_);
    audio_frame->sample_rate_hz_ = sample_rate_hz;
    audio_frame->samples_per_channel_ =
        rtc::CheckedDivExact(sample_rate_hz, 100);
    audio_frame->packet_infos_ = packet_infos_;
    return fake_info();
  }

  AudioFrame fake_frame_;
  AudioFrameInfo fake_audio_frame_info_;
  RtpPacketInfos packet_infos_;
};

class CustomRateCalculator : public OutputRateCalculator {
 public:
  explicit CustomRateCalculator(int rate) : rate_(rate) {}
  int CalculateOutputRateFromRange(
      rtc::ArrayView<const int> preferred_rates) override {
    return rate_;
  }

 private:
  const int rate_;
};

// Creates participants from `frames` and `frame_info` and adds them
// to the mixer. Compares mixed status with `expected_status`
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
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 3;

  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    ResetFrame(participants[i].fake_frame());

    // We set the 80-th sample value since the first 80 samples may be
    // modified by a ramped-in window.
    participants[i].fake_frame()->mutable_data()[80] = i;

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
                AudioMixerImpl::kDefaultNumberOfMixedAudioSources) {
      EXPECT_FALSE(is_mixed)
          << "Mixing status of AudioSource #" << i << " wrong.";
    } else {
      EXPECT_TRUE(is_mixed)
          << "Mixing status of AudioSource #" << i << " wrong.";
    }
  }
}

TEST(AudioMixer, FrameNotModifiedForSingleParticipant) {
  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());
  const size_t n_samples = participant.fake_frame()->samples_per_channel_;

  // Modify the frame so that it's not zero.
  int16_t* fake_frame_data = participant.fake_frame()->mutable_data();
  for (size_t j = 0; j < n_samples; ++j) {
    fake_frame_data[j] = static_cast<int16_t>(j);
  }

  EXPECT_TRUE(mixer->AddSource(&participant));
  EXPECT_CALL(participant, GetAudioFrameWithInfo(_, _)).Times(Exactly(2));

  AudioFrame audio_frame;
  // Two mix iteration to compare after the ramp-up step.
  for (int i = 0; i < 2; ++i) {
    mixer->Mix(1,  // number of channels
               &audio_frame);
  }

  EXPECT_EQ(0, memcmp(participant.fake_frame()->data(), audio_frame.data(),
                      n_samples));
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

    EXPECT_CALL(source, GetAudioFrameWithInfo(::testing::Ge(sample_rate), _));
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
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 1;

  const auto mixer = AudioMixerImpl::Create();
  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    ResetFrame(participants[i].fake_frame());
    // Set the participant audio energy to increase with the index
    // `i`.
    participants[i].fake_frame()->mutable_data()[0] = 100 * i;
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
  TaskQueueForTest init_queue("init");
  rtc::scoped_refptr<AudioMixer> mixer;
  init_queue.SendTask([&mixer]() { mixer = AudioMixerImpl::Create(); },
                      RTC_FROM_HERE);

  MockMixerAudioSource participant;
  EXPECT_CALL(participant, PreferredSampleRate())
      .WillRepeatedly(Return(kDefaultSampleRateHz));

  ResetFrame(participant.fake_frame());

  TaskQueueForTest participant_queue("participant");
  participant_queue.SendTask(
      [&mixer, &participant]() { mixer->AddSource(&participant); },
      RTC_FROM_HERE);

  EXPECT_CALL(participant, GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
      .Times(Exactly(1));

  // Do one mixer iteration
  mixer->Mix(1, &frame_for_mixing);
}

TEST(AudioMixer, MutedShouldMixAfterUnmuted) {
  constexpr int kAudioSources =
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 1;

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
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 1;

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
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frames[0].vad_activity_ = AudioFrame::kVadPassive;
  int16_t* frame_data = frames[0].mutable_data();
  std::fill(frame_data, frame_data + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, ShouldMixUpToSpecifiedNumberOfSourcesToMix) {
  constexpr int kAudioSources = 5;
  constexpr int kSourcesToMix = 2;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  // Set up to kSourceToMix sources with kVadActive so that they're mixed.
  const std::vector<AudioFrame::VADActivity> kVadActivities = {
      AudioFrame::kVadUnknown, AudioFrame::kVadPassive, AudioFrame::kVadPassive,
      AudioFrame::kVadActive, AudioFrame::kVadActive};
  // Populate VAD and frame for all sources.
  for (int i = 0; i < kAudioSources; i++) {
    frames[i].vad_activity_ = kVadActivities[i];
  }

  std::vector<MockMixerAudioSource> participants(kAudioSources);
  for (int i = 0; i < kAudioSources; ++i) {
    participants[i].fake_frame()->CopyFrom(frames[i]);
    participants[i].set_fake_info(frame_info[i]);
  }

  const auto mixer = AudioMixerImpl::Create(kSourcesToMix);
  for (int i = 0; i < kAudioSources; ++i) {
    EXPECT_TRUE(mixer->AddSource(&participants[i]));
    EXPECT_CALL(participants[i], GetAudioFrameWithInfo(kDefaultSampleRateHz, _))
        .Times(Exactly(1));
  }

  mixer->Mix(1, &frame_for_mixing);

  std::vector<bool> expected_status = {false, false, false, true, true};
  for (int i = 0; i < kAudioSources; ++i) {
    EXPECT_EQ(expected_status[i],
              mixer->GetAudioSourceMixabilityStatusForTest(&participants[i]))
        << "Wrong mix status for source #" << i << " is wrong";
  }
}

TEST(AudioMixer, UnmutedShouldMixBeforeLoud) {
  constexpr int kAudioSources =
      AudioMixerImpl::kDefaultNumberOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<AudioMixer::Source::AudioFrameInfo> frame_info(
      kAudioSources, AudioMixer::Source::AudioFrameInfo::kNormal);
  frame_info[0] = AudioMixer::Source::AudioFrameInfo::kMuted;
  int16_t* frame_data = frames[0].mutable_data();
  std::fill(frame_data, frame_data + kDefaultSampleRateHz / 100,
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
          ResetFrame(source.fake_frame());
          mixer->AddSource(&source);
        }

        mixer->Mix(number_of_channels, &frame_for_mixing);
        EXPECT_EQ(rate, frame_for_mixing.sample_rate_hz_);
        EXPECT_EQ(number_of_channels, frame_for_mixing.num_channels_);
      }
    }
  }
}

TEST(AudioMixer, MultipleChannelsOneParticipant) {
  // Set up a participant with a 6-channel frame, and make sure a 6-channel
  // frame with the right sample values comes out from the mixer. There are 2
  // Mix calls because of ramp-up.
  constexpr size_t kNumberOfChannels = 6;
  MockMixerAudioSource source;
  ResetFrame(source.fake_frame());
  const auto mixer = AudioMixerImpl::Create();
  mixer->AddSource(&source);
  mixer->Mix(1, &frame_for_mixing);
  auto* frame = source.fake_frame();
  frame->num_channels_ = kNumberOfChannels;
  std::fill(frame->mutable_data(),
            frame->mutable_data() + AudioFrame::kMaxDataSizeSamples, 0);
  for (size_t i = 0; i < kNumberOfChannels; ++i) {
    frame->mutable_data()[100 * frame->num_channels_ + i] = 1000 * i;
  }

  mixer->Mix(kNumberOfChannels, &frame_for_mixing);

  EXPECT_EQ(frame_for_mixing.num_channels_, kNumberOfChannels);
  for (size_t i = 0; i < kNumberOfChannels; ++i) {
    EXPECT_EQ(frame_for_mixing.data()[100 * frame_for_mixing.num_channels_ + i],
              static_cast<int16_t>(1000 * i));
  }
}

TEST(AudioMixer, MultipleChannelsManyParticipants) {
  // Sets up 2 participants. One has a 6-channel frame. Make sure a 6-channel
  // frame with the right sample values comes out from the mixer. There are 2
  // Mix calls because of ramp-up.
  constexpr size_t kNumberOfChannels = 6;
  MockMixerAudioSource source;
  const auto mixer = AudioMixerImpl::Create();
  mixer->AddSource(&source);
  ResetFrame(source.fake_frame());
  mixer->Mix(1, &frame_for_mixing);
  auto* frame = source.fake_frame();
  frame->num_channels_ = kNumberOfChannels;
  std::fill(frame->mutable_data(),
            frame->mutable_data() + AudioFrame::kMaxDataSizeSamples, 0);
  for (size_t i = 0; i < kNumberOfChannels; ++i) {
    frame->mutable_data()[100 * frame->num_channels_ + i] = 1000 * i;
  }
  MockMixerAudioSource other_source;
  ResetFrame(other_source.fake_frame());
  mixer->AddSource(&other_source);

  mixer->Mix(kNumberOfChannels, &frame_for_mixing);

  EXPECT_EQ(frame_for_mixing.num_channels_, kNumberOfChannels);
  for (size_t i = 0; i < kNumberOfChannels; ++i) {
    EXPECT_EQ(frame_for_mixing.data()[100 * frame_for_mixing.num_channels_ + i],
              static_cast<int16_t>(1000 * i));
  }
}

TEST(AudioMixer, ShouldIncludeRtpPacketInfoFromAllMixedSources) {
  const uint32_t kSsrc0 = 10;
  const uint32_t kSsrc1 = 11;
  const uint32_t kSsrc2 = 12;
  const uint32_t kCsrc0 = 20;
  const uint32_t kCsrc1 = 21;
  const uint32_t kCsrc2 = 22;
  const uint32_t kCsrc3 = 23;
  const int kAudioLevel0 = 10;
  const int kAudioLevel1 = 40;
  const absl::optional<uint32_t> kAudioLevel2 = absl::nullopt;
  const uint32_t kRtpTimestamp0 = 300;
  const uint32_t kRtpTimestamp1 = 400;
  const Timestamp kReceiveTime0 = Timestamp::Millis(10);
  const Timestamp kReceiveTime1 = Timestamp::Millis(20);

  const RtpPacketInfo kPacketInfo0(kSsrc0, {kCsrc0, kCsrc1}, kRtpTimestamp0,
                                   kAudioLevel0, absl::nullopt, kReceiveTime0);
  const RtpPacketInfo kPacketInfo1(kSsrc1, {kCsrc2}, kRtpTimestamp1,
                                   kAudioLevel1, absl::nullopt, kReceiveTime1);
  const RtpPacketInfo kPacketInfo2(kSsrc2, {kCsrc3}, kRtpTimestamp1,
                                   kAudioLevel2, absl::nullopt, kReceiveTime1);

  const auto mixer = AudioMixerImpl::Create();

  MockMixerAudioSource source;
  source.set_packet_infos(RtpPacketInfos({kPacketInfo0}));
  mixer->AddSource(&source);
  ResetFrame(source.fake_frame());
  mixer->Mix(1, &frame_for_mixing);

  MockMixerAudioSource other_source;
  other_source.set_packet_infos(RtpPacketInfos({kPacketInfo1, kPacketInfo2}));
  ResetFrame(other_source.fake_frame());
  mixer->AddSource(&other_source);

  mixer->Mix(/*number_of_channels=*/1, &frame_for_mixing);

  EXPECT_THAT(frame_for_mixing.packet_infos_,
              UnorderedElementsAre(kPacketInfo0, kPacketInfo1, kPacketInfo2));
}

TEST(AudioMixer, MixerShouldIncludeRtpPacketInfoFromMixedSourcesOnly) {
  const uint32_t kSsrc0 = 10;
  const uint32_t kSsrc1 = 11;
  const uint32_t kSsrc2 = 21;
  const uint32_t kCsrc0 = 30;
  const uint32_t kCsrc1 = 31;
  const uint32_t kCsrc2 = 32;
  const uint32_t kCsrc3 = 33;
  const int kAudioLevel0 = 10;
  const absl::optional<uint32_t> kAudioLevelMissing = absl::nullopt;
  const uint32_t kRtpTimestamp0 = 300;
  const uint32_t kRtpTimestamp1 = 400;
  const Timestamp kReceiveTime0 = Timestamp::Millis(10);
  const Timestamp kReceiveTime1 = Timestamp::Millis(20);

  const RtpPacketInfo kPacketInfo0(kSsrc0, {kCsrc0, kCsrc1}, kRtpTimestamp0,
                                   kAudioLevel0, absl::nullopt, kReceiveTime0);
  const RtpPacketInfo kPacketInfo1(kSsrc1, {kCsrc2}, kRtpTimestamp1,
                                   kAudioLevelMissing, absl::nullopt,
                                   kReceiveTime1);
  const RtpPacketInfo kPacketInfo2(kSsrc2, {kCsrc3}, kRtpTimestamp1,
                                   kAudioLevelMissing, absl::nullopt,
                                   kReceiveTime1);

  const auto mixer = AudioMixerImpl::Create(/*max_sources_to_mix=*/2);

  MockMixerAudioSource source1;
  source1.set_packet_infos(RtpPacketInfos({kPacketInfo0}));
  mixer->AddSource(&source1);
  ResetFrame(source1.fake_frame());
  mixer->Mix(1, &frame_for_mixing);

  MockMixerAudioSource source2;
  source2.set_packet_infos(RtpPacketInfos({kPacketInfo1}));
  ResetFrame(source2.fake_frame());
  mixer->AddSource(&source2);

  // The mixer prioritizes kVadActive over kVadPassive.
  // We limit the number of sources to mix to 2 and set the third source's VAD
  // activity to kVadPassive so that it will not be added to the mix.
  MockMixerAudioSource source3;
  source3.set_packet_infos(RtpPacketInfos({kPacketInfo2}));
  ResetFrame(source3.fake_frame());
  source3.fake_frame()->vad_activity_ = AudioFrame::kVadPassive;
  mixer->AddSource(&source3);

  mixer->Mix(/*number_of_channels=*/1, &frame_for_mixing);

  EXPECT_THAT(frame_for_mixing.packet_infos_,
              UnorderedElementsAre(kPacketInfo0, kPacketInfo1));
}

class HighOutputRateCalculator : public OutputRateCalculator {
 public:
  static const int kDefaultFrequency = 76000;
  int CalculateOutputRateFromRange(
      rtc::ArrayView<const int> preferred_sample_rates) override {
    return kDefaultFrequency;
  }
  ~HighOutputRateCalculator() override {}
};
const int HighOutputRateCalculator::kDefaultFrequency;

TEST(AudioMixerDeathTest, MultipleChannelsAndHighRate) {
  constexpr size_t kSamplesPerChannel =
      HighOutputRateCalculator::kDefaultFrequency / 100;
  // As many channels as an AudioFrame can fit:
  constexpr size_t kNumberOfChannels =
      AudioFrame::kMaxDataSizeSamples / kSamplesPerChannel;
  MockMixerAudioSource source;
  const auto mixer = AudioMixerImpl::Create(
      std::make_unique<HighOutputRateCalculator>(), true);
  mixer->AddSource(&source);
  ResetFrame(source.fake_frame());
  mixer->Mix(1, &frame_for_mixing);
  auto* frame = source.fake_frame();
  frame->num_channels_ = kNumberOfChannels;
  frame->sample_rate_hz_ = HighOutputRateCalculator::kDefaultFrequency;
  frame->samples_per_channel_ = kSamplesPerChannel;

  std::fill(frame->mutable_data(),
            frame->mutable_data() + AudioFrame::kMaxDataSizeSamples, 0);
  MockMixerAudioSource other_source;
  ResetFrame(other_source.fake_frame());
  auto* other_frame = other_source.fake_frame();
  other_frame->num_channels_ = kNumberOfChannels;
  other_frame->sample_rate_hz_ = HighOutputRateCalculator::kDefaultFrequency;
  other_frame->samples_per_channel_ = kSamplesPerChannel;
  mixer->AddSource(&other_source);

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
  EXPECT_DEATH(mixer->Mix(kNumberOfChannels, &frame_for_mixing), "");
#elif !RTC_DCHECK_IS_ON
  mixer->Mix(kNumberOfChannels, &frame_for_mixing);
  EXPECT_EQ(frame_for_mixing.num_channels_, kNumberOfChannels);
  EXPECT_EQ(frame_for_mixing.sample_rate_hz_,
            HighOutputRateCalculator::kDefaultFrequency);
#endif
}

}  // namespace webrtc
