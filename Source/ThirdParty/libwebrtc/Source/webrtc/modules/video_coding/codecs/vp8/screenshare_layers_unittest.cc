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

class ScreenshareLayerTest : public ::testing::Test {
 protected:
  ScreenshareLayerTest()
      : min_qp_(2), max_qp_(kDefaultQp), frame_size_(-1), clock_(1) {}
  virtual ~ScreenshareLayerTest() {}

  void SetUp() override { layers_.reset(new ScreenshareLayers(2, 0, &clock_)); }

  void EncodeFrame(uint32_t timestamp,
                   bool base_sync,
                   CodecSpecificInfoVP8* vp8_info,
                   int* flags) {
    *flags = layers_->EncodeFlags(timestamp);
    if (*flags == -1)
      return;
    layers_->PopulateCodecSpecific(base_sync, vp8_info, timestamp);
    ASSERT_NE(-1, frame_size_);
    layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
  }

  void ConfigureBitrates() {
    vpx_codec_enc_cfg_t vpx_cfg;
    memset(&vpx_cfg, 0, sizeof(vpx_codec_enc_cfg_t));
    vpx_cfg.rc_min_quantizer = min_qp_;
    vpx_cfg.rc_max_quantizer = max_qp_;
    EXPECT_THAT(layers_->OnRatesUpdated(kDefaultTl0BitrateKbps,
                                        kDefaultTl1BitrateKbps, kFrameRate),
                ElementsAre(kDefaultTl0BitrateKbps,
                            kDefaultTl1BitrateKbps - kDefaultTl0BitrateKbps));
    EXPECT_TRUE(layers_->UpdateConfiguration(&vpx_cfg));
    frame_size_ = ((vpx_cfg.rc_target_bitrate * 1000) / 8) / kFrameRate;
  }

  void WithQpLimits(int min_qp, int max_qp) {
    min_qp_ = min_qp;
    max_qp_ = max_qp;
  }

  int RunGracePeriod() {
    int flags = 0;
    uint32_t timestamp = 0;
    CodecSpecificInfoVP8 vp8_info;
    bool got_tl0 = false;
    bool got_tl1 = false;
    for (int i = 0; i < 10; ++i) {
      EncodeFrame(timestamp, false, &vp8_info, &flags);
      timestamp += kTimestampDelta5Fps;
      if (vp8_info.temporalIdx == 0) {
        got_tl0 = true;
      } else {
        got_tl1 = true;
      }
      if (got_tl0 && got_tl1)
        return timestamp;
    }
    ADD_FAILURE() << "Frames from both layers not received in time.";
    return 0;
  }

  int SkipUntilTl(int layer, int timestamp) {
    CodecSpecificInfoVP8 vp8_info;
    for (int i = 0; i < 5; ++i) {
      layers_->EncodeFlags(timestamp);
      timestamp += kTimestampDelta5Fps;
      layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
      if (vp8_info.temporalIdx != layer) {
        layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
      } else {
        return timestamp;
      }
    }
    ADD_FAILURE() << "Did not get a frame of TL" << layer << " in time.";
    return 0;
  }

  vpx_codec_enc_cfg_t GetConfig() {
    vpx_codec_enc_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rc_min_quantizer = 2;
    cfg.rc_max_quantizer = kDefaultQp;
    return cfg;
  }

  int min_qp_;
  int max_qp_;
  int frame_size_;
  SimulatedClock clock_;
  std::unique_ptr<ScreenshareLayers> layers_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(new ScreenshareLayers(1, 0, &clock_));
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  // One layer screenshare should not use the frame dropper as all frames will
  // belong to the base layer.
  const int kSingleLayerFlags = 0;
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(kSingleLayerFlags, flags);
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(kSingleLayerFlags, flags);
  timestamp += kTimestampDelta5Fps;
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
}

TEST_F(ScreenshareLayerTest, 2Layer) {
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  uint8_t expected_tl0_idx = 0;
  CodecSpecificInfoVP8 vp8_info;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  // Insert 5 frames, cover grace period. All should be in TL0.
  for (int i = 0; i < 5; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    EXPECT_EQ(0, vp8_info.temporalIdx);
    EXPECT_FALSE(vp8_info.layerSync);
    ++expected_tl0_idx;
    EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
  }

  // First frame in TL0.
  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  // Drop two frames from TL0, thus being coded in TL1.
  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  // First frame is sync frame.
  EXPECT_EQ(ScreenshareLayers::kTl1SyncFlags, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_TRUE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl1Flags, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
}

TEST_F(ScreenshareLayerTest, 2LayersPeriodicSync) {
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = kSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync) {
      sync_times.push_back(timestamp);
    }
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterTimeout) {
  ConfigureBitrates();
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = kMaxSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    // Simulate TL1 being at least 8 qp steps better.
    if (vp8_info.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
    }

    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync)
      sync_times.push_back(timestamp);
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kMaxSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterSimilarQP) {
  ConfigureBitrates();
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = (kSyncPeriodSeconds +
                          ((kMaxSyncPeriodSeconds - kSyncPeriodSeconds) / 2)) *
                         kFrameRate;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    // Simulate TL1 being at least 8 qp steps better.
    if (vp8_info.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
    }

    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync)
      sync_times.push_back(timestamp);
  }

  ASSERT_EQ(1u, sync_times.size());

  bool bumped_tl0_quality = false;
  for (int i = 0; i < 3; ++i) {
    timestamp += kTimestampDelta5Fps;
    int flags = layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    if (vp8_info.temporalIdx == 0) {
      // Bump TL0 to same quality as TL1.
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
      bumped_tl0_quality = true;
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
      if (bumped_tl0_quality) {
        EXPECT_TRUE(vp8_info.layerSync);
        EXPECT_EQ(ScreenshareLayers::kTl1SyncFlags, flags);
        return;
      }
    }
  }
  ADD_FAILURE() << "No TL1 frame arrived within time limit.";
}

TEST_F(ScreenshareLayerTest, 2LayersToggling) {
  ConfigureBitrates();
  int flags = 0;
  CodecSpecificInfoVP8 vp8_info;
  uint32_t timestamp = RunGracePeriod();

  // Insert 50 frames. 2/5 should be TL0.
  int tl0_frames = 0;
  int tl1_frames = 0;
  for (int i = 0; i < 50; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    switch (vp8_info.temporalIdx) {
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
  ConfigureBitrates();
  frame_size_ = ((kDefaultTl0BitrateKbps * 1000) / 8) / kFrameRate;

  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  // Insert 50 frames, small enough that all fits in TL0.
  for (int i = 0; i < 50; ++i) {
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    timestamp += kTimestampDelta5Fps;
    EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
    EXPECT_EQ(0, vp8_info.temporalIdx);
  }
}

TEST_F(ScreenshareLayerTest, TooHighBitrate) {
  ConfigureBitrates();
  frame_size_ = 2 * ((kDefaultTl1BitrateKbps * 1000) / 8) / kFrameRate;
  int flags = 0;
  CodecSpecificInfoVP8 vp8_info;
  uint32_t timestamp = RunGracePeriod();

  // Insert 100 frames. Half should be dropped.
  int tl0_frames = 0;
  int tl1_frames = 0;
  int dropped_frames = 0;
  for (int i = 0; i < 100; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    if (flags == -1) {
      ++dropped_frames;
    } else {
      switch (vp8_info.temporalIdx) {
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
  }

  EXPECT_EQ(50, tl0_frames + tl1_frames);
  EXPECT_EQ(50, dropped_frames);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL0) {
  vpx_codec_enc_cfg_t cfg = GetConfig();
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 1000;
  layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5);

  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps, kTl1_kbps - kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * kTl0_kbps + 0.5),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  vpx_codec_enc_cfg_t cfg = GetConfig();
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 450;
  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps, kTl1_kbps - kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));

  EXPECT_EQ(static_cast<unsigned int>(
                kTl1_kbps / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  vpx_codec_enc_cfg_t cfg = GetConfig();
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 100;
  EXPECT_THAT(layers_->OnRatesUpdated(kTl0_kbps, kTl1_kbps, 5),
              ElementsAre(kTl0_kbps));
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));

  EXPECT_EQ(static_cast<uint32_t>(kTl1_kbps), cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, EncoderDrop) {
  ConfigureBitrates();
  CodecSpecificInfoVP8 vp8_info;
  vpx_codec_enc_cfg_t cfg = GetConfig();
  // Updates cfg with current target bitrate.
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));

  uint32_t timestamp = RunGracePeriod();
  timestamp = SkipUntilTl(0, timestamp);

  // Size 0 indicates dropped frame.
  layers_->FrameEncoded(0, timestamp, kDefaultQp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg));
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, layers_->EncodeFlags(timestamp));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  timestamp = SkipUntilTl(0, timestamp);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  EXPECT_LT(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  layers_->EncodeFlags(timestamp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  // Next drop in TL1.

  timestamp = SkipUntilTl(1, timestamp);
  layers_->FrameEncoded(0, timestamp, kDefaultQp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg));
  EXPECT_EQ(ScreenshareLayers::kTl1Flags, layers_->EncodeFlags(timestamp));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  timestamp = SkipUntilTl(1, timestamp);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  EXPECT_LT(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  layers_->EncodeFlags(timestamp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
}

TEST_F(ScreenshareLayerTest, RespectsMaxIntervalBetweenFrames) {
  const int kLowBitrateKbps = 50;
  const int kLargeFrameSizeBytes = 100000;
  const uint32_t kStartTimestamp = 1234;

  vpx_codec_enc_cfg_t cfg = GetConfig();
  layers_->OnRatesUpdated(kLowBitrateKbps, kLowBitrateKbps, 5);
  layers_->UpdateConfiguration(&cfg);

  EXPECT_EQ(ScreenshareLayers::kTl0Flags,
            layers_->EncodeFlags(kStartTimestamp));
  layers_->FrameEncoded(kLargeFrameSizeBytes, kStartTimestamp, kDefaultQp);

  const uint32_t kTwoSecondsLater =
      kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90);

  // Sanity check, repayment time should exceed kMaxFrameIntervalMs.
  ASSERT_GT(kStartTimestamp + 90 * (kLargeFrameSizeBytes * 8) / kLowBitrateKbps,
            kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90));

  EXPECT_EQ(-1, layers_->EncodeFlags(kTwoSecondsLater));
  // More than two seconds has passed since last frame, one should be emitted
  // even if bitrate target is then exceeded.
  EXPECT_EQ(ScreenshareLayers::kTl0Flags,
            layers_->EncodeFlags(kTwoSecondsLater + 90));
}

TEST_F(ScreenshareLayerTest, UpdatesHistograms) {
  metrics::Reset();
  ConfigureBitrates();
  vpx_codec_enc_cfg_t cfg = GetConfig();
  bool trigger_drop = false;
  bool dropped_frame = false;
  bool overshoot = false;
  const int kTl0Qp = 35;
  const int kTl1Qp = 30;
  for (int64_t timestamp = 0;
       timestamp < kTimestampDelta5Fps * 5 * metrics::kMinRunTimeInSeconds;
       timestamp += kTimestampDelta5Fps) {
    int flags = layers_->EncodeFlags(timestamp);
    if (flags != -1)
      layers_->UpdateConfiguration(&cfg);

    if (timestamp >= kTimestampDelta5Fps * 5 && !overshoot && flags != -1) {
      // Simulate one overshoot.
      layers_->FrameEncoded(0, timestamp, 0);
      overshoot = true;
      flags = layers_->EncodeFlags(timestamp);
    }

    if (flags == ScreenshareLayers::kTl0Flags) {
      if (timestamp >= kTimestampDelta5Fps * 20 && !trigger_drop) {
        // Simulate a too large frame, to cause frame drop.
        layers_->FrameEncoded(frame_size_ * 5, timestamp, kTl0Qp);
        trigger_drop = true;
      } else {
        layers_->FrameEncoded(frame_size_, timestamp, kTl0Qp);
      }
    } else if (flags == ScreenshareLayers::kTl1Flags ||
               flags == ScreenshareLayers::kTl1SyncFlags) {
      layers_->FrameEncoded(frame_size_, timestamp, kTl1Qp);
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
  vpx_codec_enc_cfg_t cfg = GetConfig();
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg));
}

TEST_F(ScreenshareLayerTest, RespectsConfiguredFramerate) {
  ConfigureBitrates();

  int64_t kTestSpanMs = 2000;
  int64_t kFrameIntervalsMs = 1000 / kFrameRate;

  uint32_t timestamp = 1234;
  int num_input_frames = 0;
  int num_discarded_frames = 0;

  // Send at regular rate - no drops expected.
  for (int64_t i = 0; i < kTestSpanMs; i += kFrameIntervalsMs) {
    if (layers_->EncodeFlags(timestamp) == -1) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->FrameEncoded(frame_size_bytes, timestamp, kDefaultQp);
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
    if (layers_->EncodeFlags(timestamp) == -1) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->FrameEncoded(frame_size_bytes, timestamp, kDefaultQp);
    }
    timestamp += kFrameIntervalsMs * 90 / 2;
    clock_.AdvanceTimeMilliseconds(kFrameIntervalsMs / 2);
    ++num_input_frames;
  }

  // Allow for some rounding errors in the measurements.
  EXPECT_NEAR(num_discarded_frames, num_input_frames / 2, 2);
}
}  // namespace webrtc
