/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/opus_interface.h"
#include "modules/audio_coding/codecs/opus/opus_inst.h"
#include "modules/audio_coding/neteq/tools/neteq_quality_test.h"
#include "rtc_base/flags.h"

using testing::InitGoogleTest;

namespace webrtc {
namespace test {
namespace {

static const int kOpusBlockDurationMs = 20;
static const int kOpusSamplingKhz = 48;

DEFINE_int(bit_rate_kbps, 32, "Target bit rate (kbps).");

DEFINE_int(complexity, 10, "Complexity: 0 ~ 10 -- defined as in Opus"
    "specification.");

DEFINE_int(maxplaybackrate, 48000, "Maximum playback rate (Hz).");

DEFINE_int(application, 0, "Application mode: 0 -- VOIP, 1 -- Audio.");

DEFINE_int(reported_loss_rate, 10, "Reported percentile of packet loss.");

DEFINE_bool(fec, false, "Enable FEC for encoding (-nofec to disable).");

DEFINE_bool(dtx, false, "Enable DTX for encoding (-nodtx to disable).");

DEFINE_int(sub_packets, 1, "Number of sub packets to repacketize.");

}  // namespace

class NetEqOpusQualityTest : public NetEqQualityTest {
 protected:
  NetEqOpusQualityTest();
  void SetUp() override;
  void TearDown() override;
  int EncodeBlock(int16_t* in_data, size_t block_size_samples,
                  rtc::Buffer* payload, size_t max_bytes) override;
 private:
  WebRtcOpusEncInst* opus_encoder_;
  OpusRepacketizer* repacketizer_;
  size_t sub_block_size_samples_;
  int bit_rate_kbps_;
  bool fec_;
  bool dtx_;
  int complexity_;
  int maxplaybackrate_;
  int target_loss_rate_;
  int sub_packets_;
  int application_;
};

NetEqOpusQualityTest::NetEqOpusQualityTest()
    : NetEqQualityTest(kOpusBlockDurationMs * FLAG_sub_packets,
                       kOpusSamplingKhz,
                       kOpusSamplingKhz,
                       NetEqDecoder::kDecoderOpus),
      opus_encoder_(NULL),
      repacketizer_(NULL),
      sub_block_size_samples_(
          static_cast<size_t>(kOpusBlockDurationMs * kOpusSamplingKhz)),
      bit_rate_kbps_(FLAG_bit_rate_kbps),
      fec_(FLAG_fec),
      dtx_(FLAG_dtx),
      complexity_(FLAG_complexity),
      maxplaybackrate_(FLAG_maxplaybackrate),
      target_loss_rate_(FLAG_reported_loss_rate),
      sub_packets_(FLAG_sub_packets) {
  // Flag validation
  RTC_CHECK(FLAG_bit_rate_kbps >= 6 && FLAG_bit_rate_kbps <= 510)
      << "Invalid bit rate, should be between 6 and 510 kbps.";

  RTC_CHECK(FLAG_complexity >= -1 && FLAG_complexity <= 10)
      << "Invalid complexity setting, should be between 0 and 10.";

  RTC_CHECK(FLAG_application == 0 || FLAG_application == 1)
      << "Invalid application mode, should be 0 or 1.";

  RTC_CHECK(FLAG_reported_loss_rate >= 0 && FLAG_reported_loss_rate <= 100)
      << "Invalid packet loss percentile, should be between 0 and 100.";

  RTC_CHECK(FLAG_sub_packets >= 1 && FLAG_sub_packets <= 3)
      << "Invalid number of sub packets, should be between 1 and 3.";

  // Redefine decoder type if input is stereo.
  if (channels_ > 1) {
    decoder_type_ = NetEqDecoder::kDecoderOpus_2ch;
  }
  application_ = FLAG_application;
}

void NetEqOpusQualityTest::SetUp() {
  // Create encoder memory.
  WebRtcOpus_EncoderCreate(&opus_encoder_, channels_, application_);
  ASSERT_TRUE(opus_encoder_);

  // Create repacketizer.
  repacketizer_ = opus_repacketizer_create();
  ASSERT_TRUE(repacketizer_);

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, bit_rate_kbps_ * 1000));
  if (fec_) {
    EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_encoder_));
  }
  if (dtx_) {
    EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_encoder_));
  }
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_encoder_, complexity_));
  EXPECT_EQ(0, WebRtcOpus_SetMaxPlaybackRate(opus_encoder_, maxplaybackrate_));
  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_encoder_,
                                            target_loss_rate_));
  NetEqQualityTest::SetUp();
}

void NetEqOpusQualityTest::TearDown() {
  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  opus_repacketizer_destroy(repacketizer_);
  NetEqQualityTest::TearDown();
}

int NetEqOpusQualityTest::EncodeBlock(int16_t* in_data,
                                      size_t block_size_samples,
                                      rtc::Buffer* payload, size_t max_bytes) {
  EXPECT_EQ(block_size_samples, sub_block_size_samples_ * sub_packets_);
  int16_t* pointer = in_data;
  int value;
  opus_repacketizer_init(repacketizer_);
  for (int idx = 0; idx < sub_packets_; idx++) {
    payload->AppendData(max_bytes, [&] (rtc::ArrayView<uint8_t> payload) {
        value = WebRtcOpus_Encode(opus_encoder_,
                                  pointer, sub_block_size_samples_,
                                  max_bytes, payload.data());

        Log() << "Encoded a frame with Opus mode "
              << (value == 0 ? 0 : payload[0] >> 3)
              << std::endl;

        return (value >= 0) ? static_cast<size_t>(value) : 0;
      });

    if (OPUS_OK != opus_repacketizer_cat(repacketizer_,
                                         payload->data(), value)) {
      opus_repacketizer_init(repacketizer_);
      // If the repacketization fails, we discard this frame.
      return 0;
    }
    pointer += sub_block_size_samples_ * channels_;
  }
  value = opus_repacketizer_out(repacketizer_, payload->data(),
                                static_cast<opus_int32>(max_bytes));
  EXPECT_GE(value, 0);
  return value;
}

TEST_F(NetEqOpusQualityTest, Test) {
  Simulate();
}

}  // namespace test
}  // namespace webrtc
