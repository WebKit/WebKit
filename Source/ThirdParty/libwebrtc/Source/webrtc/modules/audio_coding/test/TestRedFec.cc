/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/TestRedFec.h"

#include <memory>
#include <utility>

#include "absl/strings/match.h"
#include "api/audio_codecs/L16/audio_decoder_L16.h"
#include "api/audio_codecs/L16/audio_encoder_L16.h"
#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/g711/audio_decoder_g711.h"
#include "api/audio_codecs/g711/audio_encoder_g711.h"
#include "api/audio_codecs/g722/audio_decoder_g722.h"
#include "api/audio_codecs/g722/audio_encoder_g722.h"
#include "api/audio_codecs/isac/audio_decoder_isac_float.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

TestRedFec::TestRedFec()
    : encoder_factory_(CreateAudioEncoderFactory<AudioEncoderG711,
                                                 AudioEncoderG722,
                                                 AudioEncoderIsacFloat,
                                                 AudioEncoderL16,
                                                 AudioEncoderOpus>()),
      decoder_factory_(CreateAudioDecoderFactory<AudioDecoderG711,
                                                 AudioDecoderG722,
                                                 AudioDecoderIsacFloat,
                                                 AudioDecoderL16,
                                                 AudioDecoderOpus>()),
      _acmA(AudioCodingModule::Create(
          AudioCodingModule::Config(decoder_factory_))),
      _acmB(AudioCodingModule::Create(
          AudioCodingModule::Config(decoder_factory_))),
      _channelA2B(NULL),
      _testCntr(0) {}

TestRedFec::~TestRedFec() {
  if (_channelA2B != NULL) {
    delete _channelA2B;
    _channelA2B = NULL;
  }
}

void TestRedFec::Perform() {
  const std::string file_name =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  _inFileA.Open(file_name, 32000, "rb");

  ASSERT_EQ(0, _acmA->InitializeReceiver());
  ASSERT_EQ(0, _acmB->InitializeReceiver());

  // Create and connect the channel
  _channelA2B = new Channel;
  _acmA->RegisterTransportCallback(_channelA2B);
  _channelA2B->RegisterReceiverACM(_acmB.get());

  RegisterSendCodec(_acmA, {"L16", 8000, 1}, Vad::kVadAggressive, true);

  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  // Switch to another 8 kHz codec; RED should remain switched on.
  RegisterSendCodec(_acmA, {"PCMU", 8000, 1}, Vad::kVadAggressive, true);
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  // Switch to a 16 kHz codec; RED should be switched off.
  RegisterSendCodec(_acmA, {"G722", 8000, 1}, Vad::kVadAggressive, false);

  OpenOutFile(_testCntr);
  RegisterSendCodec(_acmA, {"G722", 8000, 1}, Vad::kVadAggressive, false);
  Run();
  RegisterSendCodec(_acmA, {"G722", 8000, 1}, Vad::kVadAggressive, false);
  Run();
  _outFileB.Close();

  RegisterSendCodec(_acmA, {"ISAC", 16000, 1}, Vad::kVadVeryAggressive, false);
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  // Switch to a 32 kHz codec; RED should be switched off.
  RegisterSendCodec(_acmA, {"ISAC", 32000, 1}, Vad::kVadVeryAggressive, false);
  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec(_acmA, {"ISAC", 32000, 1}, absl::nullopt, false);

  _channelA2B->SetFECTestWithPacketLoss(true);
  // Following tests are under packet losses.

  // Switch to a 16 kHz codec; RED should be switched off.
  RegisterSendCodec(_acmA, {"G722", 8000, 1}, Vad::kVadAggressive, false);

  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  // Switch to a 16 kHz codec, RED should have been switched off.
  RegisterSendCodec(_acmA, {"ISAC", 16000, 1}, Vad::kVadVeryAggressive, false);

  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  // Switch to a 32 kHz codec, RED should have been switched off.
  RegisterSendCodec(_acmA, {"ISAC", 32000, 1}, Vad::kVadVeryAggressive, false);

  OpenOutFile(_testCntr);
  Run();
  _outFileB.Close();

  RegisterSendCodec(_acmA, {"ISAC", 32000, 1}, absl::nullopt, false);

  RegisterSendCodec(_acmA, {"opus", 48000, 2}, absl::nullopt, false);

  // _channelA2B imposes 25% packet loss rate.
  EXPECT_EQ(0, _acmA->SetPacketLossRate(25));

  _acmA->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* enc) {
    EXPECT_EQ(true, (*enc)->SetFec(true));
  });

  OpenOutFile(_testCntr);
  Run();

  // Switch to L16 with RED.
  RegisterSendCodec(_acmA, {"L16", 8000, 1}, absl::nullopt, true);
  Run();

  // Switch to Opus again.
  RegisterSendCodec(_acmA, {"opus", 48000, 2}, absl::nullopt, false);
  _acmA->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* enc) {
    EXPECT_EQ(true, (*enc)->SetFec(false));
  });
  Run();

  _acmA->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* enc) {
    EXPECT_EQ(true, (*enc)->SetFec(true));
  });
  _outFileB.Close();
}

void TestRedFec::RegisterSendCodec(
    const std::unique_ptr<AudioCodingModule>& acm,
    const SdpAudioFormat& codec_format,
    absl::optional<Vad::Aggressiveness> vad_mode,
    bool use_red) {
  constexpr int payload_type = 17, cn_payload_type = 27, red_payload_type = 37;
  const auto& other_acm = &acm == &_acmA ? _acmB : _acmA;

  auto encoder = encoder_factory_->MakeAudioEncoder(payload_type, codec_format,
                                                    absl::nullopt);
  EXPECT_NE(encoder, nullptr);
  std::map<int, SdpAudioFormat> receive_codecs = {{payload_type, codec_format}};
  if (!absl::EqualsIgnoreCase(codec_format.name, "opus")) {
    if (vad_mode.has_value()) {
      AudioEncoderCngConfig config;
      config.speech_encoder = std::move(encoder);
      config.num_channels = 1;
      config.payload_type = cn_payload_type;
      config.vad_mode = vad_mode.value();
      encoder = CreateComfortNoiseEncoder(std::move(config));
      receive_codecs.emplace(std::make_pair(
          cn_payload_type, SdpAudioFormat("CN", codec_format.clockrate_hz, 1)));
    }
    if (use_red) {
      AudioEncoderCopyRed::Config config;
      config.payload_type = red_payload_type;
      config.speech_encoder = std::move(encoder);
      encoder = std::make_unique<AudioEncoderCopyRed>(std::move(config),
                                                      field_trials_);
      receive_codecs.emplace(
          std::make_pair(red_payload_type,
                         SdpAudioFormat("red", codec_format.clockrate_hz, 1)));
    }
  }
  acm->SetEncoder(std::move(encoder));
  other_acm->SetReceiveCodecs(receive_codecs);
}

void TestRedFec::Run() {
  AudioFrame audioFrame;
  int32_t outFreqHzB = _outFileB.SamplingFrequency();
  // Set test length to 500 ms (50 blocks of 10 ms each).
  _inFileA.SetNum10MsBlocksToRead(50);
  // Fast-forward 1 second (100 blocks) since the file starts with silence.
  _inFileA.FastForward(100);

  while (!_inFileA.EndOfFile()) {
    EXPECT_GT(_inFileA.Read10MsData(audioFrame), 0);
    EXPECT_GE(_acmA->Add10MsData(audioFrame), 0);
    bool muted;
    EXPECT_EQ(0, _acmB->PlayoutData10Ms(outFreqHzB, &audioFrame, &muted));
    ASSERT_FALSE(muted);
    _outFileB.Write10MsData(audioFrame.data(), audioFrame.samples_per_channel_);
  }
  _inFileA.Rewind();
}

void TestRedFec::OpenOutFile(int16_t test_number) {
  std::string file_name;
  rtc::StringBuilder file_stream;
  file_stream << webrtc::test::OutputPath();
  file_stream << "TestRedFec_outFile_";
  file_stream << test_number << ".pcm";
  file_name = file_stream.str();
  _outFileB.Open(file_name, 16000, "wb");
}

}  // namespace webrtc
