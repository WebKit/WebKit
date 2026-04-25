/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metric.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

using test::GetGlobalMetricsLogger;
using test::ImprovementDirection;
using test::Unit;

int64_t RunComplexityTest(const Environment& env,
                          AudioEncoderOpusConfig config) {
  // Create encoder.
  const auto encoder = AudioEncoderOpus::MakeAudioEncoder(
      env, std::move(config), {.payload_type = 17});
  // Open speech file.
  const std::string kInputFileName =
      test::ResourcePath("audio_coding/speech_mono_32_48kHz", "pcm");
  test::AudioLoop audio_loop;
  constexpr int kSampleRateHz = 48000;
  EXPECT_EQ(kSampleRateHz, encoder->SampleRateHz());
  constexpr size_t kMaxLoopLengthSamples =
      kSampleRateHz * 10;  // 10 second loop.
  constexpr size_t kInputBlockSizeSamples =
      10 * kSampleRateHz / 1000;  // 60 ms.
  EXPECT_TRUE(audio_loop.Init(kInputFileName, kMaxLoopLengthSamples,
                              kInputBlockSizeSamples));
  // Encode.
  const int64_t start_time_ms = env.clock().TimeInMilliseconds();
  AudioEncoder::EncodedInfo info;
  Buffer encoded = Buffer::CreateWithCapacity(500);
  uint32_t rtp_timestamp = 0u;
  for (size_t i = 0; i < 10000; ++i) {
    encoded.Clear();
    info = encoder->Encode(rtp_timestamp, audio_loop.GetNextBlock(), &encoded);
    rtp_timestamp += kInputBlockSizeSamples;
  }
  return env.clock().TimeInMilliseconds() - start_time_ms;
}

// This test encodes an audio file using Opus twice with different bitrates
// (~11 kbps and 15.5 kbps). The runtime for each is measured, and the ratio
// between the two is calculated and tracked. This test explicitly sets the
// low_rate_complexity to 9. When running on desktop platforms, this is the same
// as the regular complexity, and the expectation is that the resulting ratio
// should be less than 100% (since the encoder runs faster at lower bitrates,
// given a fixed complexity setting). On the other hand, when running on
// mobiles, the regular complexity is 5, and we expect the resulting ratio to
// be higher, since we have explicitly asked for a higher complexity setting at
// the lower rate.
TEST(AudioEncoderOpusComplexityAdaptationTest, Adaptation_On) {
  const Environment env = CreateEnvironment();
  // Create config.
  // The limit -- including the hysteresis window -- at which the complexity
  // shuold be increased.
  int64_t runtime_10999bps = RunComplexityTest(
      env, {.bitrate_bps = 11000 - 1, .low_rate_complexity = 9});

  // Re-create config because RunComplexityTest consumed the previous one.
  // Re-create config because RunComplexityTest consumed the previous one.
  int64_t runtime_15500bps =
      RunComplexityTest(env, {.bitrate_bps = 15500, .low_rate_complexity = 9});

  GetGlobalMetricsLogger()->LogSingleValueMetric(
      "opus_encoding_complexity_ratio", "adaptation_on",
      100.0 * runtime_10999bps / runtime_15500bps, Unit::kPercent,
      ImprovementDirection::kNeitherIsBetter);
}

// This test is identical to the one above, but without the complexity
// adaptation enabled (neither on desktop, nor on mobile). The expectation is
// that the resulting ratio is less than 100% at all times.
TEST(AudioEncoderOpusComplexityAdaptationTest, Adaptation_Off) {
  const Environment env = CreateEnvironment();
  // Create config.
  // The limit -- including the hysteresis window -- at which the complexity
  // shuold be increased (but not in this test since complexity adaptation is
  // disabled).
  int64_t runtime_10999bps = RunComplexityTest(env, {.bitrate_bps = 11000 - 1});

  int64_t runtime_15500bps = RunComplexityTest(env, {.bitrate_bps = 15500});

  GetGlobalMetricsLogger()->LogSingleValueMetric(
      "opus_encoding_complexity_ratio", "adaptation_off",
      100.0 * runtime_10999bps / runtime_15500bps, Unit::kPercent,
      ImprovementDirection::kNeitherIsBetter);
}

}  // namespace
}  // namespace webrtc
