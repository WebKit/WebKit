/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/voice_engine/include/voe_codec.h"

#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {
namespace {

TEST(VoECodecInst, TestCompareCodecInstances) {
  CodecInst codec1, codec2;
  memset(&codec1, 0, sizeof(CodecInst));
  memset(&codec2, 0, sizeof(CodecInst));

  codec1.pltype = 101;
  strncpy(codec1.plname, "isac", 4);
  codec1.plfreq = 8000;
  codec1.pacsize = 110;
  codec1.channels = 1;
  codec1.rate = 8000;
  memcpy(&codec2, &codec1, sizeof(CodecInst));
  // Compare two codecs now.
  EXPECT_TRUE(codec1 == codec2);
  EXPECT_FALSE(codec1 != codec2);

  // Changing pltype.
  codec2.pltype = 102;
  EXPECT_FALSE(codec1 == codec2);
  EXPECT_TRUE(codec1 != codec2);

  // Reset to codec2 to codec1 state.
  memcpy(&codec2, &codec1, sizeof(CodecInst));
  // payload name should be case insensitive.
  strncpy(codec2.plname, "ISAC", 4);
  EXPECT_TRUE(codec1 == codec2);

  // Test modifying the |plfreq|
  codec2.plfreq = 16000;
  EXPECT_FALSE(codec1 == codec2);

  // Reset to codec2 to codec1 state.
  memcpy(&codec2, &codec1, sizeof(CodecInst));
  // Test modifying the |pacsize|.
  codec2.pacsize = 440;
  EXPECT_FALSE(codec1 == codec2);

  // Reset to codec2 to codec1 state.
  memcpy(&codec2, &codec1, sizeof(CodecInst));
  // Test modifying the |channels|.
  codec2.channels = 2;
  EXPECT_FALSE(codec1 == codec2);

  // Reset to codec2 to codec1 state.
  memcpy(&codec2, &codec1, sizeof(CodecInst));
  // Test modifying the |rate|.
  codec2.rate = 0;
  EXPECT_FALSE(codec1 == codec2);
}

// This is a regression test for
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6020
// The Opus DTX setting was being forgotten after unrelated VoE calls.
TEST(VoECodecInst, RememberOpusDtxAfterSettingChange) {
  VoiceEngine* voe(VoiceEngine::Create());
  VoEBase* base(VoEBase::GetInterface(voe));
  VoECodec* voe_codec(VoECodec::GetInterface(voe));
  std::unique_ptr<FakeAudioDeviceModule> adm(new FakeAudioDeviceModule);

  base->Init(adm.get());

  CodecInst codec = {111, "opus", 48000, 960, 1, 32000};

  int channel = base->CreateChannel();

  bool DTX = false;

  EXPECT_EQ(0, voe_codec->SetSendCodec(channel, codec));
  EXPECT_EQ(0, voe_codec->SetOpusDtx(channel, true));
  EXPECT_EQ(0, voe_codec->SetFECStatus(channel, true));
  EXPECT_EQ(0, voe_codec->GetOpusDtxStatus(channel, &DTX));
  EXPECT_TRUE(DTX);

  base->DeleteChannel(channel);
  base->Terminate();
  base->Release();
  voe_codec->Release();
  VoiceEngine::Delete(voe);
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
