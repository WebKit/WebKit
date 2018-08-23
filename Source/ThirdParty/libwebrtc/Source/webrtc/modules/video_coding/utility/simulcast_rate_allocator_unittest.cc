/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/simulcast_rate_allocator.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "modules/video_coding/codecs/vp8/temporal_layers.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::_;

constexpr uint32_t kFramerateFps = 5;
constexpr uint32_t kMinBitrateKbps = 50;
// These correspond to kLegacyScreenshareTl(0|1)BitrateKbps in cc.
constexpr uint32_t kTargetBitrateKbps = 200;
constexpr uint32_t kMaxBitrateKbps = 1000;

class MockTemporalLayers : public TemporalLayers {
 public:
  MOCK_METHOD1(UpdateLayerConfig, TemporalLayers::FrameConfig(uint32_t));
  MOCK_METHOD2(OnRatesUpdated, void(const std::vector<uint32_t>&, int));
  MOCK_METHOD1(UpdateConfiguration, bool(Vp8EncoderConfig*));
  MOCK_METHOD4(PopulateCodecSpecific,
               void(bool,
                    const TemporalLayers::FrameConfig&,
                    CodecSpecificInfoVP8*,
                    uint32_t));
  MOCK_METHOD3(FrameEncoded, void(uint32_t, size_t, int));
  MOCK_CONST_METHOD0(Tl0PicIdx, uint8_t());
  MOCK_CONST_METHOD1(GetTemporalLayerId,
                     int(const TemporalLayers::FrameConfig&));
};
}  // namespace

class SimulcastRateAllocatorTest : public ::testing::TestWithParam<bool> {
 public:
  SimulcastRateAllocatorTest() {
    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.codecType = kVideoCodecVP8;
    codec_.minBitrate = kMinBitrateKbps;
    codec_.maxBitrate = kMaxBitrateKbps;
    codec_.active = true;
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

  template <size_t S>
  void ExpectEqual(uint32_t (&expected)[S],
                   const VideoBitrateAllocation& actual) {
    // EXPECT_EQ(S, actual.size());
    uint32_t sum = 0;
    for (size_t i = 0; i < S; ++i) {
      uint32_t layer_bitrate = actual.GetSpatialLayerSum(i);
      if (layer_bitrate == 0) {
        EXPECT_FALSE(actual.IsSpatialLayerUsed(i));
      }
      EXPECT_EQ(expected[i] * 1000U, layer_bitrate)
          << "Mismatch at index " << i;
      sum += layer_bitrate;
    }
    EXPECT_EQ(sum, actual.get_sum_bps());
  }

  void CreateAllocator() {
    allocator_.reset(new SimulcastRateAllocator(codec_));
  }

  void SetupCodecThreeSimulcastStreams(
      const std::vector<bool>& active_streams) {
    size_t num_streams = 3;
    RTC_DCHECK_GE(active_streams.size(), num_streams);
    SetupCodecTwoSimulcastStreams(active_streams);
    codec_.numberOfSimulcastStreams = num_streams;
    codec_.simulcastStream[2].minBitrate = 2000;
    codec_.simulcastStream[2].targetBitrate = 3000;
    codec_.simulcastStream[2].maxBitrate = 4000;
    codec_.simulcastStream[2].active = active_streams[2];
  }

  void SetupCodecTwoSimulcastStreams(const std::vector<bool>& active_streams) {
    size_t num_streams = 2;
    RTC_DCHECK_GE(active_streams.size(), num_streams);
    codec_.numberOfSimulcastStreams = num_streams;
    codec_.maxBitrate = 0;
    codec_.simulcastStream[0].minBitrate = 10;
    codec_.simulcastStream[0].targetBitrate = 100;
    codec_.simulcastStream[0].maxBitrate = 500;
    codec_.simulcastStream[1].minBitrate = 50;
    codec_.simulcastStream[1].targetBitrate = 500;
    codec_.simulcastStream[1].maxBitrate = 1000;
    for (size_t i = 0; i < num_streams; ++i) {
      codec_.simulcastStream[i].active = active_streams[i];
    }
  }

  VideoBitrateAllocation GetAllocation(uint32_t target_bitrate) {
    return allocator_->GetAllocation(target_bitrate * 1000U, kDefaultFrameRate);
  }

 protected:
  static const int kDefaultFrameRate = 30;
  VideoCodec codec_;
  std::unique_ptr<SimulcastRateAllocator> allocator_;
};

TEST_F(SimulcastRateAllocatorTest, NoSimulcastBelowMin) {
  uint32_t expected[] = {codec_.minBitrate};
  codec_.active = true;
  ExpectEqual(expected, GetAllocation(codec_.minBitrate - 1));
  ExpectEqual(expected, GetAllocation(1));
  ExpectEqual(expected, GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastAboveMax) {
  uint32_t expected[] = {codec_.maxBitrate};
  codec_.active = true;
  ExpectEqual(expected, GetAllocation(codec_.maxBitrate + 1));
  ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastNoMax) {
  const uint32_t kMax = VideoBitrateAllocation::kMaxBitrateBps / 1000;
  codec_.active = true;
  codec_.maxBitrate = 0;
  CreateAllocator();

  uint32_t expected[] = {kMax};
  ExpectEqual(expected, GetAllocation(kMax));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastWithinLimits) {
  codec_.active = true;
  for (uint32_t bitrate = codec_.minBitrate; bitrate <= codec_.maxBitrate;
       ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

// Tests that when we aren't using simulcast and the codec is marked inactive no
// bitrate will be allocated.
TEST_F(SimulcastRateAllocatorTest, NoSimulcastInactive) {
  codec_.active = false;
  uint32_t expected[] = {0};
  CreateAllocator();

  ExpectEqual(expected, GetAllocation(kMinBitrateKbps - 10));
  ExpectEqual(expected, GetAllocation(kTargetBitrateKbps));
  ExpectEqual(expected, GetAllocation(kMaxBitrateKbps + 10));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastBelowMin) {
  // With simulcast, use the min bitrate from the ss spec instead of the global.
  codec_.numberOfSimulcastStreams = 1;
  const uint32_t kMin = codec_.minBitrate - 10;
  codec_.simulcastStream[0].minBitrate = kMin;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
  codec_.simulcastStream[0].active = true;
  CreateAllocator();

  uint32_t expected[] = {kMin};
  ExpectEqual(expected, GetAllocation(kMin - 1));
  ExpectEqual(expected, GetAllocation(1));
  ExpectEqual(expected, GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastAboveMax) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
  const uint32_t kMax = codec_.simulcastStream[0].maxBitrate + 1000;
  codec_.simulcastStream[0].maxBitrate = kMax;
  codec_.simulcastStream[0].active = true;
  CreateAllocator();

  uint32_t expected[] = {kMax};
  ExpectEqual(expected, GetAllocation(kMax));
  ExpectEqual(expected, GetAllocation(kMax + 1));
  ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastWithinLimits) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
  codec_.simulcastStream[0].maxBitrate = kMaxBitrateKbps;
  codec_.simulcastStream[0].active = true;
  CreateAllocator();

  for (uint32_t bitrate = kMinBitrateKbps; bitrate <= kMaxBitrateKbps;
       ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastInactive) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
  codec_.simulcastStream[0].maxBitrate = kMaxBitrateKbps;
  codec_.simulcastStream[0].active = false;
  CreateAllocator();

  uint32_t expected[] = {0};
  ExpectEqual(expected, GetAllocation(kMinBitrateKbps - 10));
  ExpectEqual(expected, GetAllocation(kTargetBitrateKbps));
  ExpectEqual(expected, GetAllocation(kMaxBitrateKbps + 10));
}

TEST_F(SimulcastRateAllocatorTest, OneToThreeStreams) {
  const std::vector<bool> active_streams(3, true);
  SetupCodecThreeSimulcastStreams(active_streams);
  CreateAllocator();

  {
    // Single stream, min bitrate.
    const uint32_t bitrate = codec_.simulcastStream[0].minBitrate;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Single stream at target bitrate.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Bitrate above target for first stream, but below min for the next one.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].minBitrate - 1;
    uint32_t expected[] = {bitrate, 0, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Just enough for two streams.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].minBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].minBitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Second stream maxed out, but not enough for third.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].maxBitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // First two streams maxed out, but not enough for third. Nowhere to put
    // remaining bits.
    const uint32_t bitrate = codec_.simulcastStream[0].maxBitrate +
                             codec_.simulcastStream[1].maxBitrate + 499;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].maxBitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Just enough for all three streams.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].targetBitrate +
                             codec_.simulcastStream[2].minBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].targetBitrate,
                           codec_.simulcastStream[2].minBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Third maxed out.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].targetBitrate +
                             codec_.simulcastStream[2].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].targetBitrate,
                           codec_.simulcastStream[2].maxBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Enough to max out all streams which will allocate the target amount to
    // the lower streams.
    const uint32_t bitrate = codec_.simulcastStream[0].maxBitrate +
                             codec_.simulcastStream[1].maxBitrate +
                             codec_.simulcastStream[2].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].targetBitrate,
                           codec_.simulcastStream[2].maxBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

// If three simulcast streams that are all inactive, none of them should be
// allocated bitrate.
TEST_F(SimulcastRateAllocatorTest, ThreeStreamsInactive) {
  const std::vector<bool> active_streams(3, false);
  SetupCodecThreeSimulcastStreams(active_streams);
  CreateAllocator();

  // Just enough to allocate the min.
  const uint32_t min_bitrate = codec_.simulcastStream[0].minBitrate +
                               codec_.simulcastStream[1].minBitrate +
                               codec_.simulcastStream[2].minBitrate;
  // Enough bitrate to allocate target to all streams.
  const uint32_t target_bitrate = codec_.simulcastStream[0].targetBitrate +
                                  codec_.simulcastStream[1].targetBitrate +
                                  codec_.simulcastStream[2].targetBitrate;
  // Enough bitrate to allocate max to all streams.
  const uint32_t max_bitrate = codec_.simulcastStream[0].maxBitrate +
                               codec_.simulcastStream[1].maxBitrate +
                               codec_.simulcastStream[2].maxBitrate;
  uint32_t expected[] = {0, 0, 0};
  ExpectEqual(expected, GetAllocation(0));
  ExpectEqual(expected, GetAllocation(min_bitrate));
  ExpectEqual(expected, GetAllocation(target_bitrate));
  ExpectEqual(expected, GetAllocation(max_bitrate));
}

// If there are two simulcast streams, we expect the high active stream to be
// allocated as if it is a single active stream.
TEST_F(SimulcastRateAllocatorTest, TwoStreamsLowInactive) {
  const std::vector<bool> active_streams({false, true});
  SetupCodecTwoSimulcastStreams(active_streams);
  CreateAllocator();

  const uint32_t kActiveStreamMinBitrate = codec_.simulcastStream[1].minBitrate;
  const uint32_t kActiveStreamTargetBitrate =
      codec_.simulcastStream[1].targetBitrate;
  const uint32_t kActiveStreamMaxBitrate = codec_.simulcastStream[1].maxBitrate;
  {
    // Expect that the stream is always allocated its min bitrate.
    uint32_t expected[] = {0, kActiveStreamMinBitrate};
    ExpectEqual(expected, GetAllocation(0));
    ExpectEqual(expected, GetAllocation(kActiveStreamMinBitrate - 10));
    ExpectEqual(expected, GetAllocation(kActiveStreamMinBitrate));
  }

  {
    // The stream should be allocated its target bitrate.
    uint32_t expected[] = {0, kActiveStreamTargetBitrate};
    ExpectEqual(expected, GetAllocation(kActiveStreamTargetBitrate));
  }

  {
    // The stream should be allocated its max if the target input is sufficient.
    uint32_t expected[] = {0, kActiveStreamMaxBitrate};
    ExpectEqual(expected, GetAllocation(kActiveStreamMaxBitrate));
    ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
  }
}

// If there are two simulcast streams, we expect the low active stream to be
// allocated as if it is a single active stream.
TEST_F(SimulcastRateAllocatorTest, TwoStreamsHighInactive) {
  const std::vector<bool> active_streams({true, false});
  SetupCodecTwoSimulcastStreams(active_streams);
  CreateAllocator();

  const uint32_t kActiveStreamMinBitrate = codec_.simulcastStream[0].minBitrate;
  const uint32_t kActiveStreamTargetBitrate =
      codec_.simulcastStream[0].targetBitrate;
  const uint32_t kActiveStreamMaxBitrate = codec_.simulcastStream[0].maxBitrate;
  {
    // Expect that the stream is always allocated its min bitrate.
    uint32_t expected[] = {kActiveStreamMinBitrate, 0};
    ExpectEqual(expected, GetAllocation(0));
    ExpectEqual(expected, GetAllocation(kActiveStreamMinBitrate - 10));
    ExpectEqual(expected, GetAllocation(kActiveStreamMinBitrate));
  }

  {
    // The stream should be allocated its target bitrate.
    uint32_t expected[] = {kActiveStreamTargetBitrate, 0};
    ExpectEqual(expected, GetAllocation(kActiveStreamTargetBitrate));
  }

  {
    // The stream should be allocated its max if the target input is sufficent.
    uint32_t expected[] = {kActiveStreamMaxBitrate, 0};
    ExpectEqual(expected, GetAllocation(kActiveStreamMaxBitrate));
    ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
  }
}

// If there are three simulcast streams and the middle stream is inactive, the
// other two streams should be allocated bitrate the same as if they are two
// active simulcast streams.
TEST_F(SimulcastRateAllocatorTest, ThreeStreamsMiddleInactive) {
  const std::vector<bool> active_streams({true, false, true});
  SetupCodecThreeSimulcastStreams(active_streams);
  CreateAllocator();

  {
    const uint32_t kLowStreamMinBitrate = codec_.simulcastStream[0].minBitrate;
    // The lowest stream should always be allocated its minimum bitrate.
    uint32_t expected[] = {kLowStreamMinBitrate, 0, 0};
    ExpectEqual(expected, GetAllocation(0));
    ExpectEqual(expected, GetAllocation(kLowStreamMinBitrate - 10));
    ExpectEqual(expected, GetAllocation(kLowStreamMinBitrate));
  }

  {
    // The lowest stream gets its target bitrate.
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate, 0, 0};
    ExpectEqual(expected,
                GetAllocation(codec_.simulcastStream[0].targetBitrate));
  }

  {
    // The lowest stream gets its max bitrate, but not enough for the high
    // stream.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[2].minBitrate - 1;
    uint32_t expected[] = {codec_.simulcastStream[0].maxBitrate, 0, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Both active streams get allocated target bitrate.
    const uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[2].targetBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate, 0,
                           codec_.simulcastStream[2].targetBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Lowest stream gets its target bitrate, high stream gets its max bitrate.
    uint32_t bitrate = codec_.simulcastStream[0].targetBitrate +
                       codec_.simulcastStream[2].maxBitrate;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate, 0,
                           codec_.simulcastStream[2].maxBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
    ExpectEqual(expected, GetAllocation(bitrate + 10));
    ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
  }
}

class ScreenshareRateAllocationTest : public SimulcastRateAllocatorTest {
 public:
  void SetupConferenceScreenshare(bool use_simulcast, bool active = true) {
    codec_.mode = VideoCodecMode::kScreensharing;
    codec_.minBitrate = kMinBitrateKbps;
    codec_.maxBitrate = kMaxBitrateKbps;
    if (use_simulcast) {
      codec_.numberOfSimulcastStreams = 1;
      codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
      codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
      codec_.simulcastStream[0].maxBitrate = kMaxBitrateKbps;
      codec_.simulcastStream[0].numberOfTemporalLayers = 2;
      codec_.simulcastStream[0].active = active;
    } else {
      codec_.numberOfSimulcastStreams = 0;
      codec_.VP8()->numberOfTemporalLayers = 2;
      codec_.active = active;
    }
  }
};

INSTANTIATE_TEST_CASE_P(ScreenshareTest,
                        ScreenshareRateAllocationTest,
                        ::testing::Bool());

TEST_P(ScreenshareRateAllocationTest, BitrateBelowTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  VideoBitrateAllocation allocation =
      allocator_->GetAllocation(kTargetBitrateKbps * 1000, kFramerateFps);

  // All allocation should go in TL0.
  EXPECT_EQ(kTargetBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kTargetBitrateKbps, allocation.GetBitrate(0, 0) / 1000);
}

TEST_P(ScreenshareRateAllocationTest, BitrateAboveTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  uint32_t target_bitrate_kbps = (kTargetBitrateKbps + kMaxBitrateKbps) / 2;
  VideoBitrateAllocation allocation =
      allocator_->GetAllocation(target_bitrate_kbps * 1000, kFramerateFps);

  // Fill TL0, then put the rest in TL1.
  EXPECT_EQ(target_bitrate_kbps, allocation.get_sum_kbps());
  EXPECT_EQ(kTargetBitrateKbps, allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(target_bitrate_kbps - kTargetBitrateKbps,
            allocation.GetBitrate(0, 1) / 1000);
}

TEST_P(ScreenshareRateAllocationTest, BitrateAboveTl1) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  VideoBitrateAllocation allocation =
      allocator_->GetAllocation(kMaxBitrateKbps * 2000, kFramerateFps);

  // Fill both TL0 and TL1, but no more.
  EXPECT_EQ(kMaxBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kTargetBitrateKbps, allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(kMaxBitrateKbps - kTargetBitrateKbps,
            allocation.GetBitrate(0, 1) / 1000);
}

// This tests when the screenshare is inactive it should be allocated 0 bitrate
// for all layers.
TEST_P(ScreenshareRateAllocationTest, InactiveScreenshare) {
  SetupConferenceScreenshare(GetParam(), false);
  CreateAllocator();

  // Enough bitrate for TL0 and TL1.
  uint32_t target_bitrate_kbps = (kTargetBitrateKbps + kMaxBitrateKbps) / 2;
  VideoBitrateAllocation allocation =
      allocator_->GetAllocation(target_bitrate_kbps * 1000, kFramerateFps);

  EXPECT_EQ(0U, allocation.get_sum_kbps());
}
}  // namespace webrtc
