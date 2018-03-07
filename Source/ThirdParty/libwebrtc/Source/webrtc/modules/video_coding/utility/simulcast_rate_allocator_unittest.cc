/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::_;

constexpr uint32_t kMinBitrateKbps = 50;
constexpr uint32_t kTargetBitrateKbps = 100;
constexpr uint32_t kMaxBitrateKbps = 1000;
constexpr uint32_t kFramerateFps = 5;

class MockTemporalLayers : public TemporalLayers {
 public:
  MOCK_METHOD1(UpdateLayerConfig, TemporalLayers::FrameConfig(uint32_t));
  MOCK_METHOD3(OnRatesUpdated, std::vector<uint32_t>(int, int, int));
  MOCK_METHOD1(UpdateConfiguration, bool(vpx_codec_enc_cfg_t*));
  MOCK_METHOD4(PopulateCodecSpecific,
               void(bool,
                    const TemporalLayers::FrameConfig&,
                    CodecSpecificInfoVP8*,
                    uint32_t));
  MOCK_METHOD2(FrameEncoded, void(unsigned int, int));
  MOCK_CONST_METHOD0(Tl0PicIdx, uint8_t());
  MOCK_CONST_METHOD1(GetTemporalLayerId,
                     int(const TemporalLayers::FrameConfig&));
};
}  // namespace

class SimulcastRateAllocatorTest : public ::testing::TestWithParam<bool> {
 public:
  SimulcastRateAllocatorTest() {
    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.minBitrate = kMinBitrateKbps;
    codec_.targetBitrate = kTargetBitrateKbps;
    codec_.maxBitrate = kMaxBitrateKbps;
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
  void ExpectEqual(uint32_t (&expected)[S], const BitrateAllocation& actual) {
    // EXPECT_EQ(S, actual.size());
    uint32_t sum = 0;
    for (size_t i = 0; i < S; ++i) {
      uint32_t layer_bitrate = actual.GetSpatialLayerSum(i);
      EXPECT_EQ(expected[i] * 1000U, layer_bitrate) << "Mismatch at index "
                                                    << i;
      sum += layer_bitrate;
    }
    EXPECT_EQ(sum, actual.get_sum_bps());
  }

  void CreateAllocator() {
    std::unique_ptr<TemporalLayersFactory> tl_factory(GetTlFactory());
    codec_.VP8()->tl_factory = tl_factory.get();
    allocator_.reset(new SimulcastRateAllocator(codec_, std::move(tl_factory)));

    // Simulate InitEncode().
    tl_factories_.clear();
    if (codec_.numberOfSimulcastStreams == 0) {
      tl_factories_.push_back(
          std::unique_ptr<TemporalLayers>(codec_.VP8()->tl_factory->Create(
              0, codec_.VP8()->numberOfTemporalLayers, 0)));
    } else {
      for (uint32_t i = 0; i < codec_.numberOfSimulcastStreams; ++i) {
        tl_factories_.push_back(
            std::unique_ptr<TemporalLayers>(codec_.VP8()->tl_factory->Create(
                i, codec_.simulcastStream[i].numberOfTemporalLayers, 0)));
      }
    }
  }

  virtual std::unique_ptr<TemporalLayersFactory> GetTlFactory() {
    return std::unique_ptr<TemporalLayersFactory>(new TemporalLayersFactory());
  }

  BitrateAllocation GetAllocation(uint32_t target_bitrate) {
    return allocator_->GetAllocation(target_bitrate * 1000U, kDefaultFrameRate);
  }

 protected:
  static const int kDefaultFrameRate = 30;
  VideoCodec codec_;
  std::unique_ptr<SimulcastRateAllocator> allocator_;
  std::vector<std::unique_ptr<TemporalLayers>> tl_factories_;
};

TEST_F(SimulcastRateAllocatorTest, NoSimulcastBelowMin) {
  uint32_t expected[] = {codec_.minBitrate};
  ExpectEqual(expected, GetAllocation(codec_.minBitrate - 1));
  ExpectEqual(expected, GetAllocation(1));
  ExpectEqual(expected, GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastAboveMax) {
  uint32_t expected[] = {codec_.maxBitrate};
  ExpectEqual(expected, GetAllocation(codec_.maxBitrate + 1));
  ExpectEqual(expected, GetAllocation(std::numeric_limits<uint32_t>::max()));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastNoMax) {
  const uint32_t kMax = BitrateAllocation::kMaxBitrateBps / 1000;
  codec_.maxBitrate = 0;
  CreateAllocator();

  uint32_t expected[] = {kMax};
  ExpectEqual(expected, GetAllocation(kMax));
}

TEST_F(SimulcastRateAllocatorTest, NoSimulcastWithinLimits) {
  for (uint32_t bitrate = codec_.minBitrate; bitrate <= codec_.maxBitrate;
       ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastBelowMin) {
  // With simulcast, use the min bitrate from the ss spec instead of the global.
  codec_.numberOfSimulcastStreams = 1;
  const uint32_t kMin = codec_.minBitrate - 10;
  codec_.simulcastStream[0].minBitrate = kMin;
  codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
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
  CreateAllocator();

  for (uint32_t bitrate = kMinBitrateKbps; bitrate <= kMaxBitrateKbps;
       ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
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
}

TEST_F(SimulcastRateAllocatorTest, GetPreferredBitrateBps) {
  MockTemporalLayers mock_layers;
  allocator_.reset(new SimulcastRateAllocator(codec_, nullptr));
  allocator_->OnTemporalLayersCreated(0, &mock_layers);
  EXPECT_CALL(mock_layers, OnRatesUpdated(_, _, _)).Times(0);
  EXPECT_EQ(codec_.maxBitrate * 1000,
            allocator_->GetPreferredBitrateBps(codec_.maxFramerate));
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

  uint32_t preferred_bitrate_kbps;
  preferred_bitrate_kbps = codec_.simulcastStream[0].targetBitrate;
  preferred_bitrate_kbps += codec_.simulcastStream[1].targetBitrate;
  preferred_bitrate_kbps += codec_.simulcastStream[2].maxBitrate;

  EXPECT_EQ(preferred_bitrate_kbps * 1000,
            allocator_->GetPreferredBitrateBps(codec_.maxFramerate));
}

class ScreenshareRateAllocationTest : public SimulcastRateAllocatorTest {
 public:
  void SetupConferenceScreenshare(bool use_simulcast) {
    codec_.mode = VideoCodecMode::kScreensharing;
    codec_.minBitrate = kMinBitrateKbps;
    codec_.maxBitrate = kMaxBitrateKbps;
    if (use_simulcast) {
      codec_.numberOfSimulcastStreams = 1;
      codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
      codec_.simulcastStream[0].targetBitrate = kTargetBitrateKbps;
      codec_.simulcastStream[0].maxBitrate = kMaxBitrateKbps;
      codec_.simulcastStream[0].numberOfTemporalLayers = 2;
    } else {
      codec_.numberOfSimulcastStreams = 0;
      codec_.targetBitrate = kTargetBitrateKbps;
      codec_.VP8()->numberOfTemporalLayers = 2;
    }
  }

  std::unique_ptr<TemporalLayersFactory> GetTlFactory() override {
    return std::unique_ptr<TemporalLayersFactory>(
        new ScreenshareTemporalLayersFactory());
  }
};

INSTANTIATE_TEST_CASE_P(ScreenshareTest,
                        ScreenshareRateAllocationTest,
                        ::testing::Bool());

TEST_P(ScreenshareRateAllocationTest, BitrateBelowTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  BitrateAllocation allocation =
      allocator_->GetAllocation(kTargetBitrateKbps * 1000, kFramerateFps);

  // All allocation should go in TL0.
  EXPECT_EQ(kTargetBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kTargetBitrateKbps, allocation.GetBitrate(0, 0) / 1000);
}

TEST_P(ScreenshareRateAllocationTest, BitrateAboveTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  uint32_t target_bitrate_kbps = (kTargetBitrateKbps + kMaxBitrateKbps) / 2;
  BitrateAllocation allocation =
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

  BitrateAllocation allocation =
      allocator_->GetAllocation(kMaxBitrateKbps * 2000, kFramerateFps);

  // Fill both TL0 and TL1, but no more.
  EXPECT_EQ(kMaxBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kTargetBitrateKbps, allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(kMaxBitrateKbps - kTargetBitrateKbps,
            allocation.GetBitrate(0, 1) / 1000);
}

}  // namespace webrtc
