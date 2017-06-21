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

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/mock/mock_frame_dropper.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

// 5 frames per second at 90 kHz.
const uint32_t kTimestampDelta5Fps = 90000 / 5;
const int kDefaultQp = 54;
const int kDefaultTl0BitrateKbps = 200;
const int kDefaultTl1BitrateKbps = 2000;
const int kFrameRate = 5;
const int kSyncPeriodSeconds = 5;
const int kMaxSyncPeriodSeconds = 10;

// Expected flags for corresponding temporal layers.
const int kTl0Flags = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
const int kTl1Flags =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
const int kTl1SyncFlags = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF |
                          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;

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
    layers_.reset(new ScreenshareLayers(2, 0, &clock_));
    cfg_ = ConfigureBitrates();
  }

  int EncodeFrame(bool base_sync) {
    int flags = ConfigureFrame(base_sync);
    if (flags != -1)
      layers_->FrameEncoded(frame_size_, kDefaultQp);
    return flags;
  }

  int ConfigureFrame(bool key_frame) {
    tl_config_ = layers_->UpdateLayerConfig(timestamp_);
    if (tl_config_.drop_frame) {
      return -1;
    }
    config_updated_ = layers_->UpdateConfiguration(&cfg_);
    int flags = VP8EncoderImpl::EncodeFlags(tl_config_);
    layers_->PopulateCodecSpecific(key_frame, tl_config_, &vp8_info_,
                                   timestamp_);
    EXPECT_NE(-1, frame_size_);
    return flags;
  }

  int FrameSizeForBitrate(int bitrate_kbps) {
    return ((bitrate_kbps * 1000) / 8) / kFrameRate;
  }

  vpx_codec_enc_cfg_t ConfigureBitrates() {
    vpx_codec_enc_cfg_t vpx_cfg;
    memset(&vpx_cfg, 0, sizeof(vpx_codec_enc_cfg_t));
    vpx_cfg.rc_min_quantizer = min_qp_;
    vpx_cfg.rc_max_quantizer = max_qp_;
    EXPECT_THAT(layers_->OnRatesUpdated(kDefaultTl0BitrateKbps,
                                        kDefaultTl1BitrateKbps, kFrameRate),
                ElementsAre(kDefaultTl0BitrateKbps,
                            kDefaultTl1BitrateKbps - kDefaultTl0BitrateKbps));
    EXPECT_TRUE(layers_->UpdateConfiguration(&vpx_cfg));
    frame_size_ = FrameSizeForBitrate(vpx_cfg.rc_target_bitrate);
    return vpx_cfg;
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
  void SkipUntilTl(int layer) {
    for (int i = 0; i < 5; ++i) {
      ConfigureFrame(false);
      timestamp_ += kTimestampDelta5Fps;
      if (vp8_info_.temporalIdx != layer) {
        layers_->FrameEncoded(frame_size_, kDefaultQp);
      } else {
        // Found frame form sought layer.
        return;
      }
    }
    ADD_FAILURE() << "Did not get a frame of TL" << layer << " in time.";
  }

  int min_qp_;
  int max_qp_;
  int frame_size_;
  SimulatedClock clock_;
  std::unique_ptr<ScreenshareLayers> layers_;

  uint32_t timestamp_;
  TemporalLayers::FrameConfig tl_config_;
  vpx_codec_enc_cfg_t cfg_;
  bool config_updated_;
  CodecSpecificInfoVP8 vp8_info_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(new ScreenshareLayers(1, 0, &clock_));
  ConfigureBitrates();
  // One layer screenshare should not use the frame dropper as all frames will
  // belong to the base layer.
  const int kSingleLayerFlags = 0;
  int flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info_.temporalIdx);
  EXPECT_FALSE(vp8_info_.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info_.tl0PicIdx);

  flags = EncodeFrame(false);
  EXPECT_EQ(kSingleLayerFlags, flags);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info_.temporalIdx);
  EXPECT_FALSE(vp8_info_.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info_.tl0PicIdx);
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
    tl_config_ = layers_->UpdateLayerConfig(timestamp_);
    config_updated_ = layers_->UpdateConfiguration(&cfg_);
    layers_->PopulateCodecSpecific(false, tl_config_, &vp8_info_, timestamp_);

    // Simulate TL1 being at least 8 qp steps better.
    if (vp8_info_.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, kDefaultQp - 8);
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
    if (vp8_info_.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, kDefaultQp - 8);
    }

    if (vp8_info_.temporalIdx == 1 && vp8_info_.layerSync)
      sync_times.push_back(timestamp_);

    timestamp_ += kTimestampDelta5Fps;
  }

  ASSERT_EQ(1u, sync_times.size());

  bool bumped_tl0_quality = false;
  for (int i = 0; i < 3; ++i) {
    int flags = ConfigureFrame(false);
    if (vp8_info_.temporalIdx == 0) {
      // Bump TL0 to same quality as TL1.
      layers_->FrameEncoded(frame_size_, kDefaultQp - 8);
      bumped_tl0_quality = true;
    } else {
      layers_->FrameEncoded(frame_size_, kDefaultQp - 8);
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
  layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5);

  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps, kTl1_kbps - kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * kTl0_kbps + 0.5),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 450;
  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps, kTl1_kbps - kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<unsigned int>(
                kTl1_kbps / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 100;
  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg_));

  EXPECT_EQ(static_cast<uint32_t>(kTl1_kbps), cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, EncoderDrop) {
  EXPECT_TRUE(RunGracePeriod());
  SkipUntilTl(0);

  // Size 0 indicates dropped frame.
  layers_->FrameEncoded(0, kDefaultQp);

  // Re-encode frame (so don't advance timestamp).
  int flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl0Flags, flags);

  // Next frame should have boosted quality...
  SkipUntilTl(0);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, kDefaultQp);
  timestamp_ += kTimestampDelta5Fps;

  // ...then back to standard setup.
  SkipUntilTl(0);
  layers_->FrameEncoded(frame_size_, kDefaultQp);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));

  // Next drop in TL1.
  SkipUntilTl(1);
  layers_->FrameEncoded(0, kDefaultQp);

  // Re-encode frame (so don't advance timestamp).
  flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl1Flags, flags);

  // Next frame should have boosted QP.
  SkipUntilTl(1);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, kDefaultQp);
  timestamp_ += kTimestampDelta5Fps;

  // ...and back to normal.
  SkipUntilTl(1);
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, kDefaultQp);
  timestamp_ += kTimestampDelta5Fps;
}

TEST_F(ScreenshareLayerTest, RespectsMaxIntervalBetweenFrames) {
  const int kLowBitrateKbps = 50;
  const int kLargeFrameSizeBytes = 100000;
  const uint32_t kStartTimestamp = 1234;

  layers_->OnRatesUpdated(kLowBitrateKbps, kLowBitrateKbps, 5);
  layers_->UpdateConfiguration(&cfg_);

  EXPECT_EQ(kTl0Flags, VP8EncoderImpl::EncodeFlags(
                           layers_->UpdateLayerConfig(kStartTimestamp)));
  layers_->FrameEncoded(kLargeFrameSizeBytes, kDefaultQp);

  const uint32_t kTwoSecondsLater =
      kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90);

  // Sanity check, repayment time should exceed kMaxFrameIntervalMs.
  ASSERT_GT(kStartTimestamp + 90 * (kLargeFrameSizeBytes * 8) / kLowBitrateKbps,
            kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90));

  EXPECT_TRUE(layers_->UpdateLayerConfig(kTwoSecondsLater).drop_frame);
  // More than two seconds has passed since last frame, one should be emitted
  // even if bitrate target is then exceeded.
  EXPECT_EQ(kTl0Flags, VP8EncoderImpl::EncodeFlags(
                           layers_->UpdateLayerConfig(kTwoSecondsLater + 90)));
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
    tl_config_ = layers_->UpdateLayerConfig(timestamp);
    if (tl_config_.drop_frame) {
      dropped_frame = true;
      continue;
    }
    int flags = VP8EncoderImpl::EncodeFlags(tl_config_);
    if (flags != -1)
      layers_->UpdateConfiguration(&cfg_);

    if (timestamp >= kTimestampDelta5Fps * 5 && !overshoot && flags != -1) {
      // Simulate one overshoot.
      layers_->FrameEncoded(0, 0);
      overshoot = true;
      flags =
          VP8EncoderImpl::EncodeFlags(layers_->UpdateLayerConfig(timestamp));
    }

    if (flags == kTl0Flags) {
      if (timestamp >= kTimestampDelta5Fps * 20 && !trigger_drop) {
        // Simulate a too large frame, to cause frame drop.
        layers_->FrameEncoded(frame_size_ * 10, kTl0Qp);
        trigger_drop = true;
      } else {
        layers_->FrameEncoded(frame_size_, kTl0Qp);
      }
    } else if (flags == kTl1Flags || flags == kTl1SyncFlags) {
      layers_->FrameEncoded(frame_size_, kTl1Qp);
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
  layers_.reset(new ScreenshareLayers(2, 0, &clock_));
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
    if (layers_->UpdateLayerConfig(timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->FrameEncoded(frame_size_bytes, kDefaultQp);
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
    if (layers_->UpdateLayerConfig(timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->FrameEncoded(frame_size_bytes, kDefaultQp);
    }
    timestamp += kFrameIntervalsMs * 90 / 2;
    clock_.AdvanceTimeMilliseconds(kFrameIntervalsMs / 2);
    ++num_input_frames;
  }

  // Allow for some rounding errors in the measurements.
  EXPECT_NEAR(num_discarded_frames, num_input_frames / 2, 2);
}
}  // namespace webrtc
