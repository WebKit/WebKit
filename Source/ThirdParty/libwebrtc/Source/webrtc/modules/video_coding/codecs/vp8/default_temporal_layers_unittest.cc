/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/vp8_impl.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {
namespace test {

enum {
  kTemporalUpdateLast = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF,
  kTemporalUpdateGoldenWithoutDependency =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGolden =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltrefWithoutDependency =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltref = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateNone = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefAltRef =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefGolden =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefGoldenAltRef =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_ARF |
      VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateGoldenWithoutDependencyRefAltRef =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGoldenRefAltRef = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateLastRefAltRef =
      VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
  kTemporalUpdateLastAndGoldenRefAltRef =
      VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
};

TEST(TemporalLayersTest, 2Layers) {
  DefaultTemporalLayers tl(2, 0);
  DefaultTemporalLayersChecker checker(2, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[16] = {
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 1, 0, 1, 0, 1, 0, 1,
                                   0, 1, 0, 1, 0, 1, 0, 1};

  bool expected_layer_sync[16] = {false, true,  false, false, false, false,
                                  false, false, false, true,  false, false,
                                  false, false, false, false};

  uint32_t timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config)) << i;
    tl.PopulateCodecSpecific(i == 0, tl_config, &vp8_info, 0);
    EXPECT_TRUE(checker.CheckTemporalConfig(i == 0, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(i == 0 || expected_layer_sync[i], vp8_info.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, 3Layers) {
  DefaultTemporalLayers tl(3, 0);
  DefaultTemporalLayersChecker checker(3, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[16] = {
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 2, 1, 2, 0, 2, 1, 2,
                                   0, 2, 1, 2, 0, 2, 1, 2};

  bool expected_layer_sync[16] = {false, true,  true,  false, false, false,
                                  false, false, false, true,  true,  false,
                                  false, false, false, false};

  unsigned int timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config)) << i;
    tl.PopulateCodecSpecific(i == 0, tl_config, &vp8_info, 0);
    EXPECT_TRUE(checker.CheckTemporalConfig(i == 0, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(i == 0 || expected_layer_sync[i], vp8_info.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, Alternative3Layers) {
  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(3, 0);
  DefaultTemporalLayersChecker checker(3, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[8] = {kTemporalUpdateLast,
                           kTemporalUpdateAltrefWithoutDependency,
                           kTemporalUpdateGoldenWithoutDependency,
                           kTemporalUpdateNone,
                           kTemporalUpdateLast,
                           kTemporalUpdateAltrefWithoutDependency,
                           kTemporalUpdateGoldenWithoutDependency,
                           kTemporalUpdateNone};
  int expected_temporal_idx[8] = {0, 2, 1, 2, 0, 2, 1, 2};

  bool expected_layer_sync[8] = {false, true, true, false,
                                 false, true, true, false};

  unsigned int timestamp = 0;
  for (int i = 0; i < 8; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config)) << i;
    tl.PopulateCodecSpecific(i == 0, tl_config, &vp8_info, 0);
    EXPECT_TRUE(checker.CheckTemporalConfig(i == 0, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(i == 0 || expected_layer_sync[i], vp8_info.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, 4Layers) {
  DefaultTemporalLayers tl(4, 0);
  DefaultTemporalLayersChecker checker(4, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);
  int expected_flags[16] = {
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefGoldenAltRef,
      kTemporalUpdateAltrefWithoutDependency,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateLast,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateGolden,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] = {0, 3, 2, 3, 1, 3, 2, 3,
                                   0, 3, 2, 3, 1, 3, 2, 3};

  bool expected_layer_sync[16] = {false, true,  true,  false, true,  false,
                                  false, false, false, false, false, false,
                                  false, false, false, false};

  uint32_t timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config)) << i;
    tl.PopulateCodecSpecific(i == 0, tl_config, &vp8_info, 0);
    EXPECT_TRUE(checker.CheckTemporalConfig(i == 0, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(i == 0 || expected_layer_sync[i], vp8_info.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST(TemporalLayersTest, KeyFrame) {
  DefaultTemporalLayers tl(3, 0);
  DefaultTemporalLayersChecker checker(3, 0);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  int expected_flags[8] = {
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNoneNoRefGolden,
      kTemporalUpdateGoldenWithoutDependencyRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenRefAltRef,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[8] = {0, 2, 1, 2, 0, 2, 1, 2};
  bool expected_layer_sync[8] = {false, true,  true,  false,
                                 false, false, false, false};

  uint32_t timestamp = 0;
  for (int i = 0; i < 7; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
    EXPECT_EQ(expected_flags[i], VP8EncoderImpl::EncodeFlags(tl_config)) << i;
    tl.PopulateCodecSpecific(true, tl_config, &vp8_info, 0);
    EXPECT_TRUE(checker.CheckTemporalConfig(true, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(0, vp8_info.temporalIdx)
        << "Key frame should always be packetized as layer 0";
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    EXPECT_TRUE(vp8_info.layerSync) << "Key frame should be marked layer sync.";
    timestamp += 3000;
  }
  TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp);
  EXPECT_EQ(expected_flags[7], VP8EncoderImpl::EncodeFlags(tl_config));
  tl.PopulateCodecSpecific(false, tl_config, &vp8_info, 0);
  EXPECT_TRUE(checker.CheckTemporalConfig(false, tl_config));
  EXPECT_NE(0, vp8_info.temporalIdx)
      << "To test something useful, this frame should not use layer 0.";
  EXPECT_EQ(expected_temporal_idx[7], vp8_info.temporalIdx)
      << "Non-keyframe, should use frame temporal index.";
  EXPECT_EQ(expected_temporal_idx[7], tl_config.packetizer_temporal_idx);
  EXPECT_EQ(expected_temporal_idx[7], tl_config.encoder_layer_id);
  EXPECT_FALSE(tl_config.layer_sync);
  EXPECT_TRUE(vp8_info.layerSync) << "Frame after keyframe should always be "
                                     "marked layer sync since it only depends "
                                     "on the base layer.";
}

class TemporalLayersReferenceTest : public ::testing::TestWithParam<int> {
 public:
  TemporalLayersReferenceTest()
      : timestamp_(1),
        last_sync_timestamp_(timestamp_),
        tl0_reference_(nullptr) {}
  virtual ~TemporalLayersReferenceTest() {}

 protected:
  static const int kMaxPatternLength = 32;

  struct BufferState {
    BufferState() : BufferState(-1, 0, false) {}
    BufferState(int temporal_idx, uint32_t timestamp, bool sync)
        : temporal_idx(temporal_idx), timestamp(timestamp), sync(sync) {}
    int temporal_idx;
    uint32_t timestamp;
    bool sync;
  };

  bool UpdateSyncRefState(const TemporalLayers::BufferFlags& flags,
                          BufferState* buffer_state) {
    if (flags & TemporalLayers::kReference) {
      if (buffer_state->temporal_idx == -1)
        return true;  // References key-frame.
      if (buffer_state->temporal_idx == 0) {
        // No more than one reference to TL0 frame.
        EXPECT_EQ(nullptr, tl0_reference_);
        tl0_reference_ = buffer_state;
        return true;
      }
      return false;  // References higher layer.
    }
    return true;  // No reference, does not affect sync frame status.
  }

  void ValidateReference(const TemporalLayers::BufferFlags& flags,
                         const BufferState& buffer_state,
                         int temporal_layer) {
    if (flags & TemporalLayers::kReference) {
      if (temporal_layer > 0 && buffer_state.timestamp > 0) {
        // Check that high layer reference does not go past last sync frame.
        EXPECT_GE(buffer_state.timestamp, last_sync_timestamp_);
      }
      // No reference to buffer in higher layer.
      EXPECT_LE(buffer_state.temporal_idx, temporal_layer);
    }
  }

  uint32_t timestamp_ = 1;
  uint32_t last_sync_timestamp_ = timestamp_;
  BufferState* tl0_reference_;

  BufferState last_state;
  BufferState golden_state;
  BufferState altref_state;
};

INSTANTIATE_TEST_CASE_P(DefaultTemporalLayersTest,
                        TemporalLayersReferenceTest,
                        ::testing::Range(1, kMaxTemporalStreams + 1));

TEST_P(TemporalLayersReferenceTest, ValidFrameConfigs) {
  const int num_layers = GetParam();
  DefaultTemporalLayers tl(num_layers, 0);
  vpx_codec_enc_cfg_t cfg;
  tl.OnRatesUpdated(500, 500, 30);
  tl.UpdateConfiguration(&cfg);

  // Run through the pattern and store the frame dependencies, plus keep track
  // of the buffer state; which buffers references which temporal layers (if
  // (any). If a given buffer is never updated, it is legal to reference it
  // even for sync frames. In order to be general, don't assume TL0 always
  // updates |last|.
  std::vector<TemporalLayers::FrameConfig> tl_configs(kMaxPatternLength);
  for (int i = 0; i < kMaxPatternLength; ++i) {
    TemporalLayers::FrameConfig tl_config = tl.UpdateLayerConfig(timestamp_++);
    EXPECT_FALSE(tl_config.drop_frame);
    tl_configs.push_back(tl_config);
    int temporal_idx = tl_config.encoder_layer_id;
    // For the default layers, always keep encoder and rtp layers in sync.
    EXPECT_EQ(tl_config.packetizer_temporal_idx, temporal_idx);

    // Determine if this frame is in a higher layer but references only TL0
    // or untouched buffers, if so verify it is marked as a layer sync.
    bool is_sync_frame = true;
    tl0_reference_ = nullptr;
    if (temporal_idx <= 0) {
      is_sync_frame = false;  // TL0 by definition not a sync frame.
    } else if (!UpdateSyncRefState(tl_config.last_buffer_flags, &last_state)) {
      is_sync_frame = false;
    } else if (!UpdateSyncRefState(tl_config.golden_buffer_flags,
                                   &golden_state)) {
      is_sync_frame = false;
    } else if (!UpdateSyncRefState(tl_config.arf_buffer_flags, &altref_state)) {
      is_sync_frame = false;
    }
    if (is_sync_frame) {
      // Cache timestamp for last found sync frame, so that we can verify no
      // references back past this frame.
      ASSERT_TRUE(tl0_reference_);
      last_sync_timestamp_ = tl0_reference_->timestamp;
    }
    EXPECT_EQ(tl_config.layer_sync, is_sync_frame);

    // Validate no reference from lower to high temporal layer, or backwards
    // past last reference frame.
    ValidateReference(tl_config.last_buffer_flags, last_state, temporal_idx);
    ValidateReference(tl_config.golden_buffer_flags, golden_state,
                      temporal_idx);
    ValidateReference(tl_config.arf_buffer_flags, altref_state, temporal_idx);

    // Update the current layer state.
    BufferState state = {temporal_idx, timestamp_, is_sync_frame};
    if (tl_config.last_buffer_flags & TemporalLayers::kUpdate)
      last_state = state;
    if (tl_config.golden_buffer_flags & TemporalLayers::kUpdate)
      golden_state = state;
    if (tl_config.arf_buffer_flags & TemporalLayers::kUpdate)
      altref_state = state;
  }
}
}  // namespace test
}  // namespace webrtc
