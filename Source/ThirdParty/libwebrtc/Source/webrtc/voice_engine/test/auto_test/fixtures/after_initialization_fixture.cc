/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_initialization_fixture.h"

class TestErrorObserver : public webrtc::VoiceEngineObserver {
 public:
  TestErrorObserver() {}
  virtual ~TestErrorObserver() {}
  void CallbackOnError(int channel, int error_code) {
    ADD_FAILURE() << "Unexpected error on channel " << channel <<
        ": error code " << error_code;
  }
};

AfterInitializationFixture::AfterInitializationFixture()
    : error_observer_(new TestErrorObserver()) {
  webrtc::Config config;
  config.Set<webrtc::ExperimentalAgc>(new webrtc::ExperimentalAgc(false));
  webrtc::AudioProcessing* audioproc = webrtc::AudioProcessing::Create(config);

  EXPECT_EQ(0, voe_base_->Init(NULL, audioproc));
  EXPECT_EQ(0, voe_base_->RegisterVoiceEngineObserver(*error_observer_));
}

AfterInitializationFixture::~AfterInitializationFixture() {
  EXPECT_EQ(0, voe_base_->DeRegisterVoiceEngineObserver());
}
