/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/environment/environment_factory.h"
#include "api/neteq/default_neteq_factory.h"
#include "api/rtp_headers.h"
#include "api/units/timestamp.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

class TargetDelayTest : public ::testing::Test {
 protected:
  TargetDelayTest()
      : neteq_(
            DefaultNetEqFactory().Create(CreateEnvironment(),
                                         NetEq::Config(),
                                         CreateBuiltinAudioDecoderFactory())) {}

  ~TargetDelayTest() {}

  void SetUp() {
    constexpr int pltype = 108;
    std::map<int, SdpAudioFormat> receive_codecs = {
        {pltype, {"L16", kSampleRateHz, 1}}};
    neteq_->SetCodecs(receive_codecs);

    rtp_header_.payloadType = pltype;
    rtp_header_.timestamp = 0;
    rtp_header_.ssrc = 0x12345678;
    rtp_header_.markerBit = false;
    rtp_header_.sequenceNumber = 0;

    int16_t audio[kFrameSizeSamples];
    const int kRange = 0x7FF;  // 2047, easy for masking.
    for (size_t n = 0; n < kFrameSizeSamples; ++n)
      audio[n] = (rand() & kRange) - kRange / 2;
    WebRtcPcm16b_Encode(audio, kFrameSizeSamples, payload_);
  }

  void OutOfRangeInput() {
    EXPECT_FALSE(SetMinimumDelay(-1));
    EXPECT_FALSE(SetMinimumDelay(10001));
  }

  void TargetDelayBufferMinMax() {
    const int kTargetMinDelayMs = kNum10msPerFrame * 10;
    ASSERT_TRUE(SetMinimumDelay(kTargetMinDelayMs));
    for (int m = 0; m < 30; ++m)  // Run enough iterations to fill the buffer.
      Run(true);
    int clean_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetMinDelayMs, clean_optimal_delay);

    const int kTargetMaxDelayMs = 2 * (kNum10msPerFrame * 10);
    ASSERT_TRUE(SetMaximumDelay(kTargetMaxDelayMs));
    for (int n = 0; n < 30; ++n)  // Run enough iterations to fill the buffer.
      Run(false);

    int capped_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetMaxDelayMs, capped_optimal_delay);
  }

 private:
  static const int kSampleRateHz = 16000;
  static const int kNum10msPerFrame = 2;
  static const size_t kFrameSizeSamples = 320;  // 20 ms @ 16 kHz.
  // payload-len = frame-samples * 2 bytes/sample.
  static const int kPayloadLenBytes = 320 * 2;
  // Inter-arrival time in number of packets in a jittery channel. One is no
  // jitter.
  static const int kInterarrivalJitterPacket = 2;

  void Push() {
    rtp_header_.timestamp += kFrameSizeSamples;
    rtp_header_.sequenceNumber++;
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_header_,
                                      rtc::ArrayView<const uint8_t>(
                                          payload_, kFrameSizeSamples * 2),
                                      Timestamp::MinusInfinity()));
  }

  // Pull audio equivalent to the amount of audio in one RTP packet.
  void Pull() {
    AudioFrame frame;
    bool muted;
    for (int k = 0; k < kNum10msPerFrame; ++k) {  // Pull one frame.
      ASSERT_EQ(NetEq::kOK, neteq_->GetAudio(&frame, &muted));
      ASSERT_FALSE(muted);
      // Had to use ASSERT_TRUE, ASSERT_EQ generated error.
      ASSERT_TRUE(kSampleRateHz == frame.sample_rate_hz_);
      ASSERT_EQ(1u, frame.num_channels_);
      ASSERT_TRUE(kSampleRateHz / 100 == frame.samples_per_channel_);
    }
  }

  void Run(bool clean) {
    for (int n = 0; n < 10; ++n) {
      for (int m = 0; m < 5; ++m) {
        Push();
        Pull();
      }

      if (!clean) {
        for (int m = 0; m < 10; ++m) {  // Long enough to trigger delay change.
          Push();
          for (int n = 0; n < kInterarrivalJitterPacket; ++n)
            Pull();
        }
      }
    }
  }

  int SetMinimumDelay(int delay_ms) {
    return neteq_->SetMinimumDelay(delay_ms);
  }

  int SetMaximumDelay(int delay_ms) {
    return neteq_->SetMaximumDelay(delay_ms);
  }

  int GetCurrentOptimalDelayMs() {
    NetEqNetworkStatistics neteq_stats;
    neteq_->NetworkStatistics(&neteq_stats);
    return neteq_stats.preferred_buffer_size_ms;
  }

  std::unique_ptr<NetEq> neteq_;
  RTPHeader rtp_header_;
  uint8_t payload_[kPayloadLenBytes];
};

// Flaky on iOS: webrtc:7057.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_OutOfRangeInput DISABLED_OutOfRangeInput
#else
#define MAYBE_OutOfRangeInput OutOfRangeInput
#endif
TEST_F(TargetDelayTest, MAYBE_OutOfRangeInput) {
  OutOfRangeInput();
}

// Flaky on iOS: webrtc:7057.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_TargetDelayBufferMinMax DISABLED_TargetDelayBufferMinMax
#else
#define MAYBE_TargetDelayBufferMinMax TargetDelayBufferMinMax
#endif
TEST_F(TargetDelayTest, MAYBE_TargetDelayBufferMinMax) {
  TargetDelayBufferMinMax();
}

}  // namespace webrtc
