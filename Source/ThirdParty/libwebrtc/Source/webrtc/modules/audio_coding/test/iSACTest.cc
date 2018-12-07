/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/iSACTest.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(WEBRTC_LINUX)
#include <time.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#include "absl/strings/match.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/audio_coding/test/utility.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::StrCaseEq;

namespace {

AudioEncoderIsacFloat::Config MakeConfig(const CodecInst& ci) {
  EXPECT_THAT(ci.plname, StrCaseEq("ISAC"));
  EXPECT_THAT(ci.plfreq, AnyOf(Eq(16000), Eq(32000)));
  EXPECT_THAT(ci.channels, Eq(1u));
  AudioEncoderIsacFloat::Config config;
  config.sample_rate_hz = ci.plfreq;
  EXPECT_THAT(config.IsOk(), Eq(true));
  return config;
}

AudioEncoderIsacFloat::Config TweakConfig(
    AudioEncoderIsacFloat::Config config,
    const ACMTestISACConfig& test_config) {
  if (test_config.currentRateBitPerSec > 0) {
    config.bit_rate = test_config.currentRateBitPerSec;
  }
  if (test_config.currentFrameSizeMsec != 0) {
    config.frame_size_ms = test_config.currentFrameSizeMsec;
  }
  EXPECT_THAT(config.IsOk(), Eq(true));
  return config;
}

void SetISACConfigDefault(ACMTestISACConfig& isacConfig) {
  isacConfig.currentRateBitPerSec = 0;
  isacConfig.currentFrameSizeMsec = 0;
  isacConfig.encodingMode = -1;
  isacConfig.initRateBitPerSec = 0;
  isacConfig.initFrameSizeInMsec = 0;
  isacConfig.enforceFrameSize = false;
}

}  // namespace

ISACTest::ISACTest(int testMode)
    : _acmA(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))),
      _acmB(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))),
      _testMode(testMode) {}

ISACTest::~ISACTest() {}

void ISACTest::Setup() {
  int codecCntr;
  CodecInst codecParam;

  for (codecCntr = 0; codecCntr < AudioCodingModule::NumberOfCodecs();
       codecCntr++) {
    EXPECT_EQ(0, AudioCodingModule::Codec(codecCntr, &codecParam));
    if (absl::EqualsIgnoreCase(codecParam.plname, "ISAC") &&
        codecParam.plfreq == 16000) {
      memcpy(&_paramISAC16kHz, &codecParam, sizeof(CodecInst));
      _idISAC16kHz = codecCntr;
    }
    if (absl::EqualsIgnoreCase(codecParam.plname, "ISAC") &&
        codecParam.plfreq == 32000) {
      memcpy(&_paramISAC32kHz, &codecParam, sizeof(CodecInst));
      _idISAC32kHz = codecCntr;
    }
  }

  // Register both iSAC-wb & iSAC-swb in both sides as receiver codecs.
  EXPECT_EQ(true, _acmA->RegisterReceiveCodec(_paramISAC16kHz.pltype,
                                              CodecInstToSdp(_paramISAC16kHz)));
  EXPECT_EQ(true, _acmA->RegisterReceiveCodec(_paramISAC32kHz.pltype,
                                              CodecInstToSdp(_paramISAC32kHz)));
  EXPECT_EQ(true, _acmB->RegisterReceiveCodec(_paramISAC16kHz.pltype,
                                              CodecInstToSdp(_paramISAC16kHz)));
  EXPECT_EQ(true, _acmB->RegisterReceiveCodec(_paramISAC32kHz.pltype,
                                              CodecInstToSdp(_paramISAC32kHz)));

  //--- Set A-to-B channel
  _channel_A2B.reset(new Channel);
  EXPECT_EQ(0, _acmA->RegisterTransportCallback(_channel_A2B.get()));
  _channel_A2B->RegisterReceiverACM(_acmB.get());

  //--- Set B-to-A channel
  _channel_B2A.reset(new Channel);
  EXPECT_EQ(0, _acmB->RegisterTransportCallback(_channel_B2A.get()));
  _channel_B2A->RegisterReceiverACM(_acmA.get());

  file_name_swb_ =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");

  _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      MakeConfig(_paramISAC16kHz), _paramISAC16kHz.pltype));
  _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      MakeConfig(_paramISAC32kHz), _paramISAC32kHz.pltype));

  _inFileA.Open(file_name_swb_, 32000, "rb");
  // Set test length to 500 ms (50 blocks of 10 ms each).
  _inFileA.SetNum10MsBlocksToRead(50);
  // Fast-forward 1 second (100 blocks) since the files start with silence.
  _inFileA.FastForward(100);
  std::string fileNameA = webrtc::test::OutputPath() + "testisac_a.pcm";
  std::string fileNameB = webrtc::test::OutputPath() + "testisac_b.pcm";
  _outFileA.Open(fileNameA, 32000, "wb");
  _outFileB.Open(fileNameB, 32000, "wb");

  while (!_inFileA.EndOfFile()) {
    Run10ms();
  }
  CodecInst receiveCodec;
  EXPECT_EQ(0, _acmA->ReceiveCodec(&receiveCodec));
  EXPECT_EQ(0, _acmB->ReceiveCodec(&receiveCodec));

  _inFileA.Close();
  _outFileA.Close();
  _outFileB.Close();
}

void ISACTest::Perform() {
  Setup();

  int16_t testNr = 0;
  ACMTestISACConfig wbISACConfig;
  ACMTestISACConfig swbISACConfig;

  SetISACConfigDefault(wbISACConfig);
  SetISACConfigDefault(swbISACConfig);

  wbISACConfig.currentRateBitPerSec = -1;
  swbISACConfig.currentRateBitPerSec = -1;
  testNr++;
  EncodeDecode(testNr, wbISACConfig, swbISACConfig);

  if (_testMode != 0) {
    SetISACConfigDefault(wbISACConfig);
    SetISACConfigDefault(swbISACConfig);

    wbISACConfig.currentRateBitPerSec = -1;
    swbISACConfig.currentRateBitPerSec = -1;
    wbISACConfig.initRateBitPerSec = 13000;
    wbISACConfig.initFrameSizeInMsec = 60;
    swbISACConfig.initRateBitPerSec = 20000;
    swbISACConfig.initFrameSizeInMsec = 30;
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);

    SetISACConfigDefault(wbISACConfig);
    SetISACConfigDefault(swbISACConfig);

    wbISACConfig.currentRateBitPerSec = 20000;
    swbISACConfig.currentRateBitPerSec = 48000;
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);

    wbISACConfig.currentRateBitPerSec = 16000;
    swbISACConfig.currentRateBitPerSec = 30000;
    wbISACConfig.currentFrameSizeMsec = 60;
    testNr++;
    EncodeDecode(testNr, wbISACConfig, swbISACConfig);
  }

  SetISACConfigDefault(wbISACConfig);
  SetISACConfigDefault(swbISACConfig);
  testNr++;
  EncodeDecode(testNr, wbISACConfig, swbISACConfig);

  testNr++;
  if (_testMode == 0) {
    SwitchingSamplingRate(testNr, 4);
  } else {
    SwitchingSamplingRate(testNr, 80);
  }
}

void ISACTest::Run10ms() {
  AudioFrame audioFrame;
  EXPECT_GT(_inFileA.Read10MsData(audioFrame), 0);
  EXPECT_GE(_acmA->Add10MsData(audioFrame), 0);
  EXPECT_GE(_acmB->Add10MsData(audioFrame), 0);
  bool muted;
  EXPECT_EQ(0, _acmA->PlayoutData10Ms(32000, &audioFrame, &muted));
  ASSERT_FALSE(muted);
  _outFileA.Write10MsData(audioFrame);
  EXPECT_EQ(0, _acmB->PlayoutData10Ms(32000, &audioFrame, &muted));
  ASSERT_FALSE(muted);
  _outFileB.Write10MsData(audioFrame);
}

void ISACTest::EncodeDecode(int testNr,
                            ACMTestISACConfig& wbISACConfig,
                            ACMTestISACConfig& swbISACConfig) {
  // Files in Side A and B
  _inFileA.Open(file_name_swb_, 32000, "rb", true);
  _inFileB.Open(file_name_swb_, 32000, "rb", true);

  std::string file_name_out;
  rtc::StringBuilder file_stream_a;
  rtc::StringBuilder file_stream_b;
  file_stream_a << webrtc::test::OutputPath();
  file_stream_b << webrtc::test::OutputPath();
  file_stream_a << "out_iSACTest_A_" << testNr << ".pcm";
  file_stream_b << "out_iSACTest_B_" << testNr << ".pcm";
  file_name_out = file_stream_a.str();
  _outFileA.Open(file_name_out, 32000, "wb");
  file_name_out = file_stream_b.str();
  _outFileB.Open(file_name_out, 32000, "wb");

  // Side A is sending super-wideband, and side B is sending wideband.
  _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      TweakConfig(MakeConfig(_paramISAC32kHz), swbISACConfig),
      _paramISAC32kHz.pltype));
  _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      TweakConfig(MakeConfig(_paramISAC16kHz), wbISACConfig),
      _paramISAC16kHz.pltype));

  bool adaptiveMode = false;
  if ((swbISACConfig.currentRateBitPerSec == -1) ||
      (wbISACConfig.currentRateBitPerSec == -1)) {
    adaptiveMode = true;
  }
  _myTimer.Reset();
  _channel_A2B->ResetStats();
  _channel_B2A->ResetStats();

  char currentTime[500];
  int64_t time_ms = rtc::TimeMillis();
  while (!(_inFileA.EndOfFile() || _inFileA.Rewinded())) {
    Run10ms();
    _myTimer.Tick10ms();
    _myTimer.CurrentTimeHMS(currentTime);

    if ((adaptiveMode) && (_testMode != 0)) {
      time_ms += 10;
      int64_t time_left_ms = time_ms - rtc::TimeMillis();
      if (time_left_ms > 0) {
        SleepMs(time_left_ms);
      }

      EXPECT_TRUE(_acmA->SendCodec());
      EXPECT_TRUE(_acmB->SendCodec());
    }
  }

  if (_testMode != 0) {
    printf("\n\nSide A statistics\n\n");
    _channel_A2B->PrintStats(_paramISAC32kHz);

    printf("\n\nSide B statistics\n\n");
    _channel_B2A->PrintStats(_paramISAC16kHz);
  }

  _channel_A2B->ResetStats();
  _channel_B2A->ResetStats();

  _outFileA.Close();
  _outFileB.Close();
  _inFileA.Close();
  _inFileB.Close();
}

void ISACTest::SwitchingSamplingRate(int testNr, int maxSampRateChange) {
  // Files in Side A
  _inFileA.Open(file_name_swb_, 32000, "rb");
  _inFileB.Open(file_name_swb_, 32000, "rb");

  std::string file_name_out;
  rtc::StringBuilder file_stream_a;
  rtc::StringBuilder file_stream_b;
  file_stream_a << webrtc::test::OutputPath();
  file_stream_b << webrtc::test::OutputPath();
  file_stream_a << "out_iSACTest_A_" << testNr << ".pcm";
  file_stream_b << "out_iSACTest_B_" << testNr << ".pcm";
  file_name_out = file_stream_a.str();
  _outFileA.Open(file_name_out, 32000, "wb");
  file_name_out = file_stream_b.str();
  _outFileB.Open(file_name_out, 32000, "wb");

  // Start with side A sending super-wideband and side B seding wideband.
  // Toggle sending wideband/super-wideband in this test.
  _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      MakeConfig(_paramISAC32kHz), _paramISAC32kHz.pltype));
  _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      MakeConfig(_paramISAC16kHz), _paramISAC16kHz.pltype));

  int numSendCodecChanged = 0;
  _myTimer.Reset();
  char currentTime[50];
  while (numSendCodecChanged < (maxSampRateChange << 1)) {
    Run10ms();
    _myTimer.Tick10ms();
    _myTimer.CurrentTimeHMS(currentTime);
    if (_testMode == 2)
      printf("\r%s", currentTime);
    if (_inFileA.EndOfFile()) {
      if (_inFileA.SamplingFrequency() == 16000) {
        // Switch side A to send super-wideband.
        _inFileA.Close();
        _inFileA.Open(file_name_swb_, 32000, "rb");
        _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            MakeConfig(_paramISAC32kHz), _paramISAC32kHz.pltype));
      } else {
        // Switch side A to send wideband.
        _inFileA.Close();
        _inFileA.Open(file_name_swb_, 32000, "rb");
        _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            MakeConfig(_paramISAC16kHz), _paramISAC16kHz.pltype));
      }
      numSendCodecChanged++;
    }

    if (_inFileB.EndOfFile()) {
      if (_inFileB.SamplingFrequency() == 16000) {
        // Switch side B to send super-wideband.
        _inFileB.Close();
        _inFileB.Open(file_name_swb_, 32000, "rb");
        _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            MakeConfig(_paramISAC32kHz), _paramISAC32kHz.pltype));
      } else {
        // Switch side B to send wideband.
        _inFileB.Close();
        _inFileB.Open(file_name_swb_, 32000, "rb");
        _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            MakeConfig(_paramISAC16kHz), _paramISAC16kHz.pltype));
      }
      numSendCodecChanged++;
    }
  }
  _outFileA.Close();
  _outFileB.Close();
  _inFileA.Close();
  _inFileB.Close();
}

}  // namespace webrtc
