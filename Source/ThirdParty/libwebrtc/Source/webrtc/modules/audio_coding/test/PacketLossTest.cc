/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/PacketLossTest.h"

#include <memory>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

ReceiverWithPacketLoss::ReceiverWithPacketLoss()
    : loss_rate_(0),
      burst_length_(1),
      packet_counter_(0),
      lost_packet_counter_(0),
      burst_lost_counter_(burst_length_) {}

void ReceiverWithPacketLoss::Setup(AudioCodingModule* acm,
                                   RTPStream* rtpStream,
                                   std::string out_file_name,
                                   int channels,
                                   int file_num,
                                   int loss_rate,
                                   int burst_length) {
  loss_rate_ = loss_rate;
  burst_length_ = burst_length;
  burst_lost_counter_ = burst_length_;  // To prevent first packet gets lost.
  rtc::StringBuilder ss;
  ss << out_file_name << "_" << loss_rate_ << "_" << burst_length_ << "_";
  Receiver::Setup(acm, rtpStream, ss.str(), channels, file_num);
}

bool ReceiverWithPacketLoss::IncomingPacket() {
  if (!_rtpStream->EndOfFile()) {
    if (packet_counter_ == 0) {
      _realPayloadSizeBytes = _rtpStream->Read(&_rtpHeader, _incomingPayload,
                                               _payloadSizeBytes, &_nextTime);
      if (_realPayloadSizeBytes == 0) {
        if (_rtpStream->EndOfFile()) {
          packet_counter_ = 0;
          return true;
        } else {
          return false;
        }
      }
    }

    if (!PacketLost()) {
      _acm->IncomingPacket(_incomingPayload, _realPayloadSizeBytes, _rtpHeader);
    }
    packet_counter_++;
    _realPayloadSizeBytes = _rtpStream->Read(&_rtpHeader, _incomingPayload,
                                             _payloadSizeBytes, &_nextTime);
    if (_realPayloadSizeBytes == 0 && _rtpStream->EndOfFile()) {
      packet_counter_ = 0;
      lost_packet_counter_ = 0;
    }
  }
  return true;
}

bool ReceiverWithPacketLoss::PacketLost() {
  if (burst_lost_counter_ < burst_length_) {
    lost_packet_counter_++;
    burst_lost_counter_++;
    return true;
  }

  if (lost_packet_counter_ * 100 < loss_rate_ * packet_counter_) {
    lost_packet_counter_++;
    burst_lost_counter_ = 1;
    return true;
  }
  return false;
}

SenderWithFEC::SenderWithFEC() : expected_loss_rate_(0) {}

void SenderWithFEC::Setup(AudioCodingModule* acm,
                          RTPStream* rtpStream,
                          std::string in_file_name,
                          int payload_type,
                          SdpAudioFormat format,
                          int expected_loss_rate) {
  Sender::Setup(acm, rtpStream, in_file_name, format.clockrate_hz, payload_type,
                format);
  EXPECT_TRUE(SetFEC(true));
  EXPECT_TRUE(SetPacketLossRate(expected_loss_rate));
}

bool SenderWithFEC::SetFEC(bool enable_fec) {
  bool success = false;
  _acm->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* enc) {
    if (*enc && (*enc)->SetFec(enable_fec)) {
      success = true;
    }
  });
  return success;
}

bool SenderWithFEC::SetPacketLossRate(int expected_loss_rate) {
  if (_acm->SetPacketLossRate(expected_loss_rate) == 0) {
    expected_loss_rate_ = expected_loss_rate;
    return true;
  }
  return false;
}

PacketLossTest::PacketLossTest(int channels,
                               int expected_loss_rate,
                               int actual_loss_rate,
                               int burst_length)
    : channels_(channels),
      in_file_name_(channels_ == 1 ? "audio_coding/testfile32kHz"
                                   : "audio_coding/teststereo32kHz"),
      sample_rate_hz_(32000),
      expected_loss_rate_(expected_loss_rate),
      actual_loss_rate_(actual_loss_rate),
      burst_length_(burst_length) {}

void PacketLossTest::Perform() {
#ifndef WEBRTC_CODEC_OPUS
  return;
#else
  RTPFile rtpFile;
  std::unique_ptr<AudioCodingModule> acm(AudioCodingModule::Create(
      AudioCodingModule::Config(CreateBuiltinAudioDecoderFactory())));
  SdpAudioFormat send_format = SdpAudioFormat("opus", 48000, 2);
  if (channels_ == 2) {
    send_format.parameters = {{"stereo", "1"}};
  }

  std::string fileName = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                    "packet_loss_test");
  rtpFile.Open(fileName.c_str(), "wb+");
  rtpFile.WriteHeader();
  SenderWithFEC sender;
  sender.Setup(acm.get(), &rtpFile, in_file_name_, 120, send_format,
               expected_loss_rate_);
  sender.Run();
  sender.Teardown();
  rtpFile.Close();

  rtpFile.Open(fileName.c_str(), "rb");
  rtpFile.ReadHeader();
  ReceiverWithPacketLoss receiver;
  receiver.Setup(acm.get(), &rtpFile, "packetLoss_out", channels_, 15,
                 actual_loss_rate_, burst_length_);
  receiver.Run();
  receiver.Teardown();
  rtpFile.Close();
#endif
}

}  // namespace webrtc
