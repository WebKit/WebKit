/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/arraysize.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/test/auto_test/fakes/fake_media_process.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

class ExternalMediaTest : public AfterStreamingFixture {
 protected:
  void TestRegisterExternalMedia(int channel, webrtc::ProcessingTypes type) {
    FakeMediaProcess fake_media_process;
    EXPECT_EQ(0, voe_xmedia_->RegisterExternalMediaProcessing(
        channel, type, fake_media_process));
    Sleep(2000);

    TEST_LOG("Back to normal.\n");
    EXPECT_EQ(0, voe_xmedia_->DeRegisterExternalMediaProcessing(
        channel, type));
    Sleep(2000);
  }
};

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaProcessingOnAllChannelsAffectsPlayout) {
  TEST_LOG("Enabling external media processing: audio should be affected.\n");
  TestRegisterExternalMedia(-1, webrtc::kPlaybackAllChannelsMixed);
}

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaOnSingleChannelAffectsPlayout) {
  TEST_LOG("Enabling external media processing: audio should be affected.\n");
  TestRegisterExternalMedia(channel_, webrtc::kRecordingPerChannel);
}

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaOnAllChannelsMixedAffectsRecording) {
  SwitchToManualMicrophone();
  TEST_LOG("Speak and verify your voice is distorted.\n");
  TestRegisterExternalMedia(-1, webrtc::kRecordingAllChannelsMixed);
}

TEST_F(ExternalMediaTest,
       ExternalMixingCannotBeChangedDuringPlayback) {
  EXPECT_EQ(-1, voe_xmedia_->SetExternalMixing(channel_, true));
  EXPECT_EQ(-1, voe_xmedia_->SetExternalMixing(channel_, false));
}

TEST_F(ExternalMediaTest,
       ExternalMixingIsRequiredForGetAudioFrame) {
  webrtc::AudioFrame frame;
  EXPECT_EQ(-1, voe_xmedia_->GetAudioFrame(channel_, 0, &frame));
}

TEST_F(ExternalMediaTest,
       ExternalMixingPreventsAndRestoresRegularPlayback) {
  PausePlaying();
  ASSERT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, true));
  TEST_LOG("Verify that no sound is played out.\n");
  ResumePlaying();
  Sleep(1000);
  PausePlaying();
  ASSERT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, false));
  ResumePlaying();
  TEST_LOG("Verify that sound is played out.\n");
  ResumePlaying();
  Sleep(1000);
}

TEST_F(ExternalMediaTest,
       ExternalMixingWorks) {
  webrtc::AudioFrame frame;
  PausePlaying();
  EXPECT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, true));
  ResumePlaying();
  EXPECT_EQ(0, voe_xmedia_->GetAudioFrame(channel_, 0, &frame));
  EXPECT_GT(frame.sample_rate_hz_, 0);
  EXPECT_GT(frame.samples_per_channel_, 0U);
  PausePlaying();
  EXPECT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, false));
  ResumePlaying();
}

TEST_F(ExternalMediaTest,
       ExternalMixingResamplesToDesiredFrequency) {
  const int kValidFrequencies[] = {8000, 16000, 22000, 32000, 48000};
  webrtc::AudioFrame frame;
  PausePlaying();
  EXPECT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, true));
  ResumePlaying();
  for (size_t i = 0; i < arraysize(kValidFrequencies); i++) {
    int f = kValidFrequencies[i];
    EXPECT_EQ(0, voe_xmedia_->GetAudioFrame(channel_, f, &frame))
       << "Resampling succeeds for freq=" << f;
    EXPECT_EQ(f, frame.sample_rate_hz_);
    EXPECT_EQ(static_cast<size_t>(f / 100), frame.samples_per_channel_);
  }
  PausePlaying();
  EXPECT_EQ(0, voe_xmedia_->SetExternalMixing(channel_, false));
  ResumePlaying();
}
