/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fixtures/before_initialization_fixture.h"

#include "webrtc/system_wrappers/include/sleep.h"

BeforeInitializationFixture::BeforeInitializationFixture()
    : voice_engine_(webrtc::VoiceEngine::Create()) {
  EXPECT_TRUE(voice_engine_ != NULL);

  voe_base_ = webrtc::VoEBase::GetInterface(voice_engine_);
  voe_codec_ = webrtc::VoECodec::GetInterface(voice_engine_);
  voe_volume_control_ = webrtc::VoEVolumeControl::GetInterface(voice_engine_);
  voe_rtp_rtcp_ = webrtc::VoERTP_RTCP::GetInterface(voice_engine_);
  voe_apm_ = webrtc::VoEAudioProcessing::GetInterface(voice_engine_);
  voe_network_ = webrtc::VoENetwork::GetInterface(voice_engine_);
  voe_file_ = webrtc::VoEFile::GetInterface(voice_engine_);
  voe_vsync_ = webrtc::VoEVideoSync::GetInterface(voice_engine_);
  voe_hardware_ = webrtc::VoEHardware::GetInterface(voice_engine_);
  voe_xmedia_ = webrtc::VoEExternalMedia::GetInterface(voice_engine_);
  voe_neteq_stats_ = webrtc::VoENetEqStats::GetInterface(voice_engine_);
}

BeforeInitializationFixture::~BeforeInitializationFixture() {
  voe_base_->Release();
  voe_codec_->Release();
  voe_volume_control_->Release();
  voe_rtp_rtcp_->Release();
  voe_apm_->Release();
  voe_network_->Release();
  voe_file_->Release();
  voe_vsync_->Release();
  voe_hardware_->Release();
  voe_xmedia_->Release();
  voe_neteq_stats_->Release();

  EXPECT_TRUE(webrtc::VoiceEngine::Delete(voice_engine_));
}

void BeforeInitializationFixture::Sleep(long milliseconds) {
  webrtc::SleepMs(milliseconds);
}
