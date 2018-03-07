/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "audio/audio_state.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "test/gtest.h"
#include "test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

const int kSampleRate = 8000;
const int kNumberOfChannels = 1;
const int kBytesPerSample = 2;

struct ConfigHelper {
  ConfigHelper() : audio_mixer(AudioMixerImpl::Create()) {
    EXPECT_CALL(mock_voice_engine, audio_transport())
        .WillRepeatedly(testing::Return(&audio_transport));

    audio_state_config.voice_engine = &mock_voice_engine;
    audio_state_config.audio_mixer = audio_mixer;
    audio_state_config.audio_processing =
        new rtc::RefCountedObject<MockAudioProcessing>();
  }
  AudioState::Config& config() { return audio_state_config; }
  MockVoiceEngine& voice_engine() { return mock_voice_engine; }
  rtc::scoped_refptr<AudioMixer> mixer() { return audio_mixer; }
  MockAudioTransport& original_audio_transport() { return audio_transport; }

 private:
  testing::StrictMock<MockVoiceEngine> mock_voice_engine;
  AudioState::Config audio_state_config;
  rtc::scoped_refptr<AudioMixer> audio_mixer;
  MockAudioTransport audio_transport;
};

class FakeAudioSource : public AudioMixer::Source {
 public:
  // TODO(aleloi): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  int Ssrc() const /*override*/ { return 0; }

  int PreferredSampleRate() const /*override*/ { return kSampleRate; }

  MOCK_METHOD2(GetAudioFrameWithInfo,
               AudioFrameInfo(int sample_rate_hz, AudioFrame* audio_frame));
};

}  // namespace

TEST(AudioStateTest, Create) {
  ConfigHelper helper;
  rtc::scoped_refptr<AudioState> audio_state =
      AudioState::Create(helper.config());
  EXPECT_TRUE(audio_state.get());
}

TEST(AudioStateTest, ConstructDestruct) {
  ConfigHelper helper;
  std::unique_ptr<internal::AudioState> audio_state(
      new internal::AudioState(helper.config()));
}

TEST(AudioStateTest, GetVoiceEngine) {
  ConfigHelper helper;
  std::unique_ptr<internal::AudioState> audio_state(
      new internal::AudioState(helper.config()));
  EXPECT_EQ(audio_state->voice_engine(), &helper.voice_engine());
}

// Test that RecordedDataIsAvailable calls get to the original transport.
TEST(AudioStateAudioPathTest, RecordedAudioArrivesAtOriginalTransport) {
  ConfigHelper helper;

  rtc::scoped_refptr<AudioState> audio_state =
      AudioState::Create(helper.config());

  // Setup completed. Ensure call of original transport is forwarded to new.
  uint32_t new_mic_level;
  EXPECT_CALL(
      helper.original_audio_transport(),
      RecordedDataIsAvailable(nullptr, kSampleRate / 100, kBytesPerSample,
                              kNumberOfChannels, kSampleRate, 0, 0, 0, false,
                              testing::Ref(new_mic_level)));

  audio_state->audio_transport()->RecordedDataIsAvailable(
      nullptr, kSampleRate / 100, kBytesPerSample, kNumberOfChannels,
      kSampleRate, 0, 0, 0, false, new_mic_level);
}

TEST(AudioStateAudioPathTest,
     QueryingProxyForAudioShouldResultInGetAudioCallOnMixerSource) {
  ConfigHelper helper;

  rtc::scoped_refptr<AudioState> audio_state =
      AudioState::Create(helper.config());

  FakeAudioSource fake_source;

  helper.mixer()->AddSource(&fake_source);

  EXPECT_CALL(fake_source, GetAudioFrameWithInfo(testing::_, testing::_))
      .WillOnce(
          testing::Invoke([](int sample_rate_hz, AudioFrame* audio_frame) {
            audio_frame->sample_rate_hz_ = sample_rate_hz;
            audio_frame->samples_per_channel_ = sample_rate_hz / 100;
            audio_frame->num_channels_ = kNumberOfChannels;
            return AudioMixer::Source::AudioFrameInfo::kNormal;
          }));

  int16_t audio_buffer[kSampleRate / 100 * kNumberOfChannels];
  size_t n_samples_out;
  int64_t elapsed_time_ms;
  int64_t ntp_time_ms;
  audio_state->audio_transport()->NeedMorePlayData(
      kSampleRate / 100, kBytesPerSample, kNumberOfChannels, kSampleRate,
      audio_buffer, n_samples_out, &elapsed_time_ms, &ntp_time_ms);
}
}  // namespace test
}  // namespace webrtc
