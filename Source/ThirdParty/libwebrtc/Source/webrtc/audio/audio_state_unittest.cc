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

#include "webrtc/audio/audio_state.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

const int kSampleRate = 8000;
const int kNumberOfChannels = 1;
const int kBytesPerSample = 2;

struct ConfigHelper {
  ConfigHelper() : audio_mixer(AudioMixerImpl::Create()) {
    EXPECT_CALL(mock_voice_engine, RegisterVoiceEngineObserver(testing::_))
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock_voice_engine, DeRegisterVoiceEngineObserver())
        .WillOnce(testing::Return(0));
    EXPECT_CALL(mock_voice_engine, audio_device_module())
        .Times(testing::AtLeast(1));
    EXPECT_CALL(mock_voice_engine, audio_processing())
        .Times(testing::AtLeast(1));
    EXPECT_CALL(mock_voice_engine, audio_transport())
        .WillRepeatedly(testing::Return(&audio_transport));

    auto device = static_cast<MockAudioDeviceModule*>(
        voice_engine().audio_device_module());

    // Populate the audio transport proxy pointer to the most recent
    // transport connected to the Audio Device.
    ON_CALL(*device, RegisterAudioCallback(testing::_))
        .WillByDefault(testing::Invoke([this](AudioTransport* transport) {
          registered_audio_transport = transport;
          return 0;
        }));

    audio_state_config.voice_engine = &mock_voice_engine;
    audio_state_config.audio_mixer = audio_mixer;
  }
  AudioState::Config& config() { return audio_state_config; }
  MockVoiceEngine& voice_engine() { return mock_voice_engine; }
  rtc::scoped_refptr<AudioMixer> mixer() { return audio_mixer; }
  MockAudioTransport& original_audio_transport() { return audio_transport; }
  AudioTransport* audio_transport_proxy() { return registered_audio_transport; }

 private:
  testing::StrictMock<MockVoiceEngine> mock_voice_engine;
  AudioState::Config audio_state_config;
  rtc::scoped_refptr<AudioMixer> audio_mixer;
  MockAudioTransport audio_transport;
  AudioTransport* registered_audio_transport = nullptr;
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

TEST(AudioStateTest, TypingNoiseDetected) {
  ConfigHelper helper;
  std::unique_ptr<internal::AudioState> audio_state(
      new internal::AudioState(helper.config()));
  VoiceEngineObserver* voe_observer =
      static_cast<VoiceEngineObserver*>(audio_state.get());
  EXPECT_FALSE(audio_state->typing_noise_detected());

  voe_observer->CallbackOnError(-1, VE_NOT_INITED);
  EXPECT_FALSE(audio_state->typing_noise_detected());

  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_WARNING);
  EXPECT_TRUE(audio_state->typing_noise_detected());
  voe_observer->CallbackOnError(-1, VE_NOT_INITED);
  EXPECT_TRUE(audio_state->typing_noise_detected());

  voe_observer->CallbackOnError(-1, VE_TYPING_NOISE_OFF_WARNING);
  EXPECT_FALSE(audio_state->typing_noise_detected());
  voe_observer->CallbackOnError(-1, VE_NOT_INITED);
  EXPECT_FALSE(audio_state->typing_noise_detected());
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

  helper.audio_transport_proxy()->RecordedDataIsAvailable(
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
  helper.audio_transport_proxy()->NeedMorePlayData(
      kSampleRate / 100, kBytesPerSample, kNumberOfChannels, kSampleRate,
      audio_buffer, n_samples_out, &elapsed_time_ms, &ntp_time_ms);
}
}  // namespace test
}  // namespace webrtc
