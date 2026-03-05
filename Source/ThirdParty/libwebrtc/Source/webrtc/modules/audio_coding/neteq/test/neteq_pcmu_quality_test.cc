/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/flags/flag.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#include "modules/audio_coding/neteq/tools/neteq_quality_test.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

ABSL_FLAG(int, frame_size_ms, 20, "Codec frame size (milliseconds).");

using ::testing::InitGoogleTest;

namespace webrtc {
namespace test {
namespace {
constexpr int kInputSampleRateKhz = 8;
constexpr int kOutputSampleRateKhz = 8;
}  // namespace

class NetEqPcmuQualityTest : public NetEqQualityTest {
 protected:
  NetEqPcmuQualityTest()
      : NetEqQualityTest(absl::GetFlag(FLAGS_frame_size_ms),
                         kInputSampleRateKhz,
                         kOutputSampleRateKhz,
                         SdpAudioFormat("pcmu", 8000, 1)) {
    // Flag validation
    RTC_CHECK(absl::GetFlag(FLAGS_frame_size_ms) >= 10 &&
              absl::GetFlag(FLAGS_frame_size_ms) <= 60 &&
              (absl::GetFlag(FLAGS_frame_size_ms) % 10) == 0)
        << "Invalid frame size, should be 10, 20, ..., 60 ms.";
  }

  void SetUp() override {
    ASSERT_EQ(1u, channels_) << "PCMu supports only mono audio.";
    AudioEncoderPcmU::Config config;
    config.frame_size_ms = absl::GetFlag(FLAGS_frame_size_ms);
    encoder_.reset(new AudioEncoderPcmU(config));
    NetEqQualityTest::SetUp();
  }

  int EncodeBlock(int16_t* in_data,
                  size_t /* block_size_samples */,
                  Buffer* payload,
                  size_t /* max_bytes */) override {
    const size_t kFrameSizeSamples = 80;  // Samples per 10 ms.
    size_t encoded_samples = 0;
    uint32_t dummy_timestamp = 0;
    AudioEncoder::EncodedInfo info;
    do {
      info = encoder_->Encode(dummy_timestamp,
                              ArrayView<const int16_t>(
                                  in_data + encoded_samples, kFrameSizeSamples),
                              payload);
      encoded_samples += kFrameSizeSamples;
    } while (info.encoded_bytes == 0);
    return checked_cast<int>(info.encoded_bytes);
  }

 private:
  std::unique_ptr<AudioEncoderPcmU> encoder_;
};

TEST_F(NetEqPcmuQualityTest, Test) {
  Simulate();
}

}  // namespace test
}  // namespace webrtc
