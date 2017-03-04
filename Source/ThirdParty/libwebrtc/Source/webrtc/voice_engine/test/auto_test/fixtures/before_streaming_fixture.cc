/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_streaming_fixture.h"

BeforeStreamingFixture::BeforeStreamingFixture()
    : channel_(voe_base_->CreateChannel()),
      transport_(NULL) {
  EXPECT_GE(channel_, 0);

  fake_microphone_input_file_ =
      webrtc::test::ResourcePath("voice_engine/audio_long16", "pcm");

  SetUpLocalPlayback();
  RestartFakeMicrophone();
}

BeforeStreamingFixture::~BeforeStreamingFixture() {
  voe_file_->StopPlayingFileAsMicrophone(channel_);
  PausePlaying();

  EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
  voe_base_->DeleteChannel(channel_);
  delete transport_;
}

void BeforeStreamingFixture::SwitchToManualMicrophone() {
  EXPECT_EQ(0, voe_file_->StopPlayingFileAsMicrophone(channel_));

  TEST_LOG("You need to speak manually into the microphone for this test.\n");
  TEST_LOG("Please start speaking now.\n");
  Sleep(1000);
}

void BeforeStreamingFixture::RestartFakeMicrophone() {
  EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(
        channel_, fake_microphone_input_file_.c_str(), true, true));
}

void BeforeStreamingFixture::PausePlaying() {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
}

void BeforeStreamingFixture::ResumePlaying() {
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
}

void BeforeStreamingFixture::WaitForTransmittedPackets(int32_t packet_count) {
  transport_->WaitForTransmittedPackets(packet_count);
}

void BeforeStreamingFixture::SetUpLocalPlayback() {
  transport_ = new LoopBackTransport(voe_network_, channel_);
  EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *transport_));

  webrtc::CodecInst codec;
  codec.channels = 1;
  codec.pacsize = 160;
  codec.plfreq = 8000;
  codec.pltype = 0;
  codec.rate = 64000;
#if defined(_MSC_VER) && defined(_WIN32)
  _snprintf(codec.plname, RTP_PAYLOAD_NAME_SIZE - 1, "PCMU");
#else
  snprintf(codec.plname, RTP_PAYLOAD_NAME_SIZE, "PCMU");
#endif
  voe_codec_->SetSendCodec(channel_, codec);
}
