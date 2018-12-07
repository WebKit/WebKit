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
#include <vector>

#include "modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {
namespace {
// 5 frames per second at 90 kHz.
const uint32_t kTimestampDelta5Fps = 90000 / 5;
const int kDefaultQp = 54;
const int kDefaultTl0BitrateKbps = 200;
const int kDefaultTl1BitrateKbps = 2000;
const int kFrameRate = 5;
const int kSyncPeriodSeconds = 2;
const int kMaxSyncPeriodSeconds = 4;

// Expected flags for corresponding temporal layers.
const int kTl0Flags = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
const int kTl1Flags =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
const int kTl1SyncFlags = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF |
                          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
const std::vector<uint32_t> kDefault2TlBitratesBps = {
    kDefaultTl0BitrateKbps * 1000,
    (kDefaultTl1BitrateKbps - kDefaultTl0BitrateKbps) * 1000};

}  // namespace

class ScreenshareLayerTest : public ::testing::Test {
 protected:
  ScreenshareLayerTest()
      : min_qp_(2),
        max_qp_(kDefaultQp),
        frame_size_(-1),
        clock_(1),
        timestamp_(90),
        config_updated_(false) {}
  virtual ~ScreenshareLayerTest() {}

  void SetUp() override {
    layers_.reset(new ScreenshareLayers(2, &clock_));
    cfg_ = ConfigureBitrates();
  }

  int EncodeFrame(bool base_sync) {
    int flags = ConfigureFrame(base_sync);
    if (flags != -1)
      layers_->OnEncodeDone(timestamp_, frame_size_, base_sync, kDefaultQp,
                            &vp8_info_);
    return flags;
  }

  int ConfigureFrame(bool key_frame) {
    tl_config_ = UpdateLayerConfig(timestamp_);
    EXPECT_EQ(0, tl_config_.encoder_layer_id)
        << "ScreenshareLayers always encodes using the bitrate allocator for "
           "layer 0, but may reference different buffers and packetize "
           "differently.";
    if (tl_config_.drop_frame) {
      return -1;
    }
    config_updated_ = layers_->UpdateConfiguration(&cfg_);
    int flags = LibvpxVp8Encoder::EncodeFlags(tl_config_);
    EXPECT_NE(-1, frame_size_);
    return flags;
  }

  Vp8TemporalLayers::FrameConfig UpdateLayerConfig(uint32_t timestamp) {
    int64_t timestamp_ms = timestamp / 90;
    clock_.AdvanceTimeMilliseconds(timestamp_ms - clock_.TimeInMilliseconds());
    return layers_->UpdateLayerConfig(timestamp);
  }

  int FrameSizeForBitrate(int bitrate_kbps) {
    return ((bitrate_kbps * 1000) / 8) / kFrameRate;
  }

  Vp8EncoderConfig ConfigureBitrates() {
    Vp8EncoderConfig vp8_cfg;
    memset(&vp8_cfg, 0, sizeof(Vp8EncoderConfig));
    vp8_cfg.rc_min_quantizer = min_qp_;
    vp8_cfg.rc_max_quantizer = max_qp_;
    layers_->OnRatesUpdated(kDefault2TlBitratesBps, kFrameRate);
    EXPECT_TRUE(layers_->UpdateConfiguration(&vp8_cfg));
    frame_size_ = FrameSizeForBitrate(vp8_cfg.rc_target_bitrate);
    return vp8_cfg;
  }

  void WithQpLimits(int min_qp, int max_qp) {
    min_qp_ = min_qp;
    max_qp_ = max_qp;
  }

  // Runs a few initial frames and makes sure we have seen frames on both
  // temporal layers.
  bool RunGracePeriod() {
    bool got_tl0 = false;
    bool got_tl1 = false;
    for (int i = 0; i < 10; ++i) {
      EXPECT_NE(-1, EncodeFrame(false));
      timestamp_ += kTimestampDelta5Fps;
      if (vp8_info_.temporalIdx == 0) {
        got_tl0 = true;
      } else {
        got_tl1 = true;
      }
      if (got_tl0 && got_tl1)
        return true;
    }
    return false;
  }

  // Adds frames until we get one in the specified temporal layer. The last
  // FrameEncoded() call will be omitted and needs to be done by the caller.
  // Returns the flags for the last frame.
  int SkipUntilTl(int layer) {
    return SkipUntilTlAndSync(layer, absl::nullopt);
  }

  // Same as SkipUntilTl, but also waits until the sync bit condition is met.
  int SkipUntilTlAndSync(int layer, absl::optional<bool> sync) {
    int flags = 0;
    const int kMaxFramesToSkip =
        1 + (sync.value_or(false) ? kMaxSyncPeriodSeconds : 1) * kFrameRate;
    for (int i = 0; i < kMaxFramesToSkip; ++i) {
      flags = ConfigureFrame(false);
      if (tl_config_.packetizer_temporal_idx != layer ||
          (sync && *sync != tl_config_.layer_sync)) {
        layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp,
                              &vp8_info_);
        timestamp_ += kTimestampDelta5Fps;
      } else {
        // Found frame from sought after layer.
        return flags;
      }
    }
    ADD_FAILURE() << "Did not get a frame of TL" << layer << " in time.";
    return -1;
  }

  int min_qp_;
  uint32_t max_qp_;
  int frame_size_;
  SimulatedClock clock_;
  std::unique_ptr<ScreenshareLayers> layers_;

  uint32_t timestamp_;
  Vp8TemporalLayers::FrameConfig tl_config_;
  Vp8EncoderConfig cfg_;
  bool config_updated_;
  CodecSpecificInfoVP8 vp8_info_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(new ScreenshareLayers(1, &clock_));
  ConfigureBitrates();
  // One layer screenshare should not use the frame dropper as all frames will
  // belong to the base layer.
  const int kSingleLayerFlags = 0;
  int flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info_.temporalIdx);
  EXPECT_FALSE(vp8_info_.layerSync);

  flags = EncodeFrame(false);
  EXPECT_EQ(kSingleLayerFlags, flags);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info_.temporalIdx);
  EXPECT_FALSE(vp8_info_.layerSync);
}

TEST_F(ScreenshareLayerTest, 2LayersPeriodicSync) {
  std::vector<int> sync_times;
  const int kNumFrames = kSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    EncodeFrame(false);
    timestamp_ += kTimestampDelta5Fps;
    if (vp8_info_.temporalIdx == 1 && vp8_info_.layerSync) {
      sync_times.push_back(timestamp_);
    }
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterTimeout) {
  std::vector<int> sync_times;
  const int kNumFrames = kMaxSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    tl_config_ = UpdateLayerConfig(timestamp_);
    config_updated_ = layers_->UpdateConfiguration(&cfg_);

    // Simulate TL1 being at least 8 qp steps better.
    if (tl_config_.packetizer_temporal_idx == 0) {
      layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp,
                            &vp8_info_);
    } else {
      layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp - 8,
                            &vp8_info_);
    }

    if (vp8_info_.temporalIdx == 1 && vp8_info_.layerSync)
      sync_times.push_back(timestamp_);

    timestamp_ += kTimestampDelta5Fps;
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kMaxSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterSimilarQP) {
  std::vector<int> sync_times;

  const int kNumFrames = (kSyncPeriodSeconds +
                          ((kMaxSyncPeriodSeconds - kSyncPeriodSeconds) / 2)) *
                         kFrameRate;
  for (int i = 0; i < kNumFrames; ++i) {
    ConfigureFrame(false);

    // Simulate TL1 being at least 8 qp steps better.
    if (tl_config_.packetizer_temporal_idx == 0) {
      layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp,
                            &vp8_info_);
    } else {
      layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp - 8,
                            &vp8_info_);
    }

    if (vp8_info_.temporalIdx == 1 && vp8_info_.layerSync)
      sync_times.push_back(timestamp_);

    timestamp_ += kTimestampDelta5Fps;
  }

  ASSERT_EQ(1u, sync_times.size());

  bool bumped_tl0_quality = false;
  for (int i = 0; i < 3; ++i) {
    int flags = ConfigureFrame(false);
    layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp - 8,
                          &vp8_info_);
    if (vp8_info_.temporalIdx == 0) {
      // Bump TL0 to same quality as TL1.
      bumped_tl0_quality = true;
    } else {
      if (bumped_tl0_quality) {
        EXPECT_TRUE(vp8_info_.layerSync);
        EXPECT_EQ(kTl1SyncFlags, flags);
        return;
      }
    }
    timestamp_ += kTimestampDelta5Fps;
  }
  ADD_FAILURE() << "No TL1 frame arrived within time limit.";
}

TEST_F(ScreenshareLayerTest, 2LayersToggling) {
  EXPECT_TRUE(RunGracePeriod());

  // Insert 50 frames. 2/5 should be TL0.
  int tl0_frames = 0;
  int tl1_frames = 0;
  for (int i = 0; i < 50; ++i) {
    EncodeFrame(false);
    timestamp_ += kTimestampDelta5Fps;
    switch (vp8_info_.temporalIdx) {
      case 0:
        ++tl0_frames;
        break;
      case 1:
        ++tl1_frames;
        break;
      default:
        abort();
    }
  }
  EXPECT_EQ(20, tl0_frames);
  EXPECT_EQ(30, tl1_frames);
}

TEST_F(ScreenshareLayerTest, AllFitsLayer0) {
  frame_size_ = FrameSizeForBitrate(kDefaultTl0BitrateKbps);

  // Insert 50 frames, small enough that all fits in TL0.
  for (int i = 0; i < 50; ++i) {
    int flags = EncodeFrame(false);
    timestamp_ += kTimestampDelta5Fps;
    EXPECT_EQ(kTl0Flags, flags);
    EXPECT_EQ(0, vp8_info_.temporalIdx);
  }
}

TEST_F(ScreenshareLayerTest, TooHighBitrate) {
  frame_size_ = 2 * FrameSizeForBitrate(kDefaultTl1BitrateKbps);

  // Insert 100 frames. Half should be dropped.
  int tl0_frames = 0;
  int tl1_frames = 0;
  int dropped_frames = 0;
  for (int i = 0; i < 100; ++i) {
    int flags = EncodeFrame(false);
    timestamp_ += kTimestampDelta5Fps;
    if (flags == -1) {
      ++dropped_frames;
    } else {
      switch (vp8_info_.temporalIdx) {
        case 0:
          ++tl0_frames;
          break;
        case 1:
          ++tl1_frames;
          break;
        default:
          ADD_FAILURE() << "Unexpected temporal id";
      }
    }
  }

  EXPECT_NEAR(50, tl0_frames + tl1_frames, 1);
  EXPECT_NEAR(50, dropped_frames, 1);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL0) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 1000;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000,
                                             (kTl1_kbps - kTl0_kbps) * 1000};
  layers_->OnRatesUpdated(layer_rates, kFrameRate);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * kTl0_kbps + 0.5),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 450;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000,
                                             (kTl1_kbps - kTl0_kbps) * 1000};
  layers_->OnRatesUpdated(layer_rates, kFrameRate);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<unsigned int>(
                kTl1_kbps / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  const int kTl0_kbps = 100;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000};
  layers_->OnRatesUpdated(layer_rates, kFrameRate);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<uint32_t>(kTl0_kbps), cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, EncoderDrop) {
  EXPECT_TRUE(RunGracePeriod());
  SkipUntilTl(0);

  // Size 0 indicates dropped frame.
  layers_->OnEncodeDone(timestamp_, 0, false, 0, &vp8_info_);

  // Re-encode frame (so don't advance timestamp).
  int flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl0Flags, flags);

  // Next frame should have boosted quality...
  SkipUntilTl(0);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp, &vp8_info_);
  timestamp_ += kTimestampDelta5Fps;

  // ...then back to standard setup.
  SkipUntilTl(0);
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp, &vp8_info_);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));

  // Next drop in TL1.
  SkipUntilTl(1);
  layers_->OnEncodeDone(timestamp_, 0, false, 0, &vp8_info_);

  // Re-encode frame (so don't advance timestamp).
  flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl1Flags, flags);

  // Next frame should have boosted QP.
  SkipUntilTl(1);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp, &vp8_info_);
  timestamp_ += kTimestampDelta5Fps;

  // ...and back to normal.
  SkipUntilTl(1);
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp, &vp8_info_);
  timestamp_ += kTimestampDelta5Fps;
}

TEST_F(ScreenshareLayerTest, RespectsMaxIntervalBetweenFrames) {
  const int kLowBitrateKbps = 50;
  const int kLargeFrameSizeBytes = 100000;
  const uint32_t kStartTimestamp = 1234;

  const std::vector<uint32_t> layer_rates = {kLowBitrateKbps * 1000};
  layers_->OnRatesUpdated(layer_rates, kFrameRate);
  layers_->UpdateConfiguration(&cfg_);

  EXPECT_EQ(kTl0Flags,
            LibvpxVp8Encoder::EncodeFlags(UpdateLayerConfig(kStartTimestamp)));
  layers_->OnEncodeDone(kStartTimestamp, kLargeFrameSizeBytes, false,
                        kDefaultQp, &vp8_info_);

  const uint32_t kTwoSecondsLater =
      kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90);

  // Sanity check, repayment time should exceed kMaxFrameIntervalMs.
  ASSERT_GT(kStartTimestamp + 90 * (kLargeFrameSizeBytes * 8) / kLowBitrateKbps,
            kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90));

  // Expect drop one frame interval before the two second timeout. If we try
  // any later, the frame will be dropped anyway by the frame rate throttling
  // logic.
  EXPECT_TRUE(
      UpdateLayerConfig(kTwoSecondsLater - kTimestampDelta5Fps).drop_frame);

  // More than two seconds has passed since last frame, one should be emitted
  // even if bitrate target is then exceeded.
  EXPECT_EQ(kTl0Flags, LibvpxVp8Encoder::EncodeFlags(
                           UpdateLayerConfig(kTwoSecondsLater + 90)));
}

TEST_F(ScreenshareLayerTest, UpdatesHistograms) {
  metrics::Reset();
  bool trigger_drop = false;
  bool dropped_frame = false;
  bool overshoot = false;
  const int kTl0Qp = 35;
  const int kTl1Qp = 30;
  for (int64_t timestamp = 0;
       timestamp < kTimestampDelta5Fps * 5 * metrics::kMinRunTimeInSeconds;
       timestamp += kTimestampDelta5Fps) {
    tl_config_ = UpdateLayerConfig(timestamp);
    if (tl_config_.drop_frame) {
      dropped_frame = true;
      continue;
    }
    int flags = LibvpxVp8Encoder::EncodeFlags(tl_config_);
    if (flags != -1)
      layers_->UpdateConfiguration(&cfg_);

    if (timestamp >= kTimestampDelta5Fps * 5 && !overshoot && flags != -1) {
      // Simulate one overshoot.
      layers_->OnEncodeDone(timestamp, 0, false, 0, nullptr);
      overshoot = true;
    }

    if (flags == kTl0Flags) {
      if (timestamp >= kTimestampDelta5Fps * 20 && !trigger_drop) {
        // Simulate a too large frame, to cause frame drop.
        layers_->OnEncodeDone(timestamp, frame_size_ * 10, false, kTl0Qp,
                              &vp8_info_);
        trigger_drop = true;
      } else {
        layers_->OnEncodeDone(timestamp, frame_size_, false, kTl0Qp,
                              &vp8_info_);
      }
    } else if (flags == kTl1Flags || flags == kTl1SyncFlags) {
      layers_->OnEncodeDone(timestamp, frame_size_, false, kTl1Qp, &vp8_info_);
    } else if (flags == -1) {
      dropped_frame = true;
    } else {
      RTC_NOTREACHED() << "Unexpected flags";
    }
    clock_.AdvanceTimeMilliseconds(1000 / 5);
  }

  EXPECT_TRUE(overshoot);
  EXPECT_TRUE(dropped_frame);

  layers_.reset();  // Histograms are reported on destruction.

  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.FrameRate"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.FrameRate"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Screenshare.FramesPerDrop"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.Screenshare.FramesPerOvershoot"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.Qp"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.Qp"));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.TargetBitrate"));
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.TargetBitrate"));

  EXPECT_GT(metrics::MinSample("WebRTC.Video.Screenshare.Layer0.FrameRate"), 1);
  EXPECT_GT(metrics::MinSample("WebRTC.Video.Screenshare.Layer1.FrameRate"), 1);
  EXPECT_GT(metrics::MinSample("WebRTC.Video.Screenshare.FramesPerDrop"), 1);
  EXPECT_GT(metrics::MinSample("WebRTC.Video.Screenshare.FramesPerOvershoot"),
            1);
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.Screenshare.Layer0.Qp", kTl0Qp));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.Screenshare.Layer1.Qp", kTl1Qp));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.Screenshare.Layer0.TargetBitrate",
                               kDefaultTl0BitrateKbps));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.Screenshare.Layer1.TargetBitrate",
                               kDefaultTl1BitrateKbps));
}

TEST_F(ScreenshareLayerTest, AllowsUpdateConfigBeforeSetRates) {
  layers_.reset(new ScreenshareLayers(2, &clock_));
  // New layer instance, OnRatesUpdated() never called.
  // UpdateConfiguration() call should not cause crash.
  layers_->UpdateConfiguration(&cfg_);
}

TEST_F(ScreenshareLayerTest, RespectsConfiguredFramerate) {
  int64_t kTestSpanMs = 2000;
  int64_t kFrameIntervalsMs = 1000 / kFrameRate;

  uint32_t timestamp = 1234;
  int num_input_frames = 0;
  int num_discarded_frames = 0;

  // Send at regular rate - no drops expected.
  for (int64_t i = 0; i < kTestSpanMs; i += kFrameIntervalsMs) {
    if (UpdateLayerConfig(timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->OnEncodeDone(timestamp, frame_size_bytes, false, kDefaultQp,
                            &vp8_info_);
    }
    timestamp += kFrameIntervalsMs * 90;
    clock_.AdvanceTimeMilliseconds(kFrameIntervalsMs);
    ++num_input_frames;
  }
  EXPECT_EQ(0, num_discarded_frames);

  // Send at twice the configured rate - drop every other frame.
  num_input_frames = 0;
  num_discarded_frames = 0;
  for (int64_t i = 0; i < kTestSpanMs; i += kFrameIntervalsMs / 2) {
    if (UpdateLayerConfig(timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->OnEncodeDone(timestamp, frame_size_bytes, false, kDefaultQp,
                            &vp8_info_);
    }
    timestamp += kFrameIntervalsMs * 90 / 2;
    clock_.AdvanceTimeMilliseconds(kFrameIntervalsMs / 2);
    ++num_input_frames;
  }

  // Allow for some rounding errors in the measurements.
  EXPECT_NEAR(num_discarded_frames, num_input_frames / 2, 2);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAtOvershootDrop) {
  // Run grace period so we have existing frames in both TL0 and Tl1.
  EXPECT_TRUE(RunGracePeriod());

  // Move ahead until we have a sync frame in TL1.
  EXPECT_EQ(kTl1SyncFlags, SkipUntilTlAndSync(1, true));
  ASSERT_TRUE(tl_config_.layer_sync);

  // Simulate overshoot of this frame.
  layers_->OnEncodeDone(timestamp_, 0, false, 0, nullptr);

  config_updated_ = layers_->UpdateConfiguration(&cfg_);
  EXPECT_EQ(kTl1SyncFlags, LibvpxVp8Encoder::EncodeFlags(tl_config_));

  CodecSpecificInfoVP8 new_vp8_info;
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp,
                        &new_vp8_info);
  EXPECT_TRUE(new_vp8_info.layerSync);
}

TEST_F(ScreenshareLayerTest, DropOnTooShortFrameInterval) {
  // Run grace period so we have existing frames in both TL0 and Tl1.
  EXPECT_TRUE(RunGracePeriod());

  // Add a large gap, so there's plenty of room in the rate tracker.
  timestamp_ += kTimestampDelta5Fps * 3;
  EXPECT_FALSE(UpdateLayerConfig(timestamp_).drop_frame);
  layers_->OnEncodeDone(timestamp_, frame_size_, false, kDefaultQp, &vp8_info_);

  // Frame interval below 90% if desired time is not allowed, try inserting
  // frame just before this limit.
  const int64_t kMinFrameInterval = (kTimestampDelta5Fps * 85) / 100;
  timestamp_ += kMinFrameInterval - 90;
  EXPECT_TRUE(UpdateLayerConfig(timestamp_).drop_frame);

  // Try again at the limit, now it should pass.
  timestamp_ += 90;
  EXPECT_FALSE(UpdateLayerConfig(timestamp_).drop_frame);
}

TEST_F(ScreenshareLayerTest, AdjustsBitrateWhenDroppingFrames) {
  const uint32_t kTimestampDelta10Fps = kTimestampDelta5Fps / 2;
  const int kNumFrames = 30;
  uint32_t default_bitrate = cfg_.rc_target_bitrate;
  layers_->OnRatesUpdated(kDefault2TlBitratesBps, 10);

  int num_dropped_frames = 0;
  for (int i = 0; i < kNumFrames; ++i) {
    if (EncodeFrame(false) == -1)
      ++num_dropped_frames;
    timestamp_ += kTimestampDelta10Fps;
  }
  layers_->UpdateConfiguration(&cfg_);

  EXPECT_EQ(num_dropped_frames, kNumFrames / 2);
  EXPECT_EQ(cfg_.rc_target_bitrate, default_bitrate * 2);
}

TEST_F(ScreenshareLayerTest, UpdatesConfigurationAfterRateChange) {
  // Set inital rate again, no need to update configuration.
  layers_->OnRatesUpdated(kDefault2TlBitratesBps, kFrameRate);
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg_));

  // Rate changed, now update config.
  std::vector<uint32_t> bitrates = kDefault2TlBitratesBps;
  bitrates[1] -= 100000;
  layers_->OnRatesUpdated(bitrates, 5);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  // Changed rate, but then set changed rate again before trying to update
  // configuration, update should still apply.
  bitrates[1] -= 100000;
  layers_->OnRatesUpdated(bitrates, 5);
  layers_->OnRatesUpdated(bitrates, 5);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));
}

TEST_F(ScreenshareLayerTest, MaxQpRestoredAfterDoubleDrop) {
  // Run grace period so we have existing frames in both TL0 and Tl1.
  EXPECT_TRUE(RunGracePeriod());

  // Move ahead until we have a sync frame in TL1.
  EXPECT_EQ(kTl1SyncFlags, SkipUntilTlAndSync(1, true));
  ASSERT_TRUE(tl_config_.layer_sync);

  // Simulate overshoot of this frame.
  layers_->OnEncodeDone(timestamp_, 0, false, -1, nullptr);

  // Simulate re-encoded frame.
  layers_->OnEncodeDone(timestamp_, 1, false, max_qp_, &vp8_info_);

  // Next frame, expect boosted quality.
  // Slightly alter bitrate between each frame.
  std::vector<uint32_t> kDefault2TlBitratesBpsAlt = kDefault2TlBitratesBps;
  kDefault2TlBitratesBpsAlt[1] += 4000;
  layers_->OnRatesUpdated(kDefault2TlBitratesBpsAlt, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, max_qp_);
  uint32_t adjusted_qp = cfg_.rc_max_quantizer;

  // Simulate overshoot of this frame.
  layers_->OnEncodeDone(timestamp_, 0, false, -1, nullptr);

  // Simulate re-encoded frame.
  layers_->OnEncodeDone(timestamp_, frame_size_, false, max_qp_, &vp8_info_);

  // A third frame, expect boosted quality.
  layers_->OnRatesUpdated(kDefault2TlBitratesBps, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, max_qp_);
  EXPECT_EQ(adjusted_qp, cfg_.rc_max_quantizer);

  // Frame encoded.
  layers_->OnEncodeDone(timestamp_, frame_size_, false, max_qp_, &vp8_info_);

  // A fourth frame, max qp should be restored.
  layers_->OnRatesUpdated(kDefault2TlBitratesBpsAlt, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_EQ(cfg_.rc_max_quantizer, max_qp_);
}

}  // namespace webrtc
