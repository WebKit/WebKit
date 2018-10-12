/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TwoWayCommunication.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <memory>

#ifdef WIN32
#include <Windows.h>
#endif

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/test/PCMFile.h"
#include "modules/audio_coding/test/utility.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

#define MAX_FILE_NAME_LENGTH_BYTE 500

TwoWayCommunication::TwoWayCommunication()
    : _acmA(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))),
      _acmRefA(AudioCodingModule::Create(
          AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory()))) {
  AudioCodingModule::Config config;
  // The clicks will be more obvious if time-stretching is not allowed.
  // TODO(henrik.lundin) Really?
  config.neteq_config.for_test_no_time_stretching = true;
  config.decoder_factory = CreateBuiltinAudioDecoderFactory();
  _acmB.reset(AudioCodingModule::Create(config));
  _acmRefB.reset(AudioCodingModule::Create(config));
}

TwoWayCommunication::~TwoWayCommunication() {
  delete _channel_A2B;
  delete _channel_B2A;
  delete _channelRef_A2B;
  delete _channelRef_B2A;
  _inFileA.Close();
  _inFileB.Close();
  _outFileA.Close();
  _outFileB.Close();
  _outFileRefA.Close();
  _outFileRefB.Close();
}

void TwoWayCommunication::SetUpAutotest(
    AudioEncoderFactory* const encoder_factory,
    const SdpAudioFormat& format1,
    const int payload_type1,
    const SdpAudioFormat& format2,
    const int payload_type2) {
  //--- Set A codecs
  _acmA->SetEncoder(
      encoder_factory->MakeAudioEncoder(payload_type1, format1, absl::nullopt));
  EXPECT_EQ(true, _acmA->RegisterReceiveCodec(payload_type2, format2));

  //--- Set ref-A codecs
  _acmRefA->SetEncoder(
      encoder_factory->MakeAudioEncoder(payload_type1, format1, absl::nullopt));
  EXPECT_EQ(true, _acmRefA->RegisterReceiveCodec(payload_type2, format2));

  //--- Set B codecs
  _acmB->SetEncoder(
      encoder_factory->MakeAudioEncoder(payload_type2, format2, absl::nullopt));
  EXPECT_EQ(true, _acmB->RegisterReceiveCodec(payload_type1, format1));

  //--- Set ref-B codecs
  _acmRefB->SetEncoder(
      encoder_factory->MakeAudioEncoder(payload_type2, format2, absl::nullopt));
  EXPECT_EQ(true, _acmRefB->RegisterReceiveCodec(payload_type1, format1));

  uint16_t frequencyHz;

  //--- Input A and B
  std::string in_file_name =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  frequencyHz = 16000;
  _inFileA.Open(in_file_name, frequencyHz, "rb");
  _inFileB.Open(in_file_name, frequencyHz, "rb");

  //--- Output A
  std::string output_file_a = webrtc::test::OutputPath() + "outAutotestA.pcm";
  frequencyHz = 16000;
  _outFileA.Open(output_file_a, frequencyHz, "wb");
  std::string output_ref_file_a =
      webrtc::test::OutputPath() + "ref_outAutotestA.pcm";
  _outFileRefA.Open(output_ref_file_a, frequencyHz, "wb");

  //--- Output B
  std::string output_file_b = webrtc::test::OutputPath() + "outAutotestB.pcm";
  frequencyHz = 16000;
  _outFileB.Open(output_file_b, frequencyHz, "wb");
  std::string output_ref_file_b =
      webrtc::test::OutputPath() + "ref_outAutotestB.pcm";
  _outFileRefB.Open(output_ref_file_b, frequencyHz, "wb");

  //--- Set A-to-B channel
  _channel_A2B = new Channel;
  _acmA->RegisterTransportCallback(_channel_A2B);
  _channel_A2B->RegisterReceiverACM(_acmB.get());
  //--- Do the same for the reference
  _channelRef_A2B = new Channel;
  _acmRefA->RegisterTransportCallback(_channelRef_A2B);
  _channelRef_A2B->RegisterReceiverACM(_acmRefB.get());

  //--- Set B-to-A channel
  _channel_B2A = new Channel;
  _acmB->RegisterTransportCallback(_channel_B2A);
  _channel_B2A->RegisterReceiverACM(_acmA.get());
  //--- Do the same for reference
  _channelRef_B2A = new Channel;
  _acmRefB->RegisterTransportCallback(_channelRef_B2A);
  _channelRef_B2A->RegisterReceiverACM(_acmRefA.get());
}

void TwoWayCommunication::Perform() {
  const SdpAudioFormat format1("ISAC", 16000, 1);
  const SdpAudioFormat format2("L16", 8000, 1);
  constexpr int payload_type1 = 17, payload_type2 = 18;

  auto encoder_factory = CreateBuiltinAudioEncoderFactory();

  SetUpAutotest(encoder_factory.get(), format1, payload_type1, format2,
                payload_type2);

  unsigned int msecPassed = 0;
  unsigned int secPassed = 0;

  int32_t outFreqHzA = _outFileA.SamplingFrequency();
  int32_t outFreqHzB = _outFileB.SamplingFrequency();

  AudioFrame audioFrame;

  // In the following loop we tests that the code can handle misuse of the APIs.
  // In the middle of a session with data flowing between two sides, called A
  // and B, APIs will be called, and the code should continue to run, and be
  // able to recover.
  while (!_inFileA.EndOfFile() && !_inFileB.EndOfFile()) {
    msecPassed += 10;
    EXPECT_GT(_inFileA.Read10MsData(audioFrame), 0);
    EXPECT_GE(_acmA->Add10MsData(audioFrame), 0);
    EXPECT_GE(_acmRefA->Add10MsData(audioFrame), 0);

    EXPECT_GT(_inFileB.Read10MsData(audioFrame), 0);

    EXPECT_GE(_acmB->Add10MsData(audioFrame), 0);
    EXPECT_GE(_acmRefB->Add10MsData(audioFrame), 0);
    bool muted;
    EXPECT_EQ(0, _acmA->PlayoutData10Ms(outFreqHzA, &audioFrame, &muted));
    ASSERT_FALSE(muted);
    _outFileA.Write10MsData(audioFrame);
    EXPECT_EQ(0, _acmRefA->PlayoutData10Ms(outFreqHzA, &audioFrame, &muted));
    ASSERT_FALSE(muted);
    _outFileRefA.Write10MsData(audioFrame);
    EXPECT_EQ(0, _acmB->PlayoutData10Ms(outFreqHzB, &audioFrame, &muted));
    ASSERT_FALSE(muted);
    _outFileB.Write10MsData(audioFrame);
    EXPECT_EQ(0, _acmRefB->PlayoutData10Ms(outFreqHzB, &audioFrame, &muted));
    ASSERT_FALSE(muted);
    _outFileRefB.Write10MsData(audioFrame);

    // Update time counters each time a second of data has passed.
    if (msecPassed >= 1000) {
      msecPassed = 0;
      secPassed++;
    }
    // Re-register send codec on side B.
    if (((secPassed % 5) == 4) && (msecPassed >= 990)) {
      _acmB->SetEncoder(encoder_factory->MakeAudioEncoder(
          payload_type2, format2, absl::nullopt));
      EXPECT_TRUE(_acmB->SendCodec());
    }
    // Initialize receiver on side A.
    if (((secPassed % 7) == 6) && (msecPassed == 0))
      EXPECT_EQ(0, _acmA->InitializeReceiver());
    // Re-register codec on side A.
    if (((secPassed % 7) == 6) && (msecPassed >= 990)) {
      EXPECT_EQ(true, _acmA->RegisterReceiveCodec(payload_type2, format2));
    }
  }
}

}  // namespace webrtc
