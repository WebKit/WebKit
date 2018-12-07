/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/EncodeDecodeTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include "absl/strings/match.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/utility.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

TestPacketization::TestPacketization(RTPStream *rtpStream, uint16_t frequency)
    : _rtpStream(rtpStream),
      _frequency(frequency),
      _seqNo(0) {
}

TestPacketization::~TestPacketization() {
}

int32_t TestPacketization::SendData(
    const FrameType /* frameType */, const uint8_t payloadType,
    const uint32_t timeStamp, const uint8_t* payloadData,
    const size_t payloadSize,
    const RTPFragmentationHeader* /* fragmentation */) {
  _rtpStream->Write(payloadType, timeStamp, _seqNo++, payloadData, payloadSize,
                    _frequency);
  return 1;
}

Sender::Sender()
    : _acm(NULL),
      _pcmFile(),
      _audioFrame(),
      _packetization(NULL) {
}

void Sender::Setup(AudioCodingModule *acm, RTPStream *rtpStream,
                   std::string in_file_name, int sample_rate, size_t channels) {
  struct CodecInst sendCodec;
  int codecNo;

  // Open input file
  const std::string file_name = webrtc::test::ResourcePath(in_file_name, "pcm");
  _pcmFile.Open(file_name, sample_rate, "rb");
  if (channels == 2) {
    _pcmFile.ReadStereo(true);
  }
  // Set test length to 500 ms (50 blocks of 10 ms each).
  _pcmFile.SetNum10MsBlocksToRead(50);
  // Fast-forward 1 second (100 blocks) since the file starts with silence.
  _pcmFile.FastForward(100);

  // Set the codec for the current test.
  codecNo = codeId;

  EXPECT_EQ(0, acm->Codec(codecNo, &sendCodec));

  sendCodec.channels = channels;

  acm->SetEncoder(CreateBuiltinAudioEncoderFactory()->MakeAudioEncoder(
      sendCodec.pltype, CodecInstToSdp(sendCodec), absl::nullopt));
  _packetization = new TestPacketization(rtpStream, sendCodec.plfreq);
  EXPECT_EQ(0, acm->RegisterTransportCallback(_packetization));

  _acm = acm;
}

void Sender::Teardown() {
  _pcmFile.Close();
  delete _packetization;
}

bool Sender::Add10MsData() {
  if (!_pcmFile.EndOfFile()) {
    EXPECT_GT(_pcmFile.Read10MsData(_audioFrame), 0);
    int32_t ok = _acm->Add10MsData(_audioFrame);
    EXPECT_GE(ok, 0);
    return ok >= 0 ? true : false;
  }
  return false;
}

void Sender::Run() {
  while (true) {
    if (!Add10MsData()) {
      break;
    }
  }
}

Receiver::Receiver()
    : _playoutLengthSmpls(WEBRTC_10MS_PCM_AUDIO),
      _payloadSizeBytes(MAX_INCOMING_PAYLOAD) {
}

void Receiver::Setup(AudioCodingModule *acm, RTPStream *rtpStream,
                     std::string out_file_name, size_t channels) {
  struct CodecInst recvCodec = CodecInst();
  int noOfCodecs;
  EXPECT_EQ(0, acm->InitializeReceiver());

  noOfCodecs = acm->NumberOfCodecs();
  for (int i = 0; i < noOfCodecs; i++) {
    EXPECT_EQ(0, acm->Codec(i, &recvCodec));
    if (recvCodec.channels == channels)
      EXPECT_EQ(true, acm->RegisterReceiveCodec(recvCodec.pltype,
                                                CodecInstToSdp(recvCodec)));
    // Forces mono/stereo for Opus.
    if (!strcmp(recvCodec.plname, "opus")) {
      recvCodec.channels = channels;
      EXPECT_EQ(true, acm->RegisterReceiveCodec(recvCodec.pltype,
                                                CodecInstToSdp(recvCodec)));
    }
  }

  int playSampFreq;
  std::string file_name;
  rtc::StringBuilder file_stream;
  file_stream << webrtc::test::OutputPath() << out_file_name
      << static_cast<int>(codeId) << ".pcm";
  file_name = file_stream.str();
  _rtpStream = rtpStream;

  playSampFreq = 32000;
  _pcmFile.Open(file_name, 32000, "wb+");

  _realPayloadSizeBytes = 0;
  _playoutBuffer = new int16_t[WEBRTC_10MS_PCM_AUDIO];
  _frequency = playSampFreq;
  _acm = acm;
  _firstTime = true;
}

void Receiver::Teardown() {
  delete[] _playoutBuffer;
  _pcmFile.Close();
}

bool Receiver::IncomingPacket() {
  if (!_rtpStream->EndOfFile()) {
    if (_firstTime) {
      _firstTime = false;
      _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload,
                                               _payloadSizeBytes, &_nextTime);
      if (_realPayloadSizeBytes == 0) {
        if (_rtpStream->EndOfFile()) {
          _firstTime = true;
          return true;
        } else {
          return false;
        }
      }
    }

    EXPECT_EQ(0, _acm->IncomingPacket(_incomingPayload, _realPayloadSizeBytes,
                                      _rtpInfo));
    _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload,
                                             _payloadSizeBytes, &_nextTime);
    if (_realPayloadSizeBytes == 0 && _rtpStream->EndOfFile()) {
      _firstTime = true;
    }
  }
  return true;
}

bool Receiver::PlayoutData() {
  AudioFrame audioFrame;
  bool muted;
  int32_t ok = _acm->PlayoutData10Ms(_frequency, &audioFrame, &muted);
  if (muted) {
    ADD_FAILURE();
    return false;
  }
  EXPECT_EQ(0, ok);
  if (ok < 0){
    return false;
  }
  if (_playoutLengthSmpls == 0) {
    return false;
  }
  _pcmFile.Write10MsData(audioFrame.data(),
      audioFrame.samples_per_channel_ * audioFrame.num_channels_);
  return true;
}

void Receiver::Run() {
  uint8_t counter500Ms = 50;
  uint32_t clock = 0;

  while (counter500Ms > 0) {
    if (clock == 0 || clock >= _nextTime) {
      EXPECT_TRUE(IncomingPacket());
      if (clock == 0) {
        clock = _nextTime;
      }
    }
    if ((clock % 10) == 0) {
      if (!PlayoutData()) {
        clock++;
        continue;
      }
    }
    if (_rtpStream->EndOfFile()) {
      counter500Ms--;
    }
    clock++;
  }
}

EncodeDecodeTest::EncodeDecodeTest(int test_mode) {
  // There used to be different test modes. The only one still supported is the
  // "autotest" mode.
  RTC_CHECK_EQ(0, test_mode);
}

void EncodeDecodeTest::Perform() {
  int numCodecs = 1;
  int codePars[3];  // Frequency, packet size, rate.
  int numPars[52];  // Number of codec parameters sets (freq, pacsize, rate)
                    // to test, for a given codec.

  codePars[0] = 0;
  codePars[1] = 0;
  codePars[2] = 0;

  std::unique_ptr<AudioCodingModule> acm(AudioCodingModule::Create(
      AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory())));
  struct CodecInst sendCodecTmp;
  numCodecs = acm->NumberOfCodecs();

  for (int n = 0; n < numCodecs; n++) {
    EXPECT_EQ(0, acm->Codec(n, &sendCodecTmp));
    if (absl::EqualsIgnoreCase(sendCodecTmp.plname, "telephone-event")) {
      numPars[n] = 0;
    } else if (absl::EqualsIgnoreCase(sendCodecTmp.plname, "cn")) {
      numPars[n] = 0;
    } else if (absl::EqualsIgnoreCase(sendCodecTmp.plname, "red")) {
      numPars[n] = 0;
    } else if (sendCodecTmp.channels == 2) {
      numPars[n] = 0;
    } else {
      numPars[n] = 1;
    }
  }

  // Loop over all mono codecs:
  for (int codeId = 0; codeId < numCodecs; codeId++) {
    // Only encode using real mono encoders, not telephone-event and cng.
    for (int loopPars = 1; loopPars <= numPars[codeId]; loopPars++) {
      // Encode all data to file.
      std::string fileName = EncodeToFile(1, codeId, codePars);

      RTPFile rtpFile;
      rtpFile.Open(fileName.c_str(), "rb");

      _receiver.codeId = codeId;

      rtpFile.ReadHeader();
      _receiver.Setup(acm.get(), &rtpFile, "encodeDecode_out", 1);
      _receiver.Run();
      _receiver.Teardown();
      rtpFile.Close();
    }
  }
}

std::string EncodeDecodeTest::EncodeToFile(int fileType,
                                           int codeId,
                                           int* codePars) {
  std::unique_ptr<AudioCodingModule> acm(AudioCodingModule::Create(
      AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory())));
  RTPFile rtpFile;
  std::string fileName = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                    "encode_decode_rtp");
  rtpFile.Open(fileName.c_str(), "wb+");
  rtpFile.WriteHeader();

  // Store for auto_test and logging.
  _sender.codeId = codeId;

  _sender.Setup(acm.get(), &rtpFile, "audio_coding/testfile32kHz", 32000, 1);
  if (acm->SendCodec()) {
    _sender.Run();
  }
  _sender.Teardown();
  rtpFile.Close();

  return fileName;
}

}  // namespace webrtc
