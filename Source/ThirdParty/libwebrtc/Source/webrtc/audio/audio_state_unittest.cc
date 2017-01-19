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
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

struct ConfigHelper {
  ConfigHelper() {
    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(testing::_)).WillOnce(testing::Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(testing::Return(0));
    config_.voice_engine = &voice_engine_;
  }
  AudioState::Config& config() { return config_; }
  MockVoiceEngine& voice_engine() { return voice_engine_; }

 private:
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  AudioState::Config config_;
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
}  // namespace test
}  // namespace webrtc
