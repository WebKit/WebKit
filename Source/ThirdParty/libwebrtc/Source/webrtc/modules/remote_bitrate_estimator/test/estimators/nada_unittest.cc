/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/estimators/nada.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "modules/remote_bitrate_estimator/test/packet.h"
#include "modules/remote_bitrate_estimator/test/packet_sender.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/constructormagic.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {

class FilterTest : public ::testing::Test {
 public:
  void MedianFilterConstantArray() {
    std::fill_n(raw_signal_, kNumElements, kSignalValue);
    for (int i = 0; i < kNumElements; ++i) {
      int size = std::min(5, i + 1);
      median_filtered_[i] =
          NadaBweReceiver::MedianFilter(&raw_signal_[i + 1 - size], size);
    }
  }

  void MedianFilterIntermittentNoise() {
    const int kValue = 500;
    const int kNoise = 100;

    for (int i = 0; i < kNumElements; ++i) {
      raw_signal_[i] = kValue + kNoise * (i % 10 == 9 ? 1 : 0);
    }
    for (int i = 0; i < kNumElements; ++i) {
      int size = std::min(5, i + 1);
      median_filtered_[i] =
          NadaBweReceiver::MedianFilter(&raw_signal_[i + 1 - size], size);
      EXPECT_EQ(median_filtered_[i], kValue);
    }
  }

  void ExponentialSmoothingFilter(const int64_t raw_signal_[],
                                  int num_elements,
                                  int64_t exp_smoothed[]) {
    exp_smoothed[0] =
        NadaBweReceiver::ExponentialSmoothingFilter(raw_signal_[0], -1, kAlpha);
    for (int i = 1; i < num_elements; ++i) {
      exp_smoothed[i] = NadaBweReceiver::ExponentialSmoothingFilter(
          raw_signal_[i], exp_smoothed[i - 1], kAlpha);
    }
  }

  void ExponentialSmoothingConstantArray(int64_t exp_smoothed[]) {
    std::fill_n(raw_signal_, kNumElements, kSignalValue);
    ExponentialSmoothingFilter(raw_signal_, kNumElements, exp_smoothed);
  }

 protected:
  static const int kNumElements = 1000;
  static const int64_t kSignalValue;
  static const float kAlpha;
  int64_t raw_signal_[kNumElements];
  int64_t median_filtered_[kNumElements];
};

const int64_t FilterTest::kSignalValue = 200;
const float FilterTest::kAlpha = 0.1f;

class TestBitrateObserver : public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_(0), last_fraction_loss_(0), last_rtt_(0) {}

  virtual void OnNetworkChanged(uint32_t bitrate,
                                uint8_t fraction_loss,
                                int64_t rtt) {
    last_bitrate_ = bitrate;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ = rtt;
  }
  uint32_t last_bitrate_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_;
};

class NadaSenderSideTest : public ::testing::Test {
 public:
  NadaSenderSideTest()
      : observer_(),
        simulated_clock_(0),
        nada_sender_(&observer_, &simulated_clock_) {}
  ~NadaSenderSideTest() {}

 private:
  TestBitrateObserver observer_;
  SimulatedClock simulated_clock_;

 protected:
  NadaBweSender nada_sender_;
};

class NadaReceiverSideTest : public ::testing::Test {
 public:
  NadaReceiverSideTest() : nada_receiver_(kFlowId) {}
  ~NadaReceiverSideTest() {}

 protected:
  const int kFlowId = 1;  // Arbitrary.
  NadaBweReceiver nada_receiver_;
};

class NadaFbGenerator {
 public:
  NadaFbGenerator();

  static NadaFeedback NotCongestedFb(size_t receiving_rate,
                                     int64_t ref_signal_ms,
                                     int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = ref_signal_ms;
    int64_t est_queuing_delay_signal_ms = ref_signal_ms;
    int64_t congestion_signal_ms = ref_signal_ms;
    float derivative = 0.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

  static NadaFeedback CongestedFb(size_t receiving_rate, int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = 1000;
    int64_t est_queuing_delay_signal_ms = 800;
    int64_t congestion_signal_ms = 1000;
    float derivative = 1.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

  static NadaFeedback ExtremelyCongestedFb(size_t receiving_rate,
                                           int64_t send_time_ms) {
    int64_t exp_smoothed_delay_ms = 100000;
    int64_t est_queuing_delay_signal_ms = 0;
    int64_t congestion_signal_ms = 100000;
    float derivative = 10000.0f;
    return NadaFeedback(kFlowId, kNowMs, exp_smoothed_delay_ms,
                        est_queuing_delay_signal_ms, congestion_signal_ms,
                        derivative, receiving_rate, send_time_ms);
  }

 private:
  // Arbitrary values, won't change these test results.
  static const int kFlowId = 2;
  static const int64_t kNowMs = 1000;
};

// Verify if AcceleratedRampUp is called and that bitrate increases.
TEST_F(NadaSenderSideTest, AcceleratedRampUp) {
  const int64_t kRefSignalMs = 1;
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 2 * NadaBweSender::kMinNadaBitrateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      receiving_rate, kRefSignalMs, send_time_ms);

  nada_sender_.set_original_operating_mode(true);
  nada_sender_.set_bitrate_kbps(original_bitrate);

  // Trigger AcceleratedRampUp mode.
  nada_sender_.GiveFeedback(not_congested_fb);
  int bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_GT(bitrate_1_kbps, original_bitrate);
  // Updates the bitrate according to the receiving rate and other constant
  // parameters.
  nada_sender_.AcceleratedRampUp(not_congested_fb);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), bitrate_1_kbps);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(original_bitrate);
  // Trigger AcceleratedRampUp mode.
  nada_sender_.GiveFeedback(not_congested_fb);
  bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_GT(bitrate_1_kbps, original_bitrate);
  nada_sender_.AcceleratedRampUp(not_congested_fb);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), bitrate_1_kbps);
}

// Verify if AcceleratedRampDown is called and if bitrate decreases.
TEST_F(NadaSenderSideTest, AcceleratedRampDown) {
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 3 * NadaBweSender::kMinNadaBitrateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(original_bitrate);
  nada_sender_.GiveFeedback(congested_fb);  // Trigger AcceleratedRampDown mode.
  int bitrate_1_kbps = nada_sender_.bitrate_kbps();
  EXPECT_LE(bitrate_1_kbps, original_bitrate * 0.9f + 0.5f);
  EXPECT_LT(bitrate_1_kbps, original_bitrate);

  // Updates the bitrate according to the receiving rate and other constant
  // parameters.
  nada_sender_.AcceleratedRampDown(congested_fb);
  int bitrate_2_kbps =
      std::max(nada_sender_.bitrate_kbps(), NadaBweSender::kMinNadaBitrateKbps);
  EXPECT_EQ(bitrate_2_kbps, bitrate_1_kbps);
}

TEST_F(NadaSenderSideTest, GradualRateUpdate) {
  const int64_t kDeltaSMs = 20;
  const int64_t kRefSignalMs = 20;
  const int64_t kOneWayDelayMs = 50;
  int original_bitrate = 5 * NadaBweSender::kMinNadaBitrateKbps;
  size_t receiving_rate = static_cast<size_t>(original_bitrate);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);
  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      original_bitrate, kRefSignalMs, send_time_ms);

  nada_sender_.set_bitrate_kbps(original_bitrate);
  double smoothing_factor = 0.0;
  nada_sender_.GradualRateUpdate(congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_EQ(nada_sender_.bitrate_kbps(), original_bitrate);

  smoothing_factor = 1.0;
  nada_sender_.GradualRateUpdate(congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_LT(nada_sender_.bitrate_kbps(), original_bitrate);

  nada_sender_.set_bitrate_kbps(original_bitrate);
  nada_sender_.GradualRateUpdate(not_congested_fb, kDeltaSMs, smoothing_factor);
  EXPECT_GT(nada_sender_.bitrate_kbps(), original_bitrate);
}

// Sending bitrate should decrease and reach its Min bound.
TEST_F(NadaSenderSideTest, VeryLowBandwith) {
  const int64_t kOneWayDelayMs = 50;

  size_t receiving_rate =
      static_cast<size_t>(NadaBweSender::kMinNadaBitrateKbps);
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback extremely_congested_fb =
      NadaFbGenerator::ExtremelyCongestedFb(receiving_rate, send_time_ms);
  NadaFeedback congested_fb =
      NadaFbGenerator::CongestedFb(receiving_rate, send_time_ms);

  nada_sender_.set_bitrate_kbps(5 * NadaBweSender::kMinNadaBitrateKbps);
  nada_sender_.set_original_operating_mode(true);
  for (int i = 0; i < 100; ++i) {
    // Trigger GradualRateUpdate mode.
    nada_sender_.GiveFeedback(extremely_congested_fb);
  }
  // The original implementation doesn't allow the bitrate to stay at kMin,
  // even if the congestion signal is very high.
  EXPECT_GE(nada_sender_.bitrate_kbps(), NadaBweSender::kMinNadaBitrateKbps);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(5 * NadaBweSender::kMinNadaBitrateKbps);

  for (int i = 0; i < 1000; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    // Trigger AcceleratedRampDown mode.
    nada_sender_.GiveFeedback(congested_fb);
    EXPECT_LE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), NadaBweSender::kMinNadaBitrateKbps);
}

// Sending bitrate should increase and reach its Max bound.
TEST_F(NadaSenderSideTest, VeryHighBandwith) {
  const int64_t kOneWayDelayMs = 50;
  const size_t kRecentReceivingRate = static_cast<size_t>(kMaxBitrateKbps);
  const int64_t kRefSignalMs = 1;
  int64_t send_time_ms = nada_sender_.NowMs() - kOneWayDelayMs;

  NadaFeedback not_congested_fb = NadaFbGenerator::NotCongestedFb(
      kRecentReceivingRate, kRefSignalMs, send_time_ms);

  nada_sender_.set_original_operating_mode(true);
  for (int i = 0; i < 100; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    nada_sender_.GiveFeedback(not_congested_fb);
    EXPECT_GE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), kMaxBitrateKbps);

  nada_sender_.set_original_operating_mode(false);
  nada_sender_.set_bitrate_kbps(NadaBweSender::kMinNadaBitrateKbps);

  for (int i = 0; i < 100; ++i) {
    int previous_bitrate = nada_sender_.bitrate_kbps();
    nada_sender_.GiveFeedback(not_congested_fb);
    EXPECT_GE(nada_sender_.bitrate_kbps(), previous_bitrate);
  }
  EXPECT_EQ(nada_sender_.bitrate_kbps(), kMaxBitrateKbps);
}

TEST_F(NadaReceiverSideTest, FeedbackInitialCases) {
  std::unique_ptr<NadaFeedback> nada_feedback(
      static_cast<NadaFeedback*>(nada_receiver_.GetFeedback(0)));
  EXPECT_EQ(nada_feedback, nullptr);

  nada_feedback.reset(
      static_cast<NadaFeedback*>(nada_receiver_.GetFeedback(100)));
  EXPECT_EQ(nada_feedback->exp_smoothed_delay_ms(), -1);
  EXPECT_EQ(nada_feedback->est_queuing_delay_signal_ms(), 0L);
  EXPECT_EQ(nada_feedback->congestion_signal(), 0L);
  EXPECT_EQ(nada_feedback->derivative(), 0.0f);
  EXPECT_EQ(nada_feedback->receiving_rate(), 0.0f);
}

TEST_F(NadaReceiverSideTest, FeedbackEmptyQueues) {
  const int64_t kTimeGapMs = 50;  // Between each packet.
  const int64_t kOneWayDelayMs = 50;

  // No added latency, delay = kOneWayDelayMs.
  for (int i = 1; i < 10; ++i) {
    int64_t send_time_us = i * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + kOneWayDelayMs;
    uint16_t sequence_number = static_cast<uint16_t>(i);
    // Payload sizes are not important here.
    const MediaPacket media_packet(kFlowId, send_time_us, 0, sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);
  }

  // Baseline delay will be equal kOneWayDelayMs.
  std::unique_ptr<NadaFeedback> nada_feedback(
      static_cast<NadaFeedback*>(nada_receiver_.GetFeedback(500)));
  EXPECT_EQ(nada_feedback->exp_smoothed_delay_ms(), 0L);
  EXPECT_EQ(nada_feedback->est_queuing_delay_signal_ms(), 0L);
  EXPECT_EQ(nada_feedback->congestion_signal(), 0L);
  EXPECT_EQ(nada_feedback->derivative(), 0.0f);
}

TEST_F(NadaReceiverSideTest, FeedbackIncreasingDelay) {
  // Since packets are 100ms apart, each one corresponds to a feedback.
  const int64_t kTimeGapMs = 100;  // Between each packet.

  // Raw delays are = [10 20 30 40 50 60 70 80] ms.
  // Baseline delay will be 50 ms.
  // Delay signals should be: [0 10 20 30 40 50 60 70] ms.
  const int64_t kMedianFilteredDelaysMs[] = {0, 5, 10, 15, 20, 30, 40, 50};
  const int kNumPackets = arraysize(kMedianFilteredDelaysMs);
  const float kAlpha = 0.1f;  // Used for exponential smoothing.

  int64_t exp_smoothed_delays_ms[kNumPackets];
  exp_smoothed_delays_ms[0] = kMedianFilteredDelaysMs[0];

  for (int i = 1; i < kNumPackets; ++i) {
    exp_smoothed_delays_ms[i] = static_cast<int64_t>(
        kAlpha * kMedianFilteredDelaysMs[i] +
        (1.0f - kAlpha) * exp_smoothed_delays_ms[i - 1] + 0.5f);
  }

  for (int i = 0; i < kNumPackets; ++i) {
    int64_t send_time_us = (i + 1) * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + 10 * (i + 1);
    uint16_t sequence_number = static_cast<uint16_t>(i + 1);
    // Payload sizes are not important here.
    const MediaPacket media_packet(kFlowId, send_time_us, 0, sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);

    std::unique_ptr<NadaFeedback> nada_feedback(static_cast<NadaFeedback*>(
        nada_receiver_.GetFeedback(arrival_time_ms)));
    EXPECT_EQ(nada_feedback->exp_smoothed_delay_ms(),
              exp_smoothed_delays_ms[i]);
    // Since delay signals are lower than 50ms, they will not be non-linearly
    // warped.
    EXPECT_EQ(nada_feedback->est_queuing_delay_signal_ms(),
              exp_smoothed_delays_ms[i]);
    // Zero loss, congestion signal = queuing_delay
    EXPECT_EQ(nada_feedback->congestion_signal(), exp_smoothed_delays_ms[i]);
    if (i == 0) {
      EXPECT_NEAR(nada_feedback->derivative(),
                  static_cast<float>(exp_smoothed_delays_ms[i]) / kTimeGapMs,
                  0.005f);
    } else {
      EXPECT_NEAR(nada_feedback->derivative(),
                  static_cast<float>(exp_smoothed_delays_ms[i] -
                                     exp_smoothed_delays_ms[i - 1]) /
                      kTimeGapMs,
                  0.005f);
    }
  }
}

int64_t Warp(int64_t input) {
  const int64_t kMinThreshold = 50;   // Referred as d_th.
  const int64_t kMaxThreshold = 400;  // Referred as d_max.
  if (input < kMinThreshold) {
    return input;
  } else if (input < kMaxThreshold) {
    return static_cast<int64_t>(
        pow((static_cast<double>(kMaxThreshold - input)) /
                (kMaxThreshold - kMinThreshold),
            4.0) *
        kMinThreshold);
  } else {
    return 0L;
  }
}

TEST_F(NadaReceiverSideTest, FeedbackWarpedDelay) {
  // Since packets are 100ms apart, each one corresponds to a feedback.
  const int64_t kTimeGapMs = 100;  // Between each packet.

  // Raw delays are = [50 250 450 650 850 1050 1250 1450] ms.
  // Baseline delay will be 50 ms.
  // Delay signals should be: [0 200 400 600 800 1000 1200 1400] ms.
  const int64_t kMedianFilteredDelaysMs[] = {0,   100, 200, 300,
                                             400, 600, 800, 1000};
  const int kNumPackets = arraysize(kMedianFilteredDelaysMs);
  const float kAlpha = 0.1f;  // Used for exponential smoothing.

  int64_t exp_smoothed_delays_ms[kNumPackets];
  exp_smoothed_delays_ms[0] = kMedianFilteredDelaysMs[0];

  for (int i = 1; i < kNumPackets; ++i) {
    exp_smoothed_delays_ms[i] = static_cast<int64_t>(
        kAlpha * kMedianFilteredDelaysMs[i] +
        (1.0f - kAlpha) * exp_smoothed_delays_ms[i - 1] + 0.5f);
  }

  for (int i = 0; i < kNumPackets; ++i) {
    int64_t send_time_us = (i + 1) * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + 50 + 200 * i;
    uint16_t sequence_number = static_cast<uint16_t>(i + 1);
    // Payload sizes are not important here.
    const MediaPacket media_packet(kFlowId, send_time_us, 0, sequence_number);
    nada_receiver_.ReceivePacket(arrival_time_ms, media_packet);

    std::unique_ptr<NadaFeedback> nada_feedback(static_cast<NadaFeedback*>(
        nada_receiver_.GetFeedback(arrival_time_ms)));
    EXPECT_EQ(nada_feedback->exp_smoothed_delay_ms(),
              exp_smoothed_delays_ms[i]);
    // Delays can be non-linearly warped.
    EXPECT_EQ(nada_feedback->est_queuing_delay_signal_ms(),
              Warp(exp_smoothed_delays_ms[i]));
    // Zero loss, congestion signal = queuing_delay
    EXPECT_EQ(nada_feedback->congestion_signal(),
              Warp(exp_smoothed_delays_ms[i]));
  }
}

TEST_F(FilterTest, MedianConstantArray) {
  MedianFilterConstantArray();
  for (int i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(median_filtered_[i], raw_signal_[i]);
  }
}

TEST_F(FilterTest, MedianIntermittentNoise) {
  MedianFilterIntermittentNoise();
}

TEST_F(FilterTest, ExponentialSmoothingConstantArray) {
  int64_t exp_smoothed[kNumElements];
  ExponentialSmoothingConstantArray(exp_smoothed);
  for (int i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(exp_smoothed[i], kSignalValue);
  }
}

TEST_F(FilterTest, ExponentialSmoothingInitialPertubation) {
  const int64_t kSignal[] = {90000, 0, 0, 0, 0, 0};
  const int kNumElements = arraysize(kSignal);
  int64_t exp_smoothed[kNumElements];
  ExponentialSmoothingFilter(kSignal, kNumElements, exp_smoothed);
  for (int i = 1; i < kNumElements; ++i) {
    EXPECT_EQ(
        exp_smoothed[i],
        static_cast<int64_t>(exp_smoothed[i - 1] * (1.0f - kAlpha) + 0.5f));
  }
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
