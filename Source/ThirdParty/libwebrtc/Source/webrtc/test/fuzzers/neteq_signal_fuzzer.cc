/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/random.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace test {
namespace {
// Generate a mixture of sine wave and gaussian noise.
class SineAndNoiseGenerator : public EncodeNetEqInput::Generator {
 public:
  // The noise generator is seeded with a value from the fuzzer data, but 0 is
  // avoided (since it is not allowed by the Random class).
  SineAndNoiseGenerator(int sample_rate_hz, FuzzDataHelper* fuzz_data)
      : sample_rate_hz_(sample_rate_hz),
        fuzz_data_(*fuzz_data),
        noise_generator_(fuzz_data_.ReadOrDefaultValueNotZero<uint64_t>(1)) {}

  // Generates num_samples of the sine-gaussian mixture.
  rtc::ArrayView<const int16_t> Generate(size_t num_samples) override {
    if (samples_.size() < num_samples) {
      samples_.resize(num_samples);
    }

    rtc::ArrayView<int16_t> output(samples_.data(), num_samples);
    // Randomize an amplitude between 0 and 32768; use 65000/2 if we are out of
    // fuzzer data.
    const float amplitude = fuzz_data_.ReadOrDefaultValue<uint16_t>(65000) / 2;
    // Randomize a noise standard deviation between 0 and 1999.
    const float noise_std = fuzz_data_.ReadOrDefaultValue<uint16_t>(0) % 2000;
    for (auto& x : output) {
      x = rtc::saturated_cast<int16_t>(amplitude * std::sin(phase_) +
                                       noise_generator_.Gaussian(0, noise_std));
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
  FuzzDataHelper& fuzz_data_;
  Random noise_generator_;
};

class FuzzSignalInput : public NetEqInput {
 public:
  explicit FuzzSignalInput(FuzzDataHelper* fuzz_data,
                           int sample_rate,
                           uint8_t payload_type)
      : fuzz_data_(*fuzz_data) {
    AudioEncoderPcm16B::Config config;
    config.payload_type = payload_type;
    config.sample_rate_hz = sample_rate;
    std::unique_ptr<AudioEncoder> encoder(new AudioEncoderPcm16B(config));
    std::unique_ptr<EncodeNetEqInput::Generator> generator(
        new SineAndNoiseGenerator(config.sample_rate_hz, fuzz_data));
    input_.reset(new EncodeNetEqInput(std::move(generator), std::move(encoder),
                                      std::numeric_limits<int64_t>::max()));
    packet_ = input_->PopPacket();

    // Select an output event period. This is how long time we wait between each
    // call to NetEq::GetAudio. 10 ms is nominal, 9 and 11 ms will both lead to
    // clock drift (in different directions).
    constexpr int output_event_periods[] = {9, 10, 11};
    output_event_period_ms_ = fuzz_data_.SelectOneOf(output_event_periods);
  }

  absl::optional<int64_t> NextPacketTime() const override {
    return packet_->time_ms;
  }

  absl::optional<int64_t> NextOutputEventTime() const override {
    return next_output_event_ms_;
  }

  std::unique_ptr<PacketData> PopPacket() override {
    RTC_DCHECK(packet_);
    std::unique_ptr<PacketData> packet_to_return = std::move(packet_);
    do {
      packet_ = input_->PopPacket();
      // If the next value from the fuzzer input is 0, the packet is discarded
      // and the next one is pulled from the source.
    } while (fuzz_data_.CanReadBytes(1) && fuzz_data_.Read<uint8_t>() == 0);
    if (fuzz_data_.CanReadBytes(1)) {
      // Generate jitter by setting an offset for the arrival time.
      const int8_t arrival_time_offset_ms = fuzz_data_.Read<int8_t>();
      // The arrival time can not be before the previous packets.
      packet_->time_ms = std::max(packet_to_return->time_ms,
                                  packet_->time_ms + arrival_time_offset_ms);
    } else {
      // Mark that we are at the end of the test. However, the current packet is
      // still valid (but it may not have been fuzzed as expected).
      ended_ = true;
    }
    return packet_to_return;
  }

  void AdvanceOutputEvent() override {
    next_output_event_ms_ += output_event_period_ms_;
  }

  bool ended() const override { return ended_; }

  absl::optional<RTPHeader> NextHeader() const override {
    RTC_DCHECK(packet_);
    return packet_->header;
  }

 private:
  bool ended_ = false;
  FuzzDataHelper& fuzz_data_;
  std::unique_ptr<EncodeNetEqInput> input_;
  std::unique_ptr<PacketData> packet_;
  int64_t next_output_event_ms_ = 0;
  int64_t output_event_period_ms_ = 10;
};

template <class T>
bool MapHas(const std::map<int, T>& m, int key, const T& value) {
  const auto it = m.find(key);
  return (it != m.end() && it->second == value);
}

}  // namespace

void FuzzOneInputTest(const uint8_t* data, size_t size) {
  if (size < 1 || size > 65000) {
    return;
  }

  FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));

  // Allowed sample rates and payload types used in the test.
  std::pair<int, uint8_t> rate_types[] = {
      {8000, 93}, {16000, 94}, {32000, 95}, {48000, 96}};
  const auto rate_type = fuzz_data.SelectOneOf(rate_types);
  const int sample_rate = rate_type.first;
  const uint8_t payload_type = rate_type.second;

  // Set up the input signal generator.
  std::unique_ptr<FuzzSignalInput> input(
      new FuzzSignalInput(&fuzz_data, sample_rate, payload_type));

  // Output sink for the test.
  std::unique_ptr<AudioChecksum> output(new AudioChecksum);

  // Configure NetEq and the NetEqTest object.
  NetEqTest::Callbacks callbacks;
  NetEq::Config config;
  config.enable_post_decode_vad = true;
  config.enable_fast_accelerate = true;
  auto codecs = NetEqTest::StandardDecoderMap();
  // rate_types contains the payload types that will be used for encoding.
  // Verify that they all are included in the standard decoder map, and that
  // they point to the expected decoder types.
  RTC_CHECK(
      MapHas(codecs, rate_types[0].second, SdpAudioFormat("l16", 8000, 1)));
  RTC_CHECK(
      MapHas(codecs, rate_types[1].second, SdpAudioFormat("l16", 16000, 1)));
  RTC_CHECK(
      MapHas(codecs, rate_types[2].second, SdpAudioFormat("l16", 32000, 1)));
  RTC_CHECK(
      MapHas(codecs, rate_types[3].second, SdpAudioFormat("l16", 48000, 1)));

  NetEqTest test(config, CreateBuiltinAudioDecoderFactory(), codecs,
                 /*text_log=*/nullptr, /*neteq_factory=*/nullptr,
                 std::move(input), std::move(output), callbacks);
  test.Run();
}

}  // namespace test

void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzOneInputTest(data, size);
}

}  // namespace webrtc
