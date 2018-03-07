/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kPayloadType = 95;

class SineGenerator : public EncodeNetEqInput::Generator {
 public:
  explicit SineGenerator(int sample_rate_hz)
      : sample_rate_hz_(sample_rate_hz) {}

  rtc::ArrayView<const int16_t> Generate(size_t num_samples) override {
    if (samples_.size() < num_samples) {
      samples_.resize(num_samples);
    }

    rtc::ArrayView<int16_t> output(samples_.data(), num_samples);
    for (auto& x : output) {
      x = static_cast<int16_t>(2000.0 * std::sin(phase_));
      phase_ += 2 * kPi * kFreqHz / sample_rate_hz_;
    }
    return output;
  }

 private:
  static constexpr int kFreqHz = 300;  // The sinewave frequency.
  const int sample_rate_hz_;
  const double kPi = std::acos(-1);
  std::vector<int16_t> samples_;
  double phase_ = 0.0;
};

class FuzzRtpInput : public NetEqInput {
 public:
  explicit FuzzRtpInput(rtc::ArrayView<const uint8_t> data) : data_(data) {
    AudioEncoderPcm16B::Config config;
    config.payload_type = kPayloadType;
    config.sample_rate_hz = 32000;
    std::unique_ptr<AudioEncoder> encoder(new AudioEncoderPcm16B(config));
    std::unique_ptr<EncodeNetEqInput::Generator> generator(
        new SineGenerator(config.sample_rate_hz));
    input_.reset(new EncodeNetEqInput(std::move(generator), std::move(encoder),
                                      std::numeric_limits<int64_t>::max()));
    packet_ = input_->PopPacket();
    FuzzHeader();
  }

  rtc::Optional<int64_t> NextPacketTime() const override {
    return packet_->time_ms;
  }

  rtc::Optional<int64_t> NextOutputEventTime() const override {
    return input_->NextOutputEventTime();
  }

  std::unique_ptr<PacketData> PopPacket() override {
    RTC_DCHECK(packet_);
    std::unique_ptr<PacketData> packet_to_return = std::move(packet_);
    packet_ = input_->PopPacket();
    FuzzHeader();
    return packet_to_return;
  }

  void AdvanceOutputEvent() override { return input_->AdvanceOutputEvent(); }

  bool ended() const override { return ended_; }

  rtc::Optional<RTPHeader> NextHeader() const override {
    RTC_DCHECK(packet_);
    return packet_->header;
  }

 private:
  void FuzzHeader() {
    constexpr size_t kNumBytesToFuzz = 11;
    if (data_ix_ + kNumBytesToFuzz > data_.size()) {
      ended_ = true;
      return;
    }
    RTC_DCHECK(packet_);
    const size_t start_ix = data_ix_;
    packet_->header.payloadType =
        ByteReader<uint8_t>::ReadLittleEndian(&data_[data_ix_]);
    packet_->header.payloadType &= 0x7F;
    data_ix_ += sizeof(uint8_t);
    packet_->header.sequenceNumber =
        ByteReader<uint16_t>::ReadLittleEndian(&data_[data_ix_]);
    data_ix_ += sizeof(uint16_t);
    packet_->header.timestamp =
        ByteReader<uint32_t>::ReadLittleEndian(&data_[data_ix_]);
    data_ix_ += sizeof(uint32_t);
    packet_->header.ssrc =
        ByteReader<uint32_t>::ReadLittleEndian(&data_[data_ix_]);
    data_ix_ += sizeof(uint32_t);
    RTC_CHECK_EQ(data_ix_ - start_ix, kNumBytesToFuzz);
  }

  bool ended_ = false;
  rtc::ArrayView<const uint8_t> data_;
  size_t data_ix_ = 0;
  std::unique_ptr<EncodeNetEqInput> input_;
  std::unique_ptr<PacketData> packet_;
};
}  // namespace

void FuzzOneInputTest(const uint8_t* data, size_t size) {
  std::unique_ptr<FuzzRtpInput> input(
      new FuzzRtpInput(rtc::ArrayView<const uint8_t>(data, size)));
  std::unique_ptr<AudioChecksum> output(new AudioChecksum);
  NetEqTest::Callbacks callbacks;
  NetEq::Config config;
  NetEqTest::DecoderMap codecs;
  codecs[0] = std::make_pair(NetEqDecoder::kDecoderPCMu, "pcmu");
  codecs[8] = std::make_pair(NetEqDecoder::kDecoderPCMa, "pcma");
  codecs[103] = std::make_pair(NetEqDecoder::kDecoderISAC, "isac");
  codecs[104] = std::make_pair(NetEqDecoder::kDecoderISACswb, "isac-swb");
  codecs[111] = std::make_pair(NetEqDecoder::kDecoderOpus, "opus");
  codecs[93] = std::make_pair(NetEqDecoder::kDecoderPCM16B, "pcm16-nb");
  codecs[94] = std::make_pair(NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb");
  codecs[96] =
      std::make_pair(NetEqDecoder::kDecoderPCM16Bswb48kHz, "pcm16-swb48");
  codecs[9] = std::make_pair(NetEqDecoder::kDecoderG722, "g722");
  codecs[106] = std::make_pair(NetEqDecoder::kDecoderAVT, "avt");
  codecs[114] = std::make_pair(NetEqDecoder::kDecoderAVT16kHz, "avt-16");
  codecs[115] = std::make_pair(NetEqDecoder::kDecoderAVT32kHz, "avt-32");
  codecs[116] = std::make_pair(NetEqDecoder::kDecoderAVT48kHz, "avt-48");
  codecs[117] = std::make_pair(NetEqDecoder::kDecoderRED, "red");
  codecs[13] = std::make_pair(NetEqDecoder::kDecoderCNGnb, "cng-nb");
  codecs[98] = std::make_pair(NetEqDecoder::kDecoderCNGwb, "cng-wb");
  codecs[99] = std::make_pair(NetEqDecoder::kDecoderCNGswb32kHz, "cng-swb32");
  codecs[100] = std::make_pair(NetEqDecoder::kDecoderCNGswb48kHz, "cng-swb48");
  // This is the payload type that will be used for encoding.
  codecs[kPayloadType] =
      std::make_pair(NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16-swb32");
  NetEqTest::ExtDecoderMap ext_codecs;

  NetEqTest test(config, codecs, ext_codecs, std::move(input),
                 std::move(output), callbacks);
  test.Run();
}

}  // namespace test

void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzOneInputTest(data, size);
}

}  // namespace webrtc
