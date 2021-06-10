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

#include "absl/strings/match.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::StrCaseEq;

namespace {

constexpr int kISAC16kPayloadType = 103;
constexpr int kISAC32kPayloadType = 104;
const SdpAudioFormat kISAC16kFormat = {"ISAC", 16000, 1};
const SdpAudioFormat kISAC32kFormat = {"ISAC", 32000, 1};

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

ISACTest::ACMTestTimer::ACMTestTimer() : _msec(0), _sec(0), _min(0), _hour(0) {
  return;
}

ISACTest::ACMTestTimer::~ACMTestTimer() {
  return;
}

void ISACTest::ACMTestTimer::Reset() {
  _msec = 0;
  _sec = 0;
  _min = 0;
  _hour = 0;
  return;
}
void ISACTest::ACMTestTimer::Tick10ms() {
  _msec += 10;
  Adjust();
  return;
}

void ISACTest::ACMTestTimer::Tick1ms() {
  _msec++;
  Adjust();
  return;
}

void ISACTest::ACMTestTimer::Tick100ms() {
  _msec += 100;
  Adjust();
  return;
}

void ISACTest::ACMTestTimer::Tick1sec() {
  _sec++;
  Adjust();
  return;
}

void ISACTest::ACMTestTimer::CurrentTimeHMS(char* currTime) {
  sprintf(currTime, "%4lu:%02u:%06.3f", _hour, _min,
          (double)_sec + (double)_msec / 1000.);
  return;
}

void ISACTest::ACMTestTimer::CurrentTime(unsigned long& h,
                                         unsigned char& m,
                                         unsigned char& s,
                                         unsigned short& ms) {
  h = _hour;
  m = _min;
  s = _sec;
  ms = _msec;
  return;
}

void ISACTest::ACMTestTimer::Adjust() {
  unsigned int n;
  if (_msec >= 1000) {
    n = _msec / 1000;
    _msec -= (1000 * n);
    _sec += n;
  }
  if (_sec >= 60) {
    n = _sec / 60;
    _sec -= (n * 60);
    _min += n;
  }
  if (_min >= 60) {
    n = _min / 60;
    _min -= (n * 60);
    _hour += n;
  }
}

ISACTest::ISACTest()
    : _acmA(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))),
      _acmB(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))) {}

ISACTest::~ISACTest() {}

void ISACTest::Setup() {
  // Register both iSAC-wb & iSAC-swb in both sides as receiver codecs.
  std::map<int, SdpAudioFormat> receive_codecs = {
      {kISAC16kPayloadType, kISAC16kFormat},
      {kISAC32kPayloadType, kISAC32kFormat}};
  _acmA->SetReceiveCodecs(receive_codecs);
  _acmB->SetReceiveCodecs(receive_codecs);

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
      *AudioEncoderIsacFloat::SdpToConfig(kISAC16kFormat),
      kISAC16kPayloadType));
  _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      *AudioEncoderIsacFloat::SdpToConfig(kISAC32kFormat),
      kISAC32kPayloadType));

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

  SetISACConfigDefault(wbISACConfig);
  SetISACConfigDefault(swbISACConfig);
  testNr++;
  EncodeDecode(testNr, wbISACConfig, swbISACConfig);

  testNr++;
  SwitchingSamplingRate(testNr, 4);
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
      TweakConfig(*AudioEncoderIsacFloat::SdpToConfig(kISAC32kFormat),
                  swbISACConfig),
      kISAC32kPayloadType));
  _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      TweakConfig(*AudioEncoderIsacFloat::SdpToConfig(kISAC16kFormat),
                  wbISACConfig),
      kISAC16kPayloadType));

  bool adaptiveMode = false;
  if ((swbISACConfig.currentRateBitPerSec == -1) ||
      (wbISACConfig.currentRateBitPerSec == -1)) {
    adaptiveMode = true;
  }
  _myTimer.Reset();
  _channel_A2B->ResetStats();
  _channel_B2A->ResetStats();

  char currentTime[500];
  while (!(_inFileA.EndOfFile() || _inFileA.Rewinded())) {
    Run10ms();
    _myTimer.Tick10ms();
    _myTimer.CurrentTimeHMS(currentTime);
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
      *AudioEncoderIsacFloat::SdpToConfig(kISAC32kFormat),
      kISAC32kPayloadType));
  _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
      *AudioEncoderIsacFloat::SdpToConfig(kISAC16kFormat),
      kISAC16kPayloadType));

  int numSendCodecChanged = 0;
  _myTimer.Reset();
  char currentTime[50];
  while (numSendCodecChanged < (maxSampRateChange << 1)) {
    Run10ms();
    _myTimer.Tick10ms();
    _myTimer.CurrentTimeHMS(currentTime);
    if (_inFileA.EndOfFile()) {
      if (_inFileA.SamplingFrequency() == 16000) {
        // Switch side A to send super-wideband.
        _inFileA.Close();
        _inFileA.Open(file_name_swb_, 32000, "rb");
        _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            *AudioEncoderIsacFloat::SdpToConfig(kISAC32kFormat),
            kISAC32kPayloadType));
      } else {
        // Switch side A to send wideband.
        _inFileA.Close();
        _inFileA.Open(file_name_swb_, 32000, "rb");
        _acmA->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            *AudioEncoderIsacFloat::SdpToConfig(kISAC16kFormat),
            kISAC16kPayloadType));
      }
      numSendCodecChanged++;
    }

    if (_inFileB.EndOfFile()) {
      if (_inFileB.SamplingFrequency() == 16000) {
        // Switch side B to send super-wideband.
        _inFileB.Close();
        _inFileB.Open(file_name_swb_, 32000, "rb");
        _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            *AudioEncoderIsacFloat::SdpToConfig(kISAC32kFormat),
            kISAC32kPayloadType));
      } else {
        // Switch side B to send wideband.
        _inFileB.Close();
        _inFileB.Open(file_name_swb_, 32000, "rb");
        _acmB->SetEncoder(AudioEncoderIsacFloat::MakeAudioEncoder(
            *AudioEncoderIsacFloat::SdpToConfig(kISAC16kFormat),
            kISAC16kPayloadType));
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
