/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"

#include <limits>
#include <memory>

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
constexpr uint32_t kMinBitrate = 50;
constexpr uint32_t kTargetBitrate = 100;
constexpr uint32_t kMaxBitrate = 1000;
}  // namespace

class SimulcastRateAllocatorTest : public ::testing::Test {
 public:
  SimulcastRateAllocatorTest() {
    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.minBitrate = kMinBitrate;
    codec_.targetBitrate = kTargetBitrate;
    codec_.maxBitrate = kMaxBitrate;
    CreateAllocator();
  }
  virtual ~SimulcastRateAllocatorTest() {}

  template <size_t S>
  void ExpectEqual(uint32_t (&expected)[S],
                   const std::vector<uint32_t>& actual) {
    EXPECT_EQ(S, actual.size());
    for (size_t i = 0; i < S; ++i)
      EXPECT_EQ(expected[i], actual[i]) << "Mismatch at index " << i;
  }

  void CreateAllocator() {
    allocator_.reset(new SimulcastRateAllocator(codec_));
  }

 protected:
  VideoCodec codec_;
  std::unique_ptr<SimulcastRateAllocator> allocator_;
};

TEST_F(SimulcastRateAllocatorTest, NoSimulcastBelowMin) {
  uint32_t expected[] = {codec_.minBitrate};
  ExpectEqual(expected, allocator_->GetAllocation(codec_.minBitrate - 1));
  ExpectEqual(expected, allocator_->GetAllocation(1));
  ExpectEqual(expected, allocator_->GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastAboveMax) {
  uint32_t expected[] = {codec_.maxBitrate};
  ExpectEqual(expected, allocator_->GetAllocation(codec_.maxBitrate + 1));
  ExpectEqual(expected,
              allocator_->GetAllocation(std::numeric_limits<uint32_t>::max()));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastNoMax) {
  constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
  codec_.maxBitrate = 0;
  CreateAllocator();

  uint32_t expected[] = {kMax};
  ExpectEqual(expected, allocator_->GetAllocation(kMax));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastWithinLimits) {
  for (uint32_t bitrate = codec_.minBitrate; bitrate <= codec_.maxBitrate;
       ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastBelowMin) {
  // With simulcast, use the min bitrate from the ss spec instead of the global.
  codec_.numberOfSimulcastStreams = 1;
  const uint32_t kMin = codec_.minBitrate - 10;
  codec_.simulcastStream[0].minBitrate = kMin;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrate;
  CreateAllocator();

  uint32_t expected[] = {kMin};
  ExpectEqual(expected, allocator_->GetAllocation(kMin - 1));
  ExpectEqual(expected, allocator_->GetAllocation(1));
  ExpectEqual(expected, allocator_->GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastAboveMax) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrate;
  const uint32_t kMax = codec_.simulcastStream[0].maxBitrate + 1000;
  codec_.simulcastStream[0].maxBitrate = kMax;
  CreateAllocator();

  uint32_t expected[] = {kMax};
  ExpectEqual(expected, allocator_->GetAllocation(kMax + 1));
  ExpectEqual(expected,
              allocator_->GetAllocation(std::numeric_limits<uint32_t>::max()));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastWithinLimits) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrate;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrate;
  codec_.simulcastStream[0].maxBitrate = kMaxBitrate;
  CreateAllocator();

  for (uint32_t bitrate = kMinBitrate; bitrate <= kMaxBitrate; ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, OneToThreeStreams) {
  codec_.numberOfSimulcastStreams = 3;
  codec_.maxBitrate = 0;
  codec_.simulcastStream[0].minBitrate = 10;
  codec_.simulcastStream[0].targetBitrate = 100;
  codec_.simulcastStream[0].maxBitrate = 500;
  codec_.simulcastStream[1].minBitrate = 50;
  codec_.simulcastStream[1].targetBitrate = 500;
  codec_.simulcastStream[1].maxBitrate = 1000;
  codec_.simulcastStream[2].minBitrate = 2000;
  codec_.simulcastStream[2].targetBitrate = 3000;
  codec_.simulcastStream[2].maxBitrate = 4000;
  CreateAllocator();

  {
    // Single stream, min bitrate.
    const uint32_t bitrate = codec_.simulcastStream[0].minBitrate;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Single stream at target bitrate.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Bitrate above target for first stream, but below min for the next one.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].minBitrate - 1;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Just enough for two streams.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].minBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].minBitrate, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Second stream maxed out, but not enough for third.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].maxBitrate, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // First two streams maxed out, but not enough for third. Nowhere to put
    // remaining bits.
    const uint32_t bitrate = codec_.simulcastStream[0].maxBitrate +
                             codec_.simulcastStream[1].maxBitrate + 499;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].maxBitrate, 0};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Just enough for all three streams.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].targetBitrate +
                             codec_.simulcastStream[2].minBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].targetBitrate,
                           codec_.simulcastStream[2].minBitrate};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }

  {
    // Third maxed out.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].targetBitrate +
                             codec_.simulcastStream[2].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].targetBitrate,
                           codec_.simulcastStream[2].maxBitrate};
    ExpectEqual(expected, allocator_->GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, GetPreferredBitrate) {
  EXPECT_EQ(codec_.maxBitrate, allocator_->GetPreferedBitrate());
}

TEST_F(SimulcastRateAllocatorTest, GetPreferredBitrateSimulcast) {
  codec_.numberOfSimulcastStreams = 3;
  codec_.maxBitrate = 999999;
  codec_.simulcastStream[0].minBitrate = 10;
  codec_.simulcastStream[0].targetBitrate = 100;

  codec_.simulcastStream[0].maxBitrate = 500;
  codec_.simulcastStream[1].minBitrate = 50;
  codec_.simulcastStream[1].targetBitrate = 500;
  codec_.simulcastStream[1].maxBitrate = 1000;

  codec_.simulcastStream[2].minBitrate = 2000;
  codec_.simulcastStream[2].targetBitrate = 3000;
  codec_.simulcastStream[2].maxBitrate = 4000;
  CreateAllocator();

  uint32_t preferred_bitrate;
  preferred_bitrate = codec_.simulcastStream[0].targetBitrate;
  preferred_bitrate += codec_.simulcastStream[1].targetBitrate;
  preferred_bitrate += codec_.simulcastStream[2].maxBitrate;

  EXPECT_EQ(preferred_bitrate, allocator_->GetPreferedBitrate());
}

}  // namespace webrtc
