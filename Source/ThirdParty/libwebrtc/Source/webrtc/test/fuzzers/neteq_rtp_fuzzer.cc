/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/neteq/neteq.h"
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kPayloadType = 95;

class SineGenerator : public EncodeNetEqInput::Generator {
 public:
  explicit SineGenerator(int sample_rate_hz)
      : sample_rate_hz_(sample_rate_hz) {}

  webrtc::ArrayView<const int16_t> Generate(size_t num_samples) override {
    if (samples_.size() < num_samples) {
      samples_.resize(num_samples);
    }

    webrtc::ArrayView<int16_t> output(samples_.data(), num_samples);
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
  explicit FuzzRtpInput(webrtc::ArrayView<const uint8_t> data) : data_(data) {
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
    MaybeFuzzPayload();
  }

  std::optional<int64_t> NextPacketTime() const override {
    return packet_->arrival_time().ms();
  }

  std::optional<int64_t> NextOutputEventTime() const override {
    return input_->NextOutputEventTime();
  }

  std::optional<SetMinimumDelayInfo> NextSetMinimumDelayInfo() const override {
    return input_->NextSetMinimumDelayInfo();
  }

  std::unique_ptr<RtpPacketReceived> PopPacket() override {
    RTC_DCHECK(packet_);
    std::unique_ptr<RtpPacketReceived> packet_to_return = std::move(packet_);
    packet_ = input_->PopPacket();
    FuzzHeader();
    MaybeFuzzPayload();
    return packet_to_return;
  }

  void AdvanceOutputEvent() override { return input_->AdvanceOutputEvent(); }

  void AdvanceSetMinimumDelay() override {
    return input_->AdvanceSetMinimumDelay();
  }

  bool ended() const override { return ended_; }

  const RtpPacketReceived* NextPacket() const override {
    RTC_DCHECK(packet_);
    return packet_.get();
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
    packet_->SetPayloadType(
        ByteReader<uint8_t>::ReadLittleEndian(&data_[data_ix_]) & 0x7F);
    data_ix_ += sizeof(uint8_t);
    packet_->SetSequenceNumber(
        ByteReader<uint16_t>::ReadLittleEndian(&data_[data_ix_]));
    data_ix_ += sizeof(uint16_t);
    packet_->SetTimestamp(
        ByteReader<uint32_t>::ReadLittleEndian(&data_[data_ix_]));
    data_ix_ += sizeof(uint32_t);
    packet_->SetSsrc(ByteReader<uint32_t>::ReadLittleEndian(&data_[data_ix_]));
    data_ix_ += sizeof(uint32_t);
    RTC_CHECK_EQ(data_ix_ - start_ix, kNumBytesToFuzz);
  }

  void MaybeFuzzPayload() {
    // Read one byte of fuzz data to determine how many payload bytes to fuzz.
    if (data_ix_ + 1 > data_.size()) {
      ended_ = true;
      return;
    }
    size_t bytes_to_fuzz = data_[data_ix_++];

    // Restrict number of bytes to fuzz to 16; a reasonably low number enough to
    // cover a few RED headers. Also don't write outside the payload length.
    bytes_to_fuzz = std::min(bytes_to_fuzz % 16, packet_->payload_size());

    if (bytes_to_fuzz == 0)
      return;

    if (data_ix_ + bytes_to_fuzz > data_.size()) {
      ended_ = true;
      return;
    }

    // Call `SetPayloadSize` to get access to the writable RTP payload without
    // changing its size.
    uint8_t* payload = packet_->SetPayloadSize(packet_->payload_size());
    std::memcpy(payload, &data_[data_ix_], bytes_to_fuzz);
    data_ix_ += bytes_to_fuzz;
  }

  bool ended_ = false;
  webrtc::ArrayView<const uint8_t> data_;
  size_t data_ix_ = 0;
  std::unique_ptr<EncodeNetEqInput> input_;
  std::unique_ptr<RtpPacketReceived> packet_;
};
}  // namespace

void FuzzOneInputTest(const uint8_t* data, size_t size) {
  std::unique_ptr<FuzzRtpInput> input(
      new FuzzRtpInput(webrtc::ArrayView<const uint8_t>(data, size)));
  std::unique_ptr<AudioChecksum> output(new AudioChecksum);
  NetEqTest::Callbacks callbacks;
  NetEq::Config config;
  auto codecs = NetEqTest::StandardDecoderMap();
  // kPayloadType is the payload type that will be used for encoding. Verify
  // that it is included in the standard decoder map, and that it points to the
  // expected decoder type.
  const auto it = codecs.find(kPayloadType);
  RTC_CHECK(it != codecs.end());
  RTC_CHECK(it->second == SdpAudioFormat("L16", 32000, 1));

  NetEqTest test(config, CreateBuiltinAudioDecoderFactory(), codecs,
                 /*text_log=*/nullptr, /*neteq_factory=*/nullptr,
                 std::move(input), std::move(output), callbacks);
  test.Run();
}

}  // namespace test

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 70000) {
    return;
  }
  test::FuzzOneInputTest(data, size);
}

}  // namespace webrtc
