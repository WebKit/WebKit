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

#include "api/video_codecs/vp8_frame_buffer_controller.h"
#include "api/video_codecs/vp8_frame_config.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::_;

constexpr uint32_t kFramerateFps = 5;
constexpr uint32_t kMinBitrateKbps = 50;
// These correspond to kLegacyScreenshareTl(0|1)BitrateKbps in cc.
constexpr uint32_t kLegacyScreenshareTargetBitrateKbps = 200;
constexpr uint32_t kLegacyScreenshareMaxBitrateKbps = 1000;
// Bitrates for upper simulcast screenshare layer.
constexpr uint32_t kSimulcastScreenshareMinBitrateKbps = 600;
constexpr uint32_t kSimulcastScreenshareMaxBitrateKbps = 1250;

class MockTemporalLayers : public Vp8FrameBufferController {
 public:
  MOCK_METHOD2(NextFrameConfig, Vp8FrameConfig(size_t, uint32_t));
  MOCK_METHOD3(OnRatesUpdated, void(size_t, const std::vector<uint32_t>&, int));
  MOCK_METHOD1(UpdateConfiguration, Vp8EncoderConfig(size_t));
  MOCK_METHOD6(OnEncodeDone,
               void(size_t, uint32_t, size_t, bool, int, CodecSpecificInfo*));
  MOCK_METHOD4(FrameEncoded, void(size_t, uint32_t, size_t, int));
  MOCK_CONST_METHOD0(Tl0PicIdx, uint8_t());
  MOCK_CONST_METHOD1(GetTemporalLayerId, int(const Vp8FrameConfig&));
};
}  // namespace

class SimulcastRateAllocatorTest : public ::testing::TestWithParam<bool> {
 public:
  SimulcastRateAllocatorTest() {
    memset(&codec_, 0, sizeof(VideoCodec));
    codec_.codecType = kVideoCodecVP8;
    codec_.minBitrate = kMinBitrateKbps;
    codec_.maxBitrate = kLegacyScreenshareMaxBitrateKbps;
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

  void SetupCodec3SL3TL(const std::vector<bool>& active_streams) {
    const size_t num_simulcast_layers = 3;
    RTC_DCHECK_GE(active_streams.size(), num_simulcast_layers);
    SetupCodec2SL3TL(active_streams);
    codec_.numberOfSimulcastStreams = num_simulcast_layers;
    codec_.simulcastStream[2].numberOfTemporalLayers = 3;
    codec_.simulcastStream[2].maxBitrate = 4000;
    codec_.simulcastStream[2].targetBitrate = 3000;
    codec_.simulcastStream[2].minBitrate = 2000;
    codec_.simulcastStream[2].active = active_streams[2];
  }

  void SetupCodec2SL3TL(const std::vector<bool>& active_streams) {
    const size_t num_simulcast_layers = 2;
    RTC_DCHECK_GE(active_streams.size(), num_simulcast_layers);
    SetupCodec1SL3TL(active_streams);
    codec_.numberOfSimulcastStreams = num_simulcast_layers;
    codec_.simulcastStream[1].numberOfTemporalLayers = 3;
    codec_.simulcastStream[1].maxBitrate = 1000;
    codec_.simulcastStream[1].targetBitrate = 500;
    codec_.simulcastStream[1].minBitrate = 50;
    codec_.simulcastStream[1].active = active_streams[1];
  }

  void SetupCodec1SL3TL(const std::vector<bool>& active_streams) {
    const size_t num_simulcast_layers = 2;
    RTC_DCHECK_GE(active_streams.size(), num_simulcast_layers);
    SetupCodec3TL();
    codec_.numberOfSimulcastStreams = num_simulcast_layers;
    codec_.simulcastStream[0].numberOfTemporalLayers = 3;
    codec_.simulcastStream[0].maxBitrate = 500;
    codec_.simulcastStream[0].targetBitrate = 100;
    codec_.simulcastStream[0].minBitrate = 10;
    codec_.simulcastStream[0].active = active_streams[0];
  }

  void SetupCodec3TL() {
    codec_.maxBitrate = 0;
    codec_.VP8()->numberOfTemporalLayers = 3;
  }

  VideoBitrateAllocation GetAllocation(uint32_t target_bitrate) {
    return allocator_->Allocate(VideoBitrateAllocationParameters(
        DataRate::KilobitsPerSec(target_bitrate), kDefaultFrameRate));
  }

  VideoBitrateAllocation GetAllocation(DataRate target_rate,
                                       DataRate stable_rate) {
    return allocator_->Allocate(VideoBitrateAllocationParameters(
        target_rate, stable_rate, kDefaultFrameRate));
  }

  DataRate MinRate(size_t layer_index) const {
    return DataRate::KilobitsPerSec(
        codec_.simulcastStream[layer_index].minBitrate);
  }

  DataRate TargetRate(size_t layer_index) const {
    return DataRate::KilobitsPerSec(
        codec_.simulcastStream[layer_index].targetBitrate);
  }

  DataRate MaxRate(size_t layer_index) const {
    return DataRate::KilobitsPerSec(
        codec_.simulcastStream[layer_index].maxBitrate);
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
  ExpectEqual(expected, GetAllocation(kLegacyScreenshareTargetBitrateKbps));
  ExpectEqual(expected, GetAllocation(kLegacyScreenshareMaxBitrateKbps + 10));
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastBelowMin) {
  // With simulcast, use the min bitrate from the ss spec instead of the global.
  codec_.numberOfSimulcastStreams = 1;
  const uint32_t kMin = codec_.minBitrate - 10;
  codec_.simulcastStream[0].minBitrate = kMin;
  codec_.simulcastStream[0].targetBitrate = kLegacyScreenshareTargetBitrateKbps;
  codec_.simulcastStream[0].active = true;
  CreateAllocator();

  uint32_t expected[] = {kMin};
  ExpectEqual(expected, GetAllocation(kMin - 1));
  ExpectEqual(expected, GetAllocation(1));
  ExpectEqual(expected, GetAllocation(0));
}

TEST_F(SimulcastRateAllocatorTest, SignalsBwLimited) {
  // Enough to enable all layers.
  const int kVeryBigBitrate = 100000;
  // With simulcast, use the min bitrate from the ss spec instead of the global.
  SetupCodec3SL3TL({true, true, true});
  CreateAllocator();

  EXPECT_TRUE(
      GetAllocation(codec_.simulcastStream[0].minBitrate - 10).is_bw_limited());
  EXPECT_TRUE(
      GetAllocation(codec_.simulcastStream[0].targetBitrate).is_bw_limited());
  EXPECT_TRUE(GetAllocation(codec_.simulcastStream[0].targetBitrate +
                            codec_.simulcastStream[1].minBitrate)
                  .is_bw_limited());
  EXPECT_FALSE(GetAllocation(codec_.simulcastStream[0].targetBitrate +
                             codec_.simulcastStream[1].targetBitrate +
                             codec_.simulcastStream[2].minBitrate)
                   .is_bw_limited());
  EXPECT_FALSE(GetAllocation(kVeryBigBitrate).is_bw_limited());
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
  codec_.simulcastStream[0].targetBitrate = kLegacyScreenshareTargetBitrateKbps;
  codec_.simulcastStream[0].maxBitrate = kLegacyScreenshareMaxBitrateKbps;
  codec_.simulcastStream[0].active = true;
  CreateAllocator();

  for (uint32_t bitrate = kMinBitrateKbps;
       bitrate <= kLegacyScreenshareMaxBitrateKbps; ++bitrate) {
    uint32_t expected[] = {bitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

TEST_F(SimulcastRateAllocatorTest, Regular3TLTemporalRateAllocation) {
  SetupCodec3SL3TL({true, true, true});
  CreateAllocator();

  const VideoBitrateAllocation alloc = GetAllocation(kMinBitrateKbps);
  // 40/20/40.
  EXPECT_EQ(static_cast<uint32_t>(0.4 * kMinBitrateKbps),
            alloc.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(static_cast<uint32_t>(0.2 * kMinBitrateKbps),
            alloc.GetBitrate(0, 1) / 1000);
  EXPECT_EQ(static_cast<uint32_t>(0.4 * kMinBitrateKbps),
            alloc.GetBitrate(0, 2) / 1000);
}

TEST_F(SimulcastRateAllocatorTest, BaseHeavy3TLTemporalRateAllocation) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-UseBaseHeavyVP8TL3RateAllocation/Enabled/");

  SetupCodec3SL3TL({true, true, true});
  CreateAllocator();

  const VideoBitrateAllocation alloc = GetAllocation(kMinBitrateKbps);
  // 60/20/20.
  EXPECT_EQ(static_cast<uint32_t>(0.6 * kMinBitrateKbps),
            alloc.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(static_cast<uint32_t>(0.2 * kMinBitrateKbps),
            alloc.GetBitrate(0, 1) / 1000);
  EXPECT_EQ(static_cast<uint32_t>(0.2 * kMinBitrateKbps),
            alloc.GetBitrate(0, 2) / 1000);
}

TEST_F(SimulcastRateAllocatorTest, SingleSimulcastInactive) {
  codec_.numberOfSimulcastStreams = 1;
  codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
  codec_.simulcastStream[0].targetBitrate = kLegacyScreenshareTargetBitrateKbps;
  codec_.simulcastStream[0].maxBitrate = kLegacyScreenshareMaxBitrateKbps;
  codec_.simulcastStream[0].active = false;
  CreateAllocator();

  uint32_t expected[] = {0};
  ExpectEqual(expected, GetAllocation(kMinBitrateKbps - 10));
  ExpectEqual(expected, GetAllocation(kLegacyScreenshareTargetBitrateKbps));
  ExpectEqual(expected, GetAllocation(kLegacyScreenshareMaxBitrateKbps + 10));
}

TEST_F(SimulcastRateAllocatorTest, OneToThreeStreams) {
  SetupCodec3SL3TL({true, true, true});
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
  SetupCodec3SL3TL({false, false, false});
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
  SetupCodec2SL3TL({false, true});
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
  SetupCodec2SL3TL({true, false});
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
  SetupCodec3SL3TL({true, false, true});
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

TEST_F(SimulcastRateAllocatorTest, NonConferenceModeScreenshare) {
  codec_.mode = VideoCodecMode::kScreensharing;
  SetupCodec3SL3TL({true, true, true});
  CreateAllocator();

  // Make sure we have enough bitrate for all 3 simulcast layers
  const uint32_t bitrate = codec_.simulcastStream[0].maxBitrate +
                           codec_.simulcastStream[1].maxBitrate +
                           codec_.simulcastStream[2].maxBitrate;
  const VideoBitrateAllocation alloc = GetAllocation(bitrate);

  EXPECT_EQ(alloc.GetTemporalLayerAllocation(0).size(), 3u);
  EXPECT_EQ(alloc.GetTemporalLayerAllocation(1).size(), 3u);
  EXPECT_EQ(alloc.GetTemporalLayerAllocation(2).size(), 3u);
}

TEST_F(SimulcastRateAllocatorTest, StableRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-StableTargetRate/"
      "enabled:true,"
      "video_hysteresis_factor:1.1/");

  SetupCodec3SL3TL({true, true, true});
  CreateAllocator();

  // Let the volatile rate always be be enough for all streams, in this test we
  // are only interested in how the stable rate affects enablement.
  const DataRate volatile_rate =
      (TargetRate(0) + TargetRate(1) + MinRate(2)) * 1.1;

  {
    // On the first call to a new SimulcastRateAllocator instance, hysteresis
    // is disabled, but stable rate still caps layers.
    uint32_t expected[] = {TargetRate(0).kbps<uint32_t>(),
                           MaxRate(1).kbps<uint32_t>()};
    ExpectEqual(expected,
                GetAllocation(volatile_rate, TargetRate(0) + MinRate(1)));
  }

  {
    // Let stable rate go to a bitrate below what is needed for two streams.
    uint32_t expected[] = {MaxRate(0).kbps<uint32_t>(), 0};
    ExpectEqual(expected,
                GetAllocation(volatile_rate, TargetRate(0) + MinRate(1) -
                                                 DataRate::BitsPerSec(1)));
  }

  {
    // Don't enable stream as we need to get up above hysteresis threshold.
    uint32_t expected[] = {MaxRate(0).kbps<uint32_t>(), 0};
    ExpectEqual(expected,
                GetAllocation(volatile_rate, TargetRate(0) + MinRate(1)));
  }

  {
    // Above threshold with hysteresis, enable second stream.
    uint32_t expected[] = {TargetRate(0).kbps<uint32_t>(),
                           MaxRate(1).kbps<uint32_t>()};
    ExpectEqual(expected, GetAllocation(volatile_rate,
                                        (TargetRate(0) + MinRate(1)) * 1.1));
  }

  {
    // Enough to enable all thee layers.
    uint32_t expected[] = {
        TargetRate(0).kbps<uint32_t>(), TargetRate(1).kbps<uint32_t>(),
        (volatile_rate - TargetRate(0) - TargetRate(1)).kbps<uint32_t>()};
    ExpectEqual(expected, GetAllocation(volatile_rate, volatile_rate));
  }

  {
    // Drop hysteresis, all three still on.
    uint32_t expected[] = {
        TargetRate(0).kbps<uint32_t>(), TargetRate(1).kbps<uint32_t>(),
        (volatile_rate - TargetRate(0) - TargetRate(1)).kbps<uint32_t>()};
    ExpectEqual(expected,
                GetAllocation(volatile_rate,
                              TargetRate(0) + TargetRate(1) + MinRate(2)));
  }
}

class ScreenshareRateAllocationTest : public SimulcastRateAllocatorTest {
 public:
  void SetupConferenceScreenshare(bool use_simulcast, bool active = true) {
    codec_.mode = VideoCodecMode::kScreensharing;
    codec_.minBitrate = kMinBitrateKbps;
    codec_.maxBitrate =
        kLegacyScreenshareMaxBitrateKbps + kSimulcastScreenshareMaxBitrateKbps;
    if (use_simulcast) {
      codec_.numberOfSimulcastStreams = 2;
      codec_.simulcastStream[0].minBitrate = kMinBitrateKbps;
      codec_.simulcastStream[0].targetBitrate =
          kLegacyScreenshareTargetBitrateKbps;
      codec_.simulcastStream[0].maxBitrate = kLegacyScreenshareMaxBitrateKbps;
      codec_.simulcastStream[0].numberOfTemporalLayers = 2;
      codec_.simulcastStream[0].active = active;

      codec_.simulcastStream[1].minBitrate =
          kSimulcastScreenshareMinBitrateKbps;
      codec_.simulcastStream[1].targetBitrate =
          kSimulcastScreenshareMaxBitrateKbps;
      codec_.simulcastStream[1].maxBitrate =
          kSimulcastScreenshareMaxBitrateKbps;
      codec_.simulcastStream[1].numberOfTemporalLayers = 2;
      codec_.simulcastStream[1].active = active;
    } else {
      codec_.numberOfSimulcastStreams = 0;
      codec_.VP8()->numberOfTemporalLayers = 2;
      codec_.active = active;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(ScreenshareTest,
                         ScreenshareRateAllocationTest,
                         ::testing::Bool());

TEST_P(ScreenshareRateAllocationTest, BitrateBelowTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  VideoBitrateAllocation allocation =
      allocator_->Allocate(VideoBitrateAllocationParameters(
          kLegacyScreenshareTargetBitrateKbps * 1000, kFramerateFps));

  // All allocation should go in TL0.
  EXPECT_EQ(kLegacyScreenshareTargetBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kLegacyScreenshareTargetBitrateKbps,
            allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(allocation.is_bw_limited(), GetParam());
}

TEST_P(ScreenshareRateAllocationTest, BitrateAboveTl0) {
  SetupConferenceScreenshare(GetParam());
  CreateAllocator();

  uint32_t target_bitrate_kbps =
      (kLegacyScreenshareTargetBitrateKbps + kLegacyScreenshareMaxBitrateKbps) /
      2;
  VideoBitrateAllocation allocation =
      allocator_->Allocate(VideoBitrateAllocationParameters(
          target_bitrate_kbps * 1000, kFramerateFps));

  // Fill TL0, then put the rest in TL1.
  EXPECT_EQ(target_bitrate_kbps, allocation.get_sum_kbps());
  EXPECT_EQ(kLegacyScreenshareTargetBitrateKbps,
            allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(target_bitrate_kbps - kLegacyScreenshareTargetBitrateKbps,
            allocation.GetBitrate(0, 1) / 1000);
  EXPECT_EQ(allocation.is_bw_limited(), GetParam());
}

TEST_F(ScreenshareRateAllocationTest, BitrateAboveTl1) {
  // This test is only for the non-simulcast case.
  SetupConferenceScreenshare(false);
  CreateAllocator();

  VideoBitrateAllocation allocation =
      allocator_->Allocate(VideoBitrateAllocationParameters(
          kLegacyScreenshareMaxBitrateKbps * 2000, kFramerateFps));

  // Fill both TL0 and TL1, but no more.
  EXPECT_EQ(kLegacyScreenshareMaxBitrateKbps, allocation.get_sum_kbps());
  EXPECT_EQ(kLegacyScreenshareTargetBitrateKbps,
            allocation.GetBitrate(0, 0) / 1000);
  EXPECT_EQ(
      kLegacyScreenshareMaxBitrateKbps - kLegacyScreenshareTargetBitrateKbps,
      allocation.GetBitrate(0, 1) / 1000);
  EXPECT_FALSE(allocation.is_bw_limited());
}

// This tests when the screenshare is inactive it should be allocated 0 bitrate
// for all layers.
TEST_P(ScreenshareRateAllocationTest, InactiveScreenshare) {
  SetupConferenceScreenshare(GetParam(), false);
  CreateAllocator();

  // Enough bitrate for TL0 and TL1.
  uint32_t target_bitrate_kbps =
      (kLegacyScreenshareTargetBitrateKbps + kLegacyScreenshareMaxBitrateKbps) /
      2;
  VideoBitrateAllocation allocation =
      allocator_->Allocate(VideoBitrateAllocationParameters(
          target_bitrate_kbps * 1000, kFramerateFps));

  EXPECT_EQ(0U, allocation.get_sum_kbps());
}

TEST_F(ScreenshareRateAllocationTest, Hysteresis) {
  // This test is only for the simulcast case.
  SetupConferenceScreenshare(true);
  CreateAllocator();

  // The bitrate at which we would normally enable the upper simulcast stream.
  const uint32_t default_enable_rate_bps =
      codec_.simulcastStream[0].targetBitrate +
      codec_.simulcastStream[1].minBitrate;
  const uint32_t enable_rate_with_hysteresis_bps =
      (default_enable_rate_bps * 135) / 100;

  {
    // On the first call to a new SimulcastRateAllocator instance, hysteresis
    // is disabled.
    const uint32_t bitrate = default_enable_rate_bps;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].minBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Go down to a bitrate below what is needed for two streams.
    const uint32_t bitrate = default_enable_rate_bps - 1;
    uint32_t expected[] = {bitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Don't enable stream as we need to get up above hysteresis threshold.
    const uint32_t bitrate = default_enable_rate_bps;
    uint32_t expected[] = {bitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Above threshold, enable second stream.
    const uint32_t bitrate = enable_rate_with_hysteresis_bps;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           enable_rate_with_hysteresis_bps -
                               codec_.simulcastStream[0].targetBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Go down again, still keep the second stream alive.
    const uint32_t bitrate = default_enable_rate_bps;
    uint32_t expected[] = {codec_.simulcastStream[0].targetBitrate,
                           codec_.simulcastStream[1].minBitrate};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Go down below default enable, second stream is shut down again.
    const uint32_t bitrate = default_enable_rate_bps - 1;
    uint32_t expected[] = {bitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }

  {
    // Go up, hysteresis is blocking us again.
    const uint32_t bitrate = default_enable_rate_bps;
    uint32_t expected[] = {bitrate, 0};
    ExpectEqual(expected, GetAllocation(bitrate));
  }
}

}  // namespace webrtc
