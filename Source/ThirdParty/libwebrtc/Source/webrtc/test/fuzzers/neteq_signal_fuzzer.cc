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
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace test {
namespace {
// Helper class to take care of the fuzzer input, read from it, and keep track
// of when the end of the data has been reached.
class FuzzData {
 public:
  explicit FuzzData(rtc::ArrayView<const uint8_t> data) : data_(data) {}

  // Returns true if n bytes can be read.
  bool CanReadBytes(size_t n) const { return data_ix_ + n <= data_.size(); }

  // Reads and returns data of type T.
  template <typename T>
  T Read() {
    RTC_CHECK(CanReadBytes(sizeof(T)));
    T x = ByteReader<T>::ReadLittleEndian(&data_[data_ix_]);
    data_ix_ += sizeof(T);
    return x;
  }

  // Reads and returns data of type T. Returns default_value if not enough
  // fuzzer input remains to read a T.
  template <typename T>
  T ReadOrDefaultValue(T default_value) {
    if (!CanReadBytes(sizeof(T))) {
      return default_value;
    }
    return Read<T>();
  }

  // Like ReadOrDefaultValue, but replaces the value 0 with default_value.
  template <typename T>
  T ReadOrDefaultValueNotZero(T default_value) {
    static_assert(std::is_integral<T>::value, "");
    T x = ReadOrDefaultValue(default_value);
    return x == 0 ? default_value : x;
  }

  // Returns one of the elements from the provided input array. The selection
  // is based on the fuzzer input data. If not enough fuzzer data is available,
  // the method will return the first element in the input array. The reason for
  // not flaggin this as an error is that the method is called from the
  // FuzzSignalInput constructor, and in constructors we typically do not handle
  // errors. The code will work anyway, and the fuzzer will likely see that
  // providing more data will actually make this method return something else.
  template <typename T>
  T SelectOneOf(rtc::ArrayView<const T> select_from) {
    RTC_CHECK_LE(select_from.size(), std::numeric_limits<uint8_t>::max());
    // Read an index between 0 and select_from.size() - 1 from the fuzzer data.
    uint8_t index = ReadOrDefaultValue<uint8_t>(0) % select_from.size();
    return select_from[index];
  }

 private:
  rtc::ArrayView<const uint8_t> data_;
  size_t data_ix_ = 0;
};

// Generate a mixture of sine wave and gaussian noise.
class SineAndNoiseGenerator : public EncodeNetEqInput::Generator {
 public:
  // The noise generator is seeded with a value from the fuzzer data, but 0 is
  // avoided (since it is not allowed by the Random class).
  SineAndNoiseGenerator(int sample_rate_hz, FuzzData* fuzz_data)
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
  FuzzData& fuzz_data_;
  Random noise_generator_;
};

class FuzzSignalInput : public NetEqInput {
 public:
  explicit FuzzSignalInput(FuzzData* fuzz_data,
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
    output_event_period_ms_ =
        fuzz_data_.SelectOneOf(rtc::ArrayView<const int>(output_event_periods));
  }

  rtc::Optional<int64_t> NextPacketTime() const override {
    return packet_->time_ms;
  }

  rtc::Optional<int64_t> NextOutputEventTime() const override {
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

  rtc::Optional<RTPHeader> NextHeader() const override {
    RTC_DCHECK(packet_);
    return packet_->header;
  }

 private:
  bool ended_ = false;
  FuzzData& fuzz_data_;
  std::unique_ptr<EncodeNetEqInput> input_;
  std::unique_ptr<PacketData> packet_;
  int64_t next_output_event_ms_ = 0;
  int64_t output_event_period_ms_ = 10;
};
}  // namespace

void FuzzOneInputTest(const uint8_t* data, size_t size) {
  if (size < 1)
    return;
  FuzzData fuzz_data(rtc::ArrayView<const uint8_t>(data, size));

  // Allowed sample rates and payload types used in the test.
  std::pair<int, uint8_t> rate_types[] = {
      {8000, 93}, {16000, 94}, {32000, 95}, {48000, 96}};
  const auto rate_type = fuzz_data.SelectOneOf(
      rtc::ArrayView<const std::pair<int, uint8_t>>(rate_types));
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
  NetEqTest::DecoderMap codecs;
  codecs[0] = std::make_pair(NetEqDecoder::kDecoderPCMu, "pcmu");
  codecs[8] = std::make_pair(NetEqDecoder::kDecoderPCMa, "pcma");
  codecs[103] = std::make_pair(NetEqDecoder::kDecoderISAC, "isac");
  codecs[104] = std::make_pair(NetEqDecoder::kDecoderISACswb, "isac-swb");
  codecs[111] = std::make_pair(NetEqDecoder::kDecoderOpus, "opus");
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
  // One of these payload types will be used for encoding.
  codecs[rate_types[0].second] =
      std::make_pair(NetEqDecoder::kDecoderPCM16B, "pcm16-nb");
  codecs[rate_types[1].second] =
      std::make_pair(NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb");
  codecs[rate_types[2].second] =
      std::make_pair(NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16-swb32");
  codecs[rate_types[3].second] =
      std::make_pair(NetEqDecoder::kDecoderPCM16Bswb48kHz, "pcm16-swb48");
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
