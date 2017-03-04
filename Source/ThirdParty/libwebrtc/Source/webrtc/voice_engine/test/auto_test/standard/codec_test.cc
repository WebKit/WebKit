/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string>

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

class CodecTest : public AfterStreamingFixture {
 protected:
  void SetUp() {
    memset(&codec_instance_, 0, sizeof(codec_instance_));
  }

  void SetArbitrarySendCodec() {
    // Just grab the first codec.
    EXPECT_EQ(0, voe_codec_->GetCodec(0, codec_instance_));
    EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, codec_instance_));
  }

  webrtc::CodecInst codec_instance_;
};

static void SetRateIfILBC(webrtc::CodecInst* codec_instance, int packet_size) {
  if (!STR_CASE_CMP(codec_instance->plname, "ilbc")) {
    if (packet_size == 160 || packet_size == 320) {
      codec_instance->rate = 15200;
    } else {
      codec_instance->rate = 13300;
    }
  }
}

static bool IsNotViableSendCodec(const char* codec_name) {
  return !STR_CASE_CMP(codec_name, "CN") ||
         !STR_CASE_CMP(codec_name, "telephone-event") ||
         !STR_CASE_CMP(codec_name, "red");
}

TEST_F(CodecTest, PcmuIsDefaultCodecAndHasTheRightValues) {
  EXPECT_EQ(0, voe_codec_->GetSendCodec(channel_, codec_instance_));
  EXPECT_EQ(1u, codec_instance_.channels);
  EXPECT_EQ(160, codec_instance_.pacsize);
  EXPECT_EQ(8000, codec_instance_.plfreq);
  EXPECT_EQ(0, codec_instance_.pltype);
  EXPECT_EQ(64000, codec_instance_.rate);
  EXPECT_STRCASEEQ("PCMU", codec_instance_.plname);
}

TEST_F(CodecTest, VoiceActivityDetectionIsOffByDefault) {
  bool vad_enabled = false;
  bool dtx_disabled = false;
  webrtc::VadModes vad_mode = webrtc::kVadAggressiveMid;

  voe_codec_->GetVADStatus(channel_, vad_enabled, vad_mode, dtx_disabled);

  EXPECT_FALSE(vad_enabled);
  EXPECT_TRUE(dtx_disabled);
  EXPECT_EQ(webrtc::kVadConventional, vad_mode);
}

TEST_F(CodecTest, VoiceActivityDetectionCanBeEnabled) {
  EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, true));

  bool vad_enabled = false;
  bool dtx_disabled = false;
  webrtc::VadModes vad_mode = webrtc::kVadAggressiveMid;

  voe_codec_->GetVADStatus(channel_, vad_enabled, vad_mode, dtx_disabled);

  EXPECT_TRUE(vad_enabled);
  EXPECT_EQ(webrtc::kVadConventional, vad_mode);
  EXPECT_FALSE(dtx_disabled);
}

TEST_F(CodecTest, VoiceActivityDetectionTypeSettingsCanBeChanged) {
  bool vad_enabled = false;
  bool dtx_disabled = false;
  webrtc::VadModes vad_mode = webrtc::kVadAggressiveMid;

  EXPECT_EQ(0, voe_codec_->SetVADStatus(
      channel_, true, webrtc::kVadAggressiveLow, false));
  EXPECT_EQ(0, voe_codec_->GetVADStatus(
      channel_, vad_enabled, vad_mode, dtx_disabled));
  EXPECT_EQ(vad_mode, webrtc::kVadAggressiveLow);
  EXPECT_FALSE(dtx_disabled);

  EXPECT_EQ(0, voe_codec_->SetVADStatus(
      channel_, true, webrtc::kVadAggressiveMid, false));
  EXPECT_EQ(0, voe_codec_->GetVADStatus(
      channel_, vad_enabled, vad_mode, dtx_disabled));
  EXPECT_EQ(vad_mode, webrtc::kVadAggressiveMid);
  EXPECT_FALSE(dtx_disabled);

  // The fourth argument is the DTX disable flag, which is always supposed to
  // be false.
  EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, true,
                                        webrtc::kVadAggressiveHigh, false));
  EXPECT_EQ(0, voe_codec_->GetVADStatus(
      channel_, vad_enabled, vad_mode, dtx_disabled));
  EXPECT_EQ(vad_mode, webrtc::kVadAggressiveHigh);
  EXPECT_FALSE(dtx_disabled);

  EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, true,
                                        webrtc::kVadConventional, false));
  EXPECT_EQ(0, voe_codec_->GetVADStatus(
      channel_, vad_enabled, vad_mode, dtx_disabled));
  EXPECT_EQ(vad_mode, webrtc::kVadConventional);
}

TEST_F(CodecTest, VoiceActivityDetectionCanBeTurnedOff) {
  EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, true));

  // VAD is always on when DTX is on, so we need to turn off DTX too.
  EXPECT_EQ(0, voe_codec_->SetVADStatus(
      channel_, false, webrtc::kVadConventional, true));

  bool vad_enabled = false;
  bool dtx_disabled = false;
  webrtc::VadModes vad_mode = webrtc::kVadAggressiveMid;

  voe_codec_->GetVADStatus(channel_, vad_enabled, vad_mode, dtx_disabled);

  EXPECT_FALSE(vad_enabled);
  EXPECT_TRUE(dtx_disabled);
  EXPECT_EQ(webrtc::kVadConventional, vad_mode);
}

TEST_F(CodecTest, OpusMaxPlaybackRateCanBeSet) {
  for (int i = 0; i < voe_codec_->NumOfCodecs(); ++i) {
    voe_codec_->GetCodec(i, codec_instance_);
    if (STR_CASE_CMP("opus", codec_instance_.plname)) {
      continue;
    }
    voe_codec_->SetSendCodec(channel_, codec_instance_);
    // SetOpusMaxPlaybackRate can handle any integer as the bandwidth. Following
    // tests some most commonly used numbers.
    EXPECT_EQ(0, voe_codec_->SetOpusMaxPlaybackRate(channel_, 48000));
    EXPECT_EQ(0, voe_codec_->SetOpusMaxPlaybackRate(channel_, 32000));
    EXPECT_EQ(0, voe_codec_->SetOpusMaxPlaybackRate(channel_, 16000));
    EXPECT_EQ(0, voe_codec_->SetOpusMaxPlaybackRate(channel_, 8000));
  }
}

TEST_F(CodecTest, OpusDtxCanBeSetForOpus) {
  for (int i = 0; i < voe_codec_->NumOfCodecs(); ++i) {
    voe_codec_->GetCodec(i, codec_instance_);
    if (STR_CASE_CMP("opus", codec_instance_.plname)) {
      continue;
    }
    voe_codec_->SetSendCodec(channel_, codec_instance_);
    EXPECT_EQ(0, voe_codec_->SetOpusDtx(channel_, false));
    EXPECT_EQ(0, voe_codec_->SetOpusDtx(channel_, true));
  }
}

TEST_F(CodecTest, OpusDtxCannotBeSetForNonOpus) {
  for (int i = 0; i < voe_codec_->NumOfCodecs(); ++i) {
    voe_codec_->GetCodec(i, codec_instance_);
    if (!STR_CASE_CMP("opus", codec_instance_.plname)) {
      continue;
    }
    voe_codec_->SetSendCodec(channel_, codec_instance_);
    EXPECT_EQ(-1, voe_codec_->SetOpusDtx(channel_, true));
  }
}

// TODO(xians, phoglund): Re-enable when issue 372 is resolved.
TEST_F(CodecTest, DISABLED_ManualVerifySendCodecsForAllPacketSizes) {
  for (int i = 0; i < voe_codec_->NumOfCodecs(); ++i) {
    voe_codec_->GetCodec(i, codec_instance_);
    if (IsNotViableSendCodec(codec_instance_.plname)) {
      TEST_LOG("Skipping %s.\n", codec_instance_.plname);
      continue;
    }
    EXPECT_NE(-1, codec_instance_.pltype) <<
        "The codec database should suggest a payload type.";

    // Test with default packet size:
    TEST_LOG("%s (pt=%d): default packet size(%d), accepts sizes ",
             codec_instance_.plname, codec_instance_.pltype,
             codec_instance_.pacsize);
    voe_codec_->SetSendCodec(channel_, codec_instance_);
    Sleep(CODEC_TEST_TIME);

    // Now test other reasonable packet sizes:
    bool at_least_one_succeeded = false;
    for (int packet_size = 80; packet_size < 1000; packet_size += 80) {
      SetRateIfILBC(&codec_instance_, packet_size);
      codec_instance_.pacsize = packet_size;

      if (voe_codec_->SetSendCodec(channel_, codec_instance_) != -1) {
        // Note that it's fine for SetSendCodec to fail - what packet sizes
        // it accepts depends on the codec. It should accept one at minimum.
        TEST_LOG("%d ", packet_size);
        TEST_LOG_FLUSH;
        at_least_one_succeeded = true;
        Sleep(CODEC_TEST_TIME);
      }
    }
    TEST_LOG("\n");
    EXPECT_TRUE(at_least_one_succeeded);
  }
}
