/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

#include "webrtc/base/random.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/inter_arrival.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {

const double kRtpTimestampToMs = 1.0 / 90.0;

class OveruseDetectorTest : public ::testing::Test {
 public:
  OveruseDetectorTest()
      : now_ms_(0),
        receive_time_ms_(0),
        rtp_timestamp_(10 * 90),
        overuse_detector_(),
        overuse_estimator_(new OveruseEstimator(options_)),
        inter_arrival_(new InterArrival(5 * 90, kRtpTimestampToMs, true)),
        random_(123456789) {}

 protected:
  void SetUp() override {
    overuse_detector_.reset(new OveruseDetector());
  }

  int Run100000Samples(int packets_per_frame, size_t packet_size, int mean_ms,
                       int standard_deviation_ms) {
    int unique_overuse = 0;
    int last_overuse = -1;
    for (int i = 0; i < 100000; ++i) {
      for (int j = 0; j < packets_per_frame; ++j) {
        UpdateDetector(rtp_timestamp_, receive_time_ms_, packet_size);
      }
      rtp_timestamp_ += mean_ms * 90;
      now_ms_ += mean_ms;
      receive_time_ms_ = std::max<int64_t>(
          receive_time_ms_,
          now_ms_ + static_cast<int64_t>(
                        random_.Gaussian(0, standard_deviation_ms) + 0.5));
      if (BandwidthUsage::kBwOverusing == overuse_detector_->State()) {
        if (last_overuse + 1 != i) {
          unique_overuse++;
        }
        last_overuse = i;
      }
    }
    return unique_overuse;
  }

  int RunUntilOveruse(int packets_per_frame, size_t packet_size, int mean_ms,
                      int standard_deviation_ms, int drift_per_frame_ms) {
    // Simulate a higher send pace, that is too high.
    for (int i = 0; i < 1000; ++i) {
      for (int j = 0; j < packets_per_frame; ++j) {
        UpdateDetector(rtp_timestamp_, receive_time_ms_, packet_size);
      }
      rtp_timestamp_ += mean_ms * 90;
      now_ms_ += mean_ms + drift_per_frame_ms;
      receive_time_ms_ = std::max<int64_t>(
          receive_time_ms_,
          now_ms_ + static_cast<int64_t>(
                        random_.Gaussian(0, standard_deviation_ms) + 0.5));
      if (BandwidthUsage::kBwOverusing == overuse_detector_->State()) {
        return i + 1;
      }
    }
    return -1;
  }

  void UpdateDetector(uint32_t rtp_timestamp, int64_t receive_time_ms,
                      size_t packet_size) {
    uint32_t timestamp_delta;
    int64_t time_delta;
    int size_delta;
    if (inter_arrival_->ComputeDeltas(
            rtp_timestamp, receive_time_ms, receive_time_ms, packet_size,
            &timestamp_delta, &time_delta, &size_delta)) {
      double timestamp_delta_ms = timestamp_delta / 90.0;
      overuse_estimator_->Update(time_delta, timestamp_delta_ms, size_delta,
                                 overuse_detector_->State(), receive_time_ms);
      overuse_detector_->Detect(
          overuse_estimator_->offset(), timestamp_delta_ms,
          overuse_estimator_->num_of_deltas(), receive_time_ms);
    }
  }

  int64_t now_ms_;
  int64_t receive_time_ms_;
  uint32_t rtp_timestamp_;
  OverUseDetectorOptions options_;
  std::unique_ptr<OveruseDetector> overuse_detector_;
  std::unique_ptr<OveruseEstimator> overuse_estimator_;
  std::unique_ptr<InterArrival> inter_arrival_;
  Random random_;
};

TEST_F(OveruseDetectorTest, GaussianRandom) {
  int buckets[100];
  memset(buckets, 0, sizeof(buckets));
  for (int i = 0; i < 100000; ++i) {
    int index = random_.Gaussian(49, 10);
    if (index >= 0 && index < 100)
      buckets[index]++;
  }
  for (int n = 0; n < 100; ++n) {
    printf("Bucket n:%d, %d\n", n, buckets[n]);
  }
}

TEST_F(OveruseDetectorTest, SimpleNonOveruse30fps) {
  size_t packet_size = 1200;
  uint32_t frame_duration_ms = 33;
  uint32_t rtp_timestamp = 10 * 90;

  // No variance.
  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    now_ms_ += frame_duration_ms;
    rtp_timestamp += frame_duration_ms * 90;
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
}

// Roughly 1 Mbit/s
TEST_F(OveruseDetectorTest, SimpleNonOveruseWithReceiveVariance) {
  uint32_t frame_duration_ms = 10;
  uint32_t rtp_timestamp = 10 * 90;
  size_t packet_size = 1200;

  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    rtp_timestamp += frame_duration_ms * 90;
    if (i % 2) {
      now_ms_ += frame_duration_ms - 5;
    } else {
      now_ms_ += frame_duration_ms + 5;
    }
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
}

TEST_F(OveruseDetectorTest, SimpleNonOveruseWithRtpTimestampVariance) {
  // Roughly 1 Mbit/s.
  uint32_t frame_duration_ms = 10;
  uint32_t rtp_timestamp = 10 * 90;
  size_t packet_size = 1200;

  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    now_ms_ += frame_duration_ms;
    if (i % 2) {
      rtp_timestamp += (frame_duration_ms - 5) * 90;
    } else {
      rtp_timestamp += (frame_duration_ms + 5) * 90;
    }
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
}

TEST_F(OveruseDetectorTest, SimpleOveruse2000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 6;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 0;  // No variance.
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);

  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(7, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, SimpleOveruse100kbit10fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 100;
  int drift_per_frame_ms = 1;
  int sigma_ms = 0;  // No variance.
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);

  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(7, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, DISABLED_OveruseWithHighVariance100Kbit10fps) {
  uint32_t frame_duration_ms = 100;
  uint32_t drift_per_frame_ms = 10;
  uint32_t rtp_timestamp = frame_duration_ms * 90;
  size_t packet_size = 1200;
  int offset = 10;

  // Run 1000 samples to reach steady state.
  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    rtp_timestamp += frame_duration_ms * 90;
    if (i % 2) {
      offset = random_.Rand(0, 49);
      now_ms_ += frame_duration_ms - offset;
    } else {
      now_ms_ += frame_duration_ms + offset;
    }
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  // Simulate a higher send pace, that is too high.
  // Above noise generate a standard deviation of approximately 28 ms.
  // Total build up of 150 ms.
  for (int j = 0; j < 15; ++j) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    now_ms_ += frame_duration_ms + drift_per_frame_ms;
    rtp_timestamp += frame_duration_ms * 90;
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  UpdateDetector(rtp_timestamp, now_ms_, packet_size);
  EXPECT_EQ(BandwidthUsage::kBwOverusing, overuse_detector_->State());
}

TEST_F(OveruseDetectorTest, DISABLED_OveruseWithLowVariance100Kbit10fps) {
  uint32_t frame_duration_ms = 100;
  uint32_t drift_per_frame_ms = 1;
  uint32_t rtp_timestamp = frame_duration_ms * 90;
  size_t packet_size = 1200;
  int offset = 10;

  // Run 1000 samples to reach steady state.
  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    rtp_timestamp += frame_duration_ms * 90;
    if (i % 2) {
      offset = random_.Rand(0, 1);
      now_ms_ += frame_duration_ms - offset;
    } else {
      now_ms_ += frame_duration_ms + offset;
    }
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  // Simulate a higher send pace, that is too high.
  // Total build up of 6 ms.
  for (int j = 0; j < 6; ++j) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    now_ms_ += frame_duration_ms + drift_per_frame_ms;
    rtp_timestamp += frame_duration_ms * 90;
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  UpdateDetector(rtp_timestamp, now_ms_, packet_size);
  EXPECT_EQ(BandwidthUsage::kBwOverusing, overuse_detector_->State());
}

TEST_F(OveruseDetectorTest, OveruseWithLowVariance2000Kbit30fps) {
  uint32_t frame_duration_ms = 33;
  uint32_t drift_per_frame_ms = 1;
  uint32_t rtp_timestamp = frame_duration_ms * 90;
  size_t packet_size = 1200;
  int offset = 0;

  // Run 1000 samples to reach steady state.
  for (int i = 0; i < 1000; ++i) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    rtp_timestamp += frame_duration_ms * 90;
    if (i % 2) {
      offset = random_.Rand(0, 1);
      now_ms_ += frame_duration_ms - offset;
    } else {
      now_ms_ += frame_duration_ms + offset;
    }
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  // Simulate a higher send pace, that is too high.
  // Total build up of 30 ms.
  for (int j = 0; j < 3; ++j) {
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    UpdateDetector(rtp_timestamp, now_ms_, packet_size);
    now_ms_ += frame_duration_ms + drift_per_frame_ms * 6;
    rtp_timestamp += frame_duration_ms * 90;
    EXPECT_EQ(BandwidthUsage::kBwNormal, overuse_detector_->State());
  }
  UpdateDetector(rtp_timestamp, now_ms_, packet_size);
  EXPECT_EQ(BandwidthUsage::kBwOverusing, overuse_detector_->State());
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance30Kbit3fps \
  DISABLED_LowGaussianVariance30Kbit3fps
#else
#define MAYBE_LowGaussianVariance30Kbit3fps LowGaussianVariance30Kbit3fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance30Kbit3fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 333;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(20, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, LowGaussianVarianceFastDrift30Kbit3fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 333;
  int drift_per_frame_ms = 100;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(4, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVariance30Kbit3fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 333;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVarianceFastDrift30Kbit3fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 333;
  int drift_per_frame_ms = 100;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(4, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance100Kbit5fps \
  DISABLED_LowGaussianVariance100Kbit5fps
#else
#define MAYBE_LowGaussianVariance100Kbit5fps LowGaussianVariance100Kbit5fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance100Kbit5fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 2;
  int frame_duration_ms = 200;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(20, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_HighGaussianVariance100Kbit5fps \
  DISABLED_HighGaussianVariance100Kbit5fps
#else
#define MAYBE_HighGaussianVariance100Kbit5fps HighGaussianVariance100Kbit5fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_HighGaussianVariance100Kbit5fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 2;
  int frame_duration_ms = 200;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance100Kbit10fps \
  DISABLED_LowGaussianVariance100Kbit10fps
#else
#define MAYBE_LowGaussianVariance100Kbit10fps LowGaussianVariance100Kbit10fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance100Kbit10fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 100;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(20, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_HighGaussianVariance100Kbit10fps \
  DISABLED_HighGaussianVariance100Kbit10fps
#else
#define MAYBE_HighGaussianVariance100Kbit10fps HighGaussianVariance100Kbit10fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_HighGaussianVariance100Kbit10fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 100;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance300Kbit30fps \
  DISABLED_LowGaussianVariance300Kbit30fps
#else
#define MAYBE_LowGaussianVariance300Kbit30fps LowGaussianVariance300Kbit30fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance300Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(19, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, LowGaussianVarianceFastDrift300Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(5, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVariance300Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVarianceFastDrift300Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 1;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(10, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance1000Kbit30fps \
  DISABLED_LowGaussianVariance1000Kbit30fps
#else
#define MAYBE_LowGaussianVariance1000Kbit30fps LowGaussianVariance1000Kbit30fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance1000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 3;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(19, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, LowGaussianVarianceFastDrift1000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 3;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(5, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVariance1000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 3;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVarianceFastDrift1000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 3;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(10, frames_until_overuse);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LowGaussianVariance2000Kbit30fps \
  DISABLED_LowGaussianVariance2000Kbit30fps
#else
#define MAYBE_LowGaussianVariance2000Kbit30fps LowGaussianVariance2000Kbit30fps
#endif
TEST_F(OveruseDetectorTest, MAYBE_LowGaussianVariance2000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 6;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(19, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, LowGaussianVarianceFastDrift2000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 6;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 3;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(5, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVariance2000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 6;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 1;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(44, frames_until_overuse);
}

TEST_F(OveruseDetectorTest, HighGaussianVarianceFastDrift2000Kbit30fps) {
  size_t packet_size = 1200;
  int packets_per_frame = 6;
  int frame_duration_ms = 33;
  int drift_per_frame_ms = 10;
  int sigma_ms = 10;
  int unique_overuse = Run100000Samples(packets_per_frame, packet_size,
                                        frame_duration_ms, sigma_ms);
  EXPECT_EQ(0, unique_overuse);
  int frames_until_overuse = RunUntilOveruse(packets_per_frame, packet_size,
      frame_duration_ms, sigma_ms, drift_per_frame_ms);
  EXPECT_EQ(10, frames_until_overuse);
}

class OveruseDetectorExperimentTest : public OveruseDetectorTest {
 public:
  OveruseDetectorExperimentTest()
      : override_field_trials_(
            "WebRTC-AdaptiveBweThreshold/Enabled-0.01,0.00018/") {}

 protected:
  void SetUp() override {
    overuse_detector_.reset(new OveruseDetector());
  }

  test::ScopedFieldTrials override_field_trials_;
};

TEST_F(OveruseDetectorExperimentTest, ThresholdAdapts) {
  const double kOffset = 0.21;
  double kTsDelta = 3000.0;
  int64_t now_ms = 0;
  int num_deltas = 60;
  const int kBatchLength = 10;

  // Pass in a positive offset and verify it triggers overuse.
  bool overuse_detected = false;
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_TRUE(overuse_detected);

  // Force the threshold to increase by passing in a higher offset.
  overuse_detected = false;
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(1.1 * kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_TRUE(overuse_detected);

  // Verify that the same offset as before no longer triggers overuse.
  overuse_detected = false;
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_FALSE(overuse_detected);

  // Pass in a low offset to make the threshold adapt down.
  for (int i = 0; i < 15 * kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(0.7 * kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_FALSE(overuse_detected);

  // Make sure the original offset now again triggers overuse.
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_TRUE(overuse_detected);
}

TEST_F(OveruseDetectorExperimentTest, DoesntAdaptToSpikes) {
  const double kOffset = 1.0;
  const double kLargeOffset = 20.0;
  double kTsDelta = 3000.0;
  int64_t now_ms = 0;
  int num_deltas = 60;
  const int kBatchLength = 10;
  const int kShortBatchLength = 3;

  // Pass in a positive offset and verify it triggers overuse.
  bool overuse_detected = false;
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }

  // Pass in a large offset. This shouldn't have a too big impact on the
  // threshold, but still trigger an overuse.
  now_ms += 100;
  overuse_detected = false;
  for (int i = 0; i < kShortBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kLargeOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_TRUE(overuse_detected);

  // Pass in a positive normal offset and verify it still triggers.
  overuse_detected = false;
  for (int i = 0; i < kBatchLength; ++i) {
    BandwidthUsage overuse_state =
        overuse_detector_->Detect(kOffset, kTsDelta, num_deltas, now_ms);
    if (overuse_state == BandwidthUsage::kBwOverusing) {
      overuse_detected = true;
    }
    ++num_deltas;
    now_ms += 5;
  }
  EXPECT_TRUE(overuse_detected);
}
}  // namespace testing
}  // namespace webrtc
